#include "propertyselectorcontroller.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <algorithm>
#include <cmath>

#include "datasetbrowsercontroller.h"
#include "latticefamily.h"

namespace {

const QString kCacheTag = QStringLiteral("# MatViz3D property index v1");

QString fmt(double v, int digits = 3)
{
    return std::isfinite(v) ? QString::number(v, 'g', digits) : QStringLiteral("—");
}

// "+4 %", "-0.3 %": one decimal below 10 %, and no "-0".
QString deviationPct(double pct)
{
    if (std::abs(pct) < 0.05) return QStringLiteral("0 %");
    return std::abs(pct) < 10.0 ? QString::asprintf("%+.1f %%", pct) : QString::asprintf("%+.0f %%", pct);
}

QString familyLabel(const QString& key)
{
    LatticeFamily f;
    return parseLatticeFamily(key, f) ? latticeFamilyLabel(f) : key;
}

QString targetLabel(int t)
{
    static const char* labels[propsearch::TargetCount] = { "E x", "E y", "E z", "G xy", "G yz", "G xz", "ν xy", "ν xz", "ν yz" };
    return QString::fromUtf8(labels[t]);
}

} // namespace

PropertySelectorController::PropertySelectorController(DatasetBrowserController* browser, QObject* parent)
    : QObject(parent), m_browser(browser)
{
    m_batch.setInterval(0);
    connect(&m_batch, &QTimer::timeout, this, [this]() {
        // About a tenth of a second of reading per turn of the event loop.
        QElapsedTimer t;
        t.start();
        while (m_next < m_pending.size() && t.elapsed() < 100)
            indexOne(m_pending[m_next++]);
        emit progressChanged();
        if (m_next >= m_pending.size()) finish();
    });
    if (m_browser)
        connect(m_browser, &DatasetBrowserController::datasetChanged, this, [this]() { reset(); });
}

bool PropertySelectorController::hasDataset() const
{
    return m_browser && m_browser->isOpen();
}

QString PropertySelectorController::datasetName() const
{
    if (!m_browser) return {};
    const QStringList parts = m_browser->folder().split(QRegularExpression(QStringLiteral("[\\\\/]")), Qt::SkipEmptyParts);
    return parts.sliced(std::max<qsizetype>(0, parts.size() - 2)).join(QStringLiteral("/"));
}

void PropertySelectorController::reset()
{
    m_batch.stop();
    m_cells.clear();
    m_own.clear();
    m_pending.clear();
    m_next = 0;
    m_failed = 0;
    m_ready = false;
    m_indexing = false;
    m_families.clear();
    m_matches.clear();
    m_results.clear();
    m_current = -1;
    m_message.clear();
    emit indexChanged();
    emit progressChanged();
    emit resultsChanged();
    emit currentChanged();
}

void PropertySelectorController::prepare()
{
    m_preparing = true;
    reset();
    m_preparing = false;
    if (!hasDataset()) return;
    m_folder = m_browser->folder();
    for (const auto& r : m_browser->allRows()) {
        if (!r.hasFile || (r.status != QLatin1String("done") && r.status != QLatin1String("file"))) continue;
        const QString file = m_browser->cellFile(r);
        if (file.isEmpty()) continue;
        m_pending.push_back({ r.id, r.group, r.family, file, r.cellMm });
    }
    loadCache();
    m_indexing = true;
    emit indexChanged();
    emit progressChanged();
}

bool PropertySelectorController::indexOne(const Pending& p)
{
    const QFileInfo fi(p.file);
    propsearch::Cell cell;
    const auto it = m_cache.constFind(p.id);
    if (it != m_cache.constEnd() && it->mtime == fi.lastModified().toMSecsSinceEpoch() && it->size == fi.size()) {
        cell = it->cell;
    } else {
        cell.family = p.family;
        cell.cellMm = p.cellMm;
        QString why;
        if (!propsearch::readCell(p.file, cell, &why)) {
            ++m_failed;
            qWarning().noquote() << QString("[Property selector] %1: %2").arg(p.file, why);
            return false;
        }
        m_cache.insert(p.id, { fi.lastModified().toMSecsSinceEpoch(), fi.size(), cell });
    }
    cell.id = p.id;
    cell.group = p.group;
    if (!p.family.isEmpty()) cell.family = p.family;
    cell.file = p.file;
    m_cells.push_back(std::move(cell));
    return true;
}

void PropertySelectorController::finish()
{
    m_batch.stop();
    m_indexing = false;
    m_ready = true;
    m_own.clear();
    m_own.reserve(m_cells.size());
    for (const auto& c : m_cells) m_own.push_back(propsearch::moduli(c.C));
    QStringList keys;
    for (const auto& c : m_cells)
        if (!keys.contains(c.family)) keys << c.family;
    std::sort(keys.begin(), keys.end(), [](const QString& a, const QString& b) { return familyLabel(a) < familyLabel(b); });
    m_families.clear();
    m_families << QVariantMap{ { QStringLiteral("key"), QString() }, { QStringLiteral("label"), QStringLiteral("All families") } };
    for (const QString& k : keys)
        m_families << QVariantMap{ { QStringLiteral("key"), k }, { QStringLiteral("label"), familyLabel(k) } };
    saveCache();
    m_message = m_failed > 0 ? QString("%1 cells could not be read (see the log).").arg(m_failed) : QString();
    qInfo().noquote() << QString("[Property selector] %1: %2 accepted cells indexed, %3 unreadable")
                             .arg(m_folder).arg(m_cells.size()).arg(m_failed);
    emit progressChanged();
    emit indexChanged();
    emit resultsChanged();
}

void PropertySelectorController::buildIndex()
{
    // Built already for this dataset (a new or reloaded one resets it): a call
    // queued while the index was being read must not start it over.
    if (m_indexing || m_preparing || m_ready) return;
    prepare();
    if (!m_indexing) return;
    m_batch.start();
}

void PropertySelectorController::buildIndexNow()
{
    prepare();
    if (!m_indexing) return;
    while (m_next < m_pending.size()) indexOne(m_pending[m_next++]);
    finish();
}

QString PropertySelectorController::cachePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (dir.isEmpty() || m_folder.isEmpty()) return {};
    const QByteArray h = QCryptographicHash::hash(QDir::cleanPath(m_folder).toUtf8(), QCryptographicHash::Sha1).toHex();
    return QDir(dir).filePath(QStringLiteral("property_index/") + QString::fromLatin1(h.left(16)) + QStringLiteral(".csv"));
}

void PropertySelectorController::loadCache()
{
    m_cache.clear();
    QFile f(cachePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    if (in.readLine() != kCacheTag) return;
    in.readLine();   // header
    while (!in.atEnd()) {
        const QStringList v = in.readLine().split(QLatin1Char(','));
        if (v.size() != 11 + 36) continue;
        Cached c;
        c.mtime = v[1].toLongLong();
        c.size = v[2].toLongLong();
        c.cell.family = v[3];
        c.cell.density = v[4].toDouble();
        c.cell.cellMm = v[5].toDouble();
        c.cell.wallMm = v[6].toDouble();
        c.cell.law = { v[7].toDouble(), v[8].toDouble(), v[9].toDouble() };
        // v[10] is the dataset-relative file name, kept for whoever reads the cache by hand.
        for (int k = 0; k < 36; ++k) c.cell.C[k / 6][k % 6] = v[11 + k].toDouble();
        m_cache.insert(v[0], c);
    }
}

void PropertySelectorController::saveCache() const
{
    const QString path = cachePath();
    if (path.isEmpty() || m_cells.empty()) return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    out << kCacheTag << "\n";
    out << "id,mtime,size,family,density,cell_mm,wall_mm,e_gpa,nu,aniso,file";
    for (int k = 0; k < 36; ++k) out << ",C" << k / 6 << k % 6;
    out << "\n";
    const QDir root(m_folder);
    for (const auto& c : m_cells) {
        const auto it = m_cache.constFind(c.id);
        if (it == m_cache.constEnd()) continue;
        out << c.id << ',' << it->mtime << ',' << it->size << ',' << c.family << ','
            << QString::number(c.density, 'g', 17) << ',' << QString::number(c.cellMm, 'g', 17) << ','
            << QString::number(c.wallMm, 'g', 17) << ',' << QString::number(c.law.eGpa, 'g', 17) << ','
            << QString::number(c.law.nu, 'g', 17) << ',' << QString::number(c.law.aniso, 'g', 17) << ','
            << root.relativeFilePath(c.file);
        for (int k = 0; k < 36; ++k) out << ',' << QString::number(c.C[k / 6][k % 6], 'g', 17);
        out << "\n";
    }
    f.commit();
}

void PropertySelectorController::search(const QVariantMap& query)
{
    m_matches.clear();
    m_results.clear();
    m_current = -1;
    emit currentChanged();
    if (!m_ready) {
        m_message = hasDataset() ? QStringLiteral("The dataset is still being indexed.")
                                 : QStringLiteral("Open a dataset on the Dataset tab first.");
        emit resultsChanged();
        return;
    }

    propsearch::Query q;
    const QVariantMap mat = query.value(QStringLiteral("material")).toMap();
    q.material.eGpa  = mat.value(QStringLiteral("e_gpa"), 2.0).toDouble();
    q.material.nu    = mat.value(QStringLiteral("nu"), 0.38).toDouble();
    q.material.aniso = mat.value(QStringLiteral("aniso"), 1.0).toDouble();
    if (!(q.material.eGpa > 0.0) || !(q.material.nu > -1.0 && q.material.nu < 0.5) || !(q.material.aniso > 0.0)) {
        m_message = QStringLiteral("The material needs E > 0, −1 < ν < 0.5 and a positive factor across the layers.");
        emit resultsChanged();
        return;
    }
    const QVariantMap targets = query.value(QStringLiteral("targets")).toMap();
    int used = 0;
    for (int t = 0; t < propsearch::TargetCount; ++t) {
        const QString key = propsearch::targetKey(t);
        if (!targets.contains(key)) continue;
        bool ok = false;
        const double v = targets.value(key).toDouble(&ok);
        if (!ok || !std::isfinite(v) || (propsearch::isModulus(t) && !(v > 0.0))) continue;
        q.use[static_cast<size_t>(t)] = true;
        q.value[static_cast<size_t>(t)] = propsearch::isModulus(t) ? v * 1e6 : v;
        ++used;
    }
    if (used == 0) {
        m_message = QStringLiteral("Tick at least one target property and give it a value.");
        emit resultsChanged();
        return;
    }
    q.rhoMin = query.value(QStringLiteral("rho_min"), 0.0).toDouble();
    q.rhoMax = query.value(QStringLiteral("rho_max"), 1.0).toDouble();
    q.family = query.value(QStringLiteral("family")).toString();
    q.count  = std::clamp(query.value(QStringLiteral("count"), 10).toInt(), 1, 100);

    m_matches = propsearch::search(m_cells, q, [](const propsearch::Cell& c, std::vector<double>& T) {
        return propsearch::readSensitivity(c.file, T);
    });

    // A Young's modulus above the Hashin-Shtrikman bound cannot be reached by any
    // isotropic cell of this material at these densities.
    QStringList beyond;
    const double Es = q.material.eGpa * 1e9;
    const double bound = propsearch::hsUpperE(Es, q.material.nu, 1.0 - std::min(q.rhoMax, 0.999));
    for (int t = propsearch::Ex; t <= propsearch::Ez; ++t)
        if (q.use[static_cast<size_t>(t)] && q.value[static_cast<size_t>(t)] > bound)
            beyond << targetLabel(t);

    for (int i = 0; i < static_cast<int>(m_matches.size()); ++i) {
        const auto& m = m_matches[static_cast<size_t>(i)];
        const auto& c = m_cells[static_cast<size_t>(m.cell)];
        QVariantList rows;
        for (int t = 0; t < propsearch::TargetCount; ++t) {
            if (!q.use[static_cast<size_t>(t)]) continue;
            const double y = q.value[static_cast<size_t>(t)], p = m.predicted[static_cast<size_t>(t)];
            const bool mod = propsearch::isModulus(t);
            rows << QVariantMap{
                { QStringLiteral("label"), targetLabel(t) },
                { QStringLiteral("target"), mod ? fmt(y / 1e6) : fmt(y, 3) },
                { QStringLiteral("value"), mod ? fmt(p / 1e6) : fmt(p, 3) },
                { QStringLiteral("deviation"), mod ? deviationPct(100.0 * (p / y - 1.0))
                                                   : QString::asprintf("%+.2f", p - y) },
                { QStringLiteral("good"), mod ? std::abs(std::log(p / y)) < 0.1 : std::abs(p - y) < 0.05 },
            };
        }
        const auto& v = m.predicted;
        const QString summary = QString("E %1 / %2 / %3 · G %4 / %5 / %6 MPa · ν %7")
                                    .arg(fmt(v[0] / 1e6), fmt(v[1] / 1e6), fmt(v[2] / 1e6), fmt(v[3] / 1e6),
                                         fmt(v[4] / 1e6), fmt(v[5] / 1e6), fmt(v[propsearch::NuXy], 2));
        const QString law = QString("solved with E %1 GPa · ν %2%3")
                                .arg(fmt(c.law.eGpa), fmt(c.law.nu, 2),
                                     std::abs(c.law.aniso - 1.0) < 1e-9 ? QString()
                                                                        : QString(" · %1 × E across the layers").arg(fmt(c.law.aniso, 2)));
        m_results << QVariantMap{
            { QStringLiteral("rank"), i + 1 },
            { QStringLiteral("id"), c.id },
            { QStringLiteral("title"), familyLabel(c.family) + (c.group.isEmpty() || c.group == c.family ? QString() : QStringLiteral(" · ") + c.group) },
            { QStringLiteral("density"), c.density },
            { QStringLiteral("cellMm"), c.cellMm },
            { QStringLiteral("wallMm"), c.wallMm },
            { QStringLiteral("fitPct"), 100.0 * (std::exp(m.fit) - 1.0) },
            { QStringLiteral("uncertaintyPct"), 100.0 * m.uncertainty },
            { QStringLiteral("corrected"), m.corrected },
            { QStringLiteral("law"), law },
            { QStringLiteral("summary"), summary },
            { QStringLiteral("rows"), rows },
        };
    }

    int inLimits = 0;
    for (const auto& c : m_cells)
        if (c.density >= q.rhoMin && c.density <= q.rhoMax && (q.family.isEmpty() || c.family == q.family)) ++inLimits;
    if (m_matches.empty()) {
        m_message = inLimits == 0 ? QStringLiteral("No cell within the density and family limits.")
                                  : QStringLiteral("No cell gives finite values for these targets.");
    } else {
        m_message = QString("%1 of %2 cells within the limits; best fit %3 % off.")
                        .arg(inLimits).arg(m_cells.size()).arg(fmt(100.0 * (std::exp(m_matches.front().fit) - 1.0), 2));
    }
    if (!beyond.isEmpty())
        m_message += QString(" %1 above the Hashin–Shtrikman bound at ρ ≤ %2 (%3 MPa): no isotropic cell of this material reaches it.")
                         .arg(beyond.join(QStringLiteral(", "))).arg(fmt(q.rhoMax, 2)).arg(fmt(bound / 1e6));
    emit resultsChanged();
    if (!m_matches.empty()) show(0);
}

void PropertySelectorController::show(int i)
{
    if (i < 0 || i >= static_cast<int>(m_matches.size()) || !m_browser) return;
    m_current = i;
    m_browser->selectId(m_cells[static_cast<size_t>(m_matches[static_cast<size_t>(i)].cell)].id);
    emit currentChanged();
}

QVariantMap PropertySelectorController::ranges(double eLowGpa, double eHighGpa) const
{
    // Moduli divided by the cell's own E: the material E scales them exactly.
    std::vector<double> E, G, nu, rho;
    for (size_t i = 0; i < m_cells.size() && i < m_own.size(); ++i) {
        const auto& v = m_own[i];
        const double per = 1.0 / (m_cells[i].law.eGpa * 1e6);   // MPa per GPa of the material
        for (int t = propsearch::Ex; t <= propsearch::Ez; ++t) if (v[t] > 0.0) E.push_back(v[t] * per);
        for (int t = propsearch::Gxy; t <= propsearch::Gxz; ++t) if (v[t] > 0.0) G.push_back(v[t] * per);
        for (int t = propsearch::NuXy; t <= propsearch::NuYz; ++t) if (std::isfinite(v[t])) nu.push_back(v[t]);
        rho.push_back(m_cells[i].density);
    }
    auto at = [](std::vector<double>& v, double q) {
        const size_t k = std::min(v.size() - 1, static_cast<size_t>(q * static_cast<double>(v.size() - 1)));
        std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
        return v[k];
    };
    auto span = [&](std::vector<double>& v, double sLo, double sHi, double lo, double hi) {
        if (v.empty()) return QVariantList{ lo, hi };
        const double a = at(v, 0.005) * sLo, b = at(v, 0.995) * sHi;
        return a < b ? QVariantList{ a, b } : QVariantList{ lo, hi };
    };
    return { { QStringLiteral("E"), span(E, eLowGpa, eHighGpa, 1.0, 1000.0) },
             { QStringLiteral("G"), span(G, eLowGpa, eHighGpa, 0.5, 400.0) },
             { QStringLiteral("nu"), span(nu, 1.0, 1.0, -0.3, 0.5) },
             { QStringLiteral("rho"), span(rho, 1.0, 1.0, 0.0, 1.0) } };
}

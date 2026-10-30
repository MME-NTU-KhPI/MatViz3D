#include "datasetbrowsercontroller.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <algorithm>
#include <cmath>

#include "hdf5wrapper.h"
#include "latticefamily.h"

namespace {

// One CSV line into fields (double quotes protect commas, "" is a quote).
QStringList splitCsv(const QString& line)
{
    QStringList out;
    QString cur;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (quoted) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < line.size() && line[i + 1] == QLatin1Char('"')) { cur += c; ++i; }
                else quoted = false;
            } else {
                cur += c;
            }
        } else if (c == QLatin1Char('"')) {
            quoted = true;
        } else if (c == QLatin1Char(',')) {
            out << cur;
            cur.clear();
        } else {
            cur += c;
        }
    }
    out << cur;
    return out;
}

QString num(double v, int digits = 4)
{
    if (!std::isfinite(v)) return QStringLiteral("—");
    return QString::number(v, 'g', digits);
}

QString pct(double v) { return std::isfinite(v) ? QString::number(100.0 * v, 'f', 2) + QStringLiteral(" %") : QStringLiteral("—"); }

QVariantMap item(const QString& key, const QString& value)
{
    QVariantMap m;
    m[QStringLiteral("key")] = key;
    m[QStringLiteral("value")] = value;
    return m;
}

QVariantMap section(const QString& title, const QVariantList& items)
{
    QVariantMap m;
    m[QStringLiteral("title")] = title;
    m[QStringLiteral("items")] = items;
    return m;
}

const QString kAllGroups = QStringLiteral("All groups");

// Gauss-Jordan inverse of a 6x6 matrix; false when singular.
bool invert6(const double A[6][6], double B[6][6])
{
    double M[6][12];
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 12; ++j) M[i][j] = j < 6 ? A[i][j] : (j - 6 == i ? 1.0 : 0.0);
    for (int c = 0; c < 6; ++c) {
        int piv = c;
        for (int r = c + 1; r < 6; ++r) if (std::abs(M[r][c]) > std::abs(M[piv][c])) piv = r;
        if (std::abs(M[piv][c]) < 1e-300) return false;
        if (piv != c) for (int j = 0; j < 12; ++j) std::swap(M[c][j], M[piv][j]);
        const double d = M[c][c];
        for (int j = 0; j < 12; ++j) M[c][j] /= d;
        for (int r = 0; r < 6; ++r) {
            if (r == c) continue;
            const double f = M[r][c];
            if (f != 0.0) for (int j = 0; j < 12; ++j) M[r][j] -= f * M[c][j];
        }
    }
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) B[i][j] = M[i][j + 6];
    return true;
}

// Hashin-Shtrikman upper bound on the Young's modulus of an isotropic porous solid
// (the same formula as hs_upper_E in tools/dataset/make_dataset.py).
double hsUpperE(double Es, double nus, double porosity)
{
    if (!(porosity >= 0.0 && porosity < 1.0) || Es <= 0.0) return NAN;
    const double K = Es / (3.0 * (1.0 - 2.0 * nus)), G = Es / (2.0 * (1.0 + nus)), f = porosity;
    const double Khs = K + f / (-1.0 / K + 3.0 * (1.0 - f) / (3.0 * K + 4.0 * G));
    const double Ghs = G + f / (-1.0 / G + 6.0 * (1.0 - f) * (K + 2.0 * G) / (5.0 * G * (3.0 * K + 4.0 * G)));
    return 9.0 * Khs * Ghs / (3.0 * Khs + Ghs);
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Rows model
// ─────────────────────────────────────────────────────────────────────────────

int DatasetRowsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_view.size());
}

QVariant DatasetRowsModel::data(const QModelIndex& index, int role) const
{
    const Row* r = at(index.row());
    if (!r) return {};
    switch (role) {
        case IdRole:      return r->id;
        case GroupRole:   return r->group;
        case FamilyRole:  return r->family;
        case StatusRole:  return r->status;
        case DensityRole: return r->density;
        case CellMmRole:  return r->cellMm;
        case GridRole:    return r->grid;
        case HasFileRole: return r->hasFile;
        default:          return {};
    }
}

QHash<int, QByteArray> DatasetRowsModel::roleNames() const
{
    return { { IdRole, "rowId" }, { GroupRole, "group" }, { FamilyRole, "family" }, { StatusRole, "status" },
             { DensityRole, "density" }, { CellMmRole, "cellMm" }, { GridRole, "grid" }, { HasFileRole, "hasFile" } };
}

void DatasetRowsModel::setView(std::vector<const Row*> view)
{
    beginResetModel();
    m_view = std::move(view);
    endResetModel();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Controller
// ─────────────────────────────────────────────────────────────────────────────

DatasetBrowserController::DatasetBrowserController(QObject* parent)
    : QObject(parent)
{
    m_groups << kAllGroups;
}

QStringList DatasetBrowserController::statuses() const
{
    return { QStringLiteral("With cell file"), QStringLiteral("Accepted (done)"),
             QStringLiteral("Rejected"), QStringLiteral("Deferred or pending"), QStringLiteral("All rows") };
}

bool DatasetBrowserController::openPath(const QString& pathIn)
{
    QString path = QDir::cleanPath(pathIn);
    if (path.isEmpty()) return false;
    QFileInfo fi(path);
    QString folder;
    QString selectId;
    QString singleFile;

    if (fi.isDir()) {
        folder = fi.absoluteFilePath();
    } else if (fi.fileName() == QLatin1String("manifest.csv")) {
        folder = fi.absolutePath();
    } else if (fi.suffix().compare(QLatin1String("hdf5"), Qt::CaseInsensitive) == 0
               || fi.suffix().compare(QLatin1String("h5"), Qt::CaseInsensitive) == 0) {
        const QDir parent = fi.absoluteDir();
        if (parent.dirName() == QLatin1String("cells") && QFileInfo::exists(parent.absoluteFilePath(QStringLiteral("../manifest.csv")))) {
            folder = QDir::cleanPath(parent.absoluteFilePath(QStringLiteral("..")));
            selectId = fi.completeBaseName();
        } else {
            singleFile = fi.absoluteFilePath();
            folder = fi.absolutePath();
        }
    } else {
        m_message = QStringLiteral("Not a dataset folder, manifest.csv or HDF5 cell file: ") + path;
        emit currentChanged();
        return false;
    }

    m_all.clear();
    m_header.clear();
    m_groups = QStringList{ kAllGroups };
    m_folder = folder;

    // The dataset does not use a named material: every row draws E, nu and the
    // weakening across the layers from the ranges recorded in meta.json.
    m_lawRanges.clear();
    {
        QFile meta(QDir(folder).filePath(QStringLiteral("meta.json")));
        if (meta.open(QIODevice::ReadOnly)) {
            const QJsonObject laws = QJsonDocument::fromJson(meta.readAll()).object().value(QStringLiteral("laws")).toObject();
            auto range = [&](const char* key) {
                const QJsonArray a = laws.value(QLatin1String(key)).toArray();
                return a.size() == 2 ? num(a[0].toDouble(), 3) + QStringLiteral("–") + num(a[1].toDouble(), 3) : QString();
            };
            const QString e = range("E_GPa"), n = range("nu"), r = range("aniso");
            if (!e.isEmpty())
                m_lawRanges = QStringLiteral("E ") + e + QStringLiteral(" GPa, ν ") + n
                              + (r.isEmpty() ? QString() : QStringLiteral(", across the layers ") + r + QStringLiteral(" × E"));
        }
    }

    if (!singleFile.isEmpty()) {
        // A cell outside a dataset: one row, read from the file itself.
        DatasetRowsModel::Row r;
        r.id = fi.completeBaseName();
        r.group = QStringLiteral("file");
        r.status = QStringLiteral("file");
        r.hasFile = true;
        r.file = singleFile;
        HDF5Wrapper h(singleFile.toStdString(), /*readOnly=*/true);
        if (h.datasetExists("/1", "geometry") && h.datasetExists("/1/geometry", "lattice_family_key"))
            r.family = h.readQString("/1/geometry", "lattice_family_key");
        m_all.push_back(r);
        m_groups << r.group;
    } else {
        QFile f(QDir(folder).absoluteFilePath(QStringLiteral("manifest.csv")));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_message = QStringLiteral("No manifest.csv in ") + folder;
            m_model.setView({});
            emit datasetChanged();
            emit filterChanged();
            emit currentChanged();
            return false;
        }
        QTextStream in(&f);
        if (!in.atEnd()) m_header = splitCsv(in.readLine());
        auto col = [&](const char* name) { return m_header.indexOf(QString::fromLatin1(name)); };
        const int cId = col("id"), cGroup = col("group"), cStatus = col("status"), cFam = col("p_lattice_family"),
                  cRho = col("p_relative_density"), cMm = col("p_lattice_cell_mm"), cGrid = col("grid");

        QSet<QString> files;
        const QDir cells(QDir(folder).absoluteFilePath(QStringLiteral("cells")));
        for (const QString& n : cells.entryList({ QStringLiteral("*.hdf5") }, QDir::Files))
            files.insert(QFileInfo(n).completeBaseName());

        m_all.reserve(4096);
        while (!in.atEnd()) {
            const QString line = in.readLine();
            if (line.trimmed().isEmpty()) continue;
            const QStringList v = splitCsv(line);
            auto get = [&](int c) { return (c >= 0 && c < v.size()) ? v[c] : QString(); };
            DatasetRowsModel::Row r;
            r.id = get(cId);
            r.group = get(cGroup);
            r.status = get(cStatus);
            r.family = get(cFam);
            r.density = get(cRho).toDouble();
            r.cellMm = get(cMm).toDouble();
            r.grid = static_cast<int>(get(cGrid).toDouble());
            r.hasFile = files.contains(r.id);
            r.line = line;
            if (!r.group.isEmpty() && !m_groups.contains(r.group)) m_groups << r.group;
            m_all.push_back(std::move(r));
        }
    }

    m_groupFilter = kAllGroups;
    if (!selectId.isEmpty() || !singleFile.isEmpty()) m_statusFilter = QStringLiteral("All rows");
    m_search.clear();
    m_current = -1;
    m_message.clear();
    emit datasetChanged();
    applyFilter();
    if (!singleFile.isEmpty()) {
        select(0);
    } else if (!selectId.isEmpty()) {
        for (int i = 0; i < m_model.size(); ++i)
            if (m_model.at(i)->id == selectId) { select(i); break; }
    } else if (m_model.size() > 0) {
        select(0);
    }
    qInfo().noquote() << QString("[Dataset browser] %1: %2 rows, %3 with a cell file")
                             .arg(folder).arg(m_all.size())
                             .arg(std::count_if(m_all.begin(), m_all.end(), [](const auto& r) { return r.hasFile; }));
    return true;
}

void DatasetBrowserController::reload()
{
    const QString id = m_currentId;
    if (m_folder.isEmpty()) return;
    const QString single = (m_all.size() == 1 && !m_all.front().file.isEmpty()) ? m_all.front().file : QString();
    openPath(single.isEmpty() ? m_folder : single);
    if (!id.isEmpty())
        for (int i = 0; i < m_model.size(); ++i)
            if (m_model.at(i)->id == id) { select(i); break; }
}

void DatasetBrowserController::setGroupFilter(const QString& g)
{
    if (g == m_groupFilter) return;
    m_groupFilter = g;
    applyFilter(m_currentId);
}

void DatasetBrowserController::setStatusFilter(const QString& s)
{
    if (s == m_statusFilter) return;
    m_statusFilter = s;
    applyFilter(m_currentId);
}

void DatasetBrowserController::setSearch(const QString& s)
{
    if (s == m_search) return;
    m_search = s;
    applyFilter(m_currentId);
}

void DatasetBrowserController::applyFilter(const QString& keepId)
{
    std::vector<const DatasetRowsModel::Row*> view;
    view.reserve(m_all.size());
    const QString q = m_search.trimmed();
    for (const auto& r : m_all) {
        if (m_groupFilter != kAllGroups && r.group != m_groupFilter) continue;
        if (m_statusFilter == QLatin1String("With cell file") && !r.hasFile) continue;
        if (m_statusFilter == QLatin1String("Accepted (done)") && r.status != QLatin1String("done")) continue;
        if (m_statusFilter == QLatin1String("Rejected")
            && r.status != QLatin1String("unprintable") && r.status != QLatin1String("no_load_path")
            && r.status != QLatin1String("unresolved") && r.status != QLatin1String("out_of_band")
            && r.status != QLatin1String("failed")) continue;
        if (m_statusFilter == QLatin1String("Deferred or pending")
            && r.status != QLatin1String("deferred") && r.status != QLatin1String("pending") && r.status != QLatin1String("retry")) continue;
        if (!q.isEmpty() && !r.id.contains(q) && !r.family.contains(q, Qt::CaseInsensitive)) continue;
        view.push_back(&r);
    }
    m_model.setView(std::move(view));
    int keep = -1;
    if (!keepId.isEmpty())
        for (int i = 0; i < m_model.size(); ++i)
            if (m_model.at(i)->id == keepId) { keep = i; break; }
    m_current = keep;
    emit filterChanged();
    emit currentChanged();
}

void DatasetBrowserController::next()
{
    if (m_current + 1 < m_model.size()) select(m_current + 1);
}

void DatasetBrowserController::prev()
{
    if (m_current > 0) select(m_current - 1);
}

void DatasetBrowserController::select(int index)
{
    const DatasetRowsModel::Row* r = m_model.at(index);
    if (!r) return;
    m_current = index;
    m_currentId = r->id;
    m_currentFile = !r->file.isEmpty() ? r->file
                                       : QDir(m_folder).absoluteFilePath(QStringLiteral("cells/") + r->id + QStringLiteral(".hdf5"));
    if (!QFileInfo::exists(m_currentFile)) m_currentFile.clear();

    LatticeFamily fam;
    const QString famLabel = parseLatticeFamily(r->family, fam) ? latticeFamilyLabel(fam) : r->family;
    m_currentTitle = QString("%1 · %2 · %3").arg(r->id, r->group, famLabel);

    m_message.clear();
    if (!m_currentFile.isEmpty()) {
        if (m_opener && !m_opener(m_currentFile))
            m_message = QStringLiteral("Could not open ") + m_currentFile;
        else if (r->status == QLatin1String("no_load_path"))
            m_message = QStringLiteral("Rejected after the solve: in some load the solid carries too little of the strain energy, "
                                       "even with the soft void; there is no solid path through the cell in that direction.");
    } else {
        m_message = r->status == QLatin1String("unprintable")
                        ? QStringLiteral("No cell file: the row was rejected by the printability check at the 64³ probe, before any file was written.")
                        : QStringLiteral("No cell file yet: the row has status \"%1\".").arg(r->status);
    }
    buildSections(*r);
    emit currentChanged();
}

void DatasetBrowserController::buildSections(const DatasetRowsModel::Row& row)
{
    m_sections.clear();

    // The manifest row, column -> value.
    QHash<QString, QString> csv;
    if (!row.line.isEmpty()) {
        const QStringList v = splitCsv(row.line);
        for (int i = 0; i < m_header.size() && i < v.size(); ++i) csv.insert(m_header[i], v[i]);
    }

    // The cell file: parameters_json, moduli, solver diagnostics.
    QVariantMap P;
    bool hasC = false;
    double S[6][6] = {};
    double C[6][6] = {};
    int iterations = -1;
    double sens = NAN, shareMin = NAN;
    int cube = 0, waveRows = 0, memberRows = 0;
    if (!m_currentFile.isEmpty()) {
        HDF5Wrapper h(m_currentFile.toStdString(), /*readOnly=*/true);
        int set = h.readInt("/", "last_set");
        if (set <= 0) set = 1;
        const std::string pre = "/" + std::to_string(set);
        const std::string geo = pre + "/geometry";
        if (h.datasetExists(pre, "cubeSize")) cube = h.readInt(pre, "cubeSize");
        if (h.datasetExists(pre, "geometry")) {
            if (h.datasetExists(geo, "parameters_json")) {
                const QJsonDocument doc = QJsonDocument::fromJson(h.readQString(geo, "parameters_json").toUtf8());
                if (doc.isObject()) P = doc.object().value(QStringLiteral("parameters")).toObject().toVariantMap();
            }
            if (h.datasetExists(geo, "sensitivity_index")) sens = h.readDouble(geo, "sensitivity_index");
            if (h.datasetExists(geo, "solid_energy_share_loads")) {
                const auto s = h.readVectorFloat(geo, "solid_energy_share_loads");
                if (!s.empty()) shareMin = *std::min_element(s.begin(), s.end());
            }
            if (h.datasetExists(geo, "lattice_wave_terms")) waveRows = static_cast<int>(h.readVectorVectorFloat(geo, "lattice_wave_terms").size());
            if (h.datasetExists(geo, "lattice_assembly_terms")) memberRows = static_cast<int>(h.readVectorVectorFloat(geo, "lattice_assembly_terms").size());
        }
        auto read6 = [&](const char* name, double M[6][6]) {
            if (!h.datasetExists(pre, name)) return false;
            const auto m = h.readVectorVectorFloat(pre, name);
            if (m.size() != 6) return false;
            for (int i = 0; i < 6; ++i)
                for (int j = 0; j < 6 && j < static_cast<int>(m[static_cast<size_t>(i)].size()); ++j)
                    M[i][j] = m[static_cast<size_t>(i)][static_cast<size_t>(j)];
            return true;
        };
        const bool hasS = read6("S_matrix", S);
        const bool hasCm = read6("C_matrix", C);
        if (hasS && !hasCm) hasC = invert6(S, C);
        else if (hasCm && !hasS) hasC = invert6(C, S);
        else hasC = hasS && hasCm;
        if (h.datasetExists(pre, "iterations_total")) iterations = h.readInt(pre, "iterations_total");
    }

    // A value from the file's parameters, else from the manifest's p_ column.
    auto val = [&](const char* key) -> QString {
        const QString k = QString::fromLatin1(key);
        if (P.contains(k)) {
            const QVariant v = P.value(k);
            if (v.typeId() == QMetaType::Double || v.typeId() == QMetaType::Float) return num(v.toDouble());
            if (v.typeId() == QMetaType::Bool) return v.toBool() ? QStringLiteral("yes") : QStringLiteral("no");
            return v.toString();
        }
        const QString c = csv.value(QStringLiteral("p_") + k);
        return c;
    };
    auto dval = [&](const char* key) -> double {
        const QString k = QString::fromLatin1(key);
        if (P.contains(k)) return P.value(k).toDouble();
        bool ok = false;
        const double d = csv.value(QStringLiteral("p_") + k).toDouble(&ok);
        return ok ? d : NAN;
    };

    LatticeFamily fam = LatticeFamily::GyroidSheet;
    const QString famKey = P.contains(QStringLiteral("lattice_family_key")) ? P.value(QStringLiteral("lattice_family_key")).toString() : row.family;
    const bool famOk = parseLatticeFamily(famKey, fam);

    QVariantMap secRow, secGeom, secLaw, secPrint, secProps, secC;

    // ── Run (the manifest row) ──
    {
        QVariantList it;
        it << item(QStringLiteral("Id"), row.id) << item(QStringLiteral("Group"), row.group)
           << item(QStringLiteral("Status"), row.status);
        if (!csv.value(QStringLiteral("exit_code")).isEmpty()) it << item(QStringLiteral("Exit code"), csv.value(QStringLiteral("exit_code")));
        if (!csv.value(QStringLiteral("seed")).isEmpty()) it << item(QStringLiteral("Seed"), csv.value(QStringLiteral("seed")));
        if (!csv.value(QStringLiteral("grid")).isEmpty()) it << item(QStringLiteral("Solve grid"), csv.value(QStringLiteral("grid")) + QStringLiteral("³"));
        else if (cube > 0) it << item(QStringLiteral("Grid"), QString::number(cube) + QStringLiteral("³"));
        if (!csv.value(QStringLiteral("solve_cells")).isEmpty()) it << item(QStringLiteral("Cells solved"), csv.value(QStringLiteral("solve_cells")));
        if (!csv.value(QStringLiteral("wall_vox")).isEmpty()) it << item(QStringLiteral("Wall at the 64³ probe"), csv.value(QStringLiteral("wall_vox")) + QStringLiteral(" vox"));
        if (!csv.value(QStringLiteral("void_eta")).isEmpty()) it << item(QStringLiteral("Void stiffness"), csv.value(QStringLiteral("void_eta")));
        if (!csv.value(QStringLiteral("seconds")).isEmpty()) it << item(QStringLiteral("Time"), csv.value(QStringLiteral("seconds")) + QStringLiteral(" s"));
        if (!m_currentFile.isEmpty()) it << item(QStringLiteral("File"), QFileInfo(m_currentFile).fileName());
        secRow = section(QStringLiteral("Run"), it);
    }

    // ── Geometry ──
    {
        QVariantList it;
        it << item(QStringLiteral("Family"), famOk ? QString("%1 (%2)").arg(latticeFamilyLabel(fam), latticeFamilyKey(fam)) : famKey);
        it << item(QStringLiteral("Cells per edge"), val("cells_per_edge"));
        it << item(QStringLiteral("Cell size"), val("lattice_cell_mm") + QStringLiteral(" mm"));
        it << item(QStringLiteral("Density, target"), val("relative_density"));
        if (P.contains(QStringLiteral("lattice_achieved_density"))) it << item(QStringLiteral("Density, achieved"), val("lattice_achieved_density"));
        if (P.contains(QStringLiteral("lattice_wall_mm"))) it << item(QStringLiteral("Wall"), val("lattice_wall_mm") + QStringLiteral(" mm  (") + val("lattice_wall_voxels") + QStringLiteral(" vox)"));
        if (P.contains(QStringLiteral("voxel_mm"))) it << item(QStringLiteral("Voxel"), val("voxel_mm") + QStringLiteral(" mm"));
        if (famOk) {
            const bool sheet = fam == LatticeFamily::GyroidSheet || fam == LatticeFamily::SchwarzPSheet || fam == LatticeFamily::SchwarzDSheet
                            || fam == LatticeFamily::IWPSheet || fam == LatticeFamily::FourierSheet;
            if (sheet && dval("lattice_uniform_wall") > 0.5) it << item(QStringLiteral("Uniform wall"), QStringLiteral("yes"));
            if (fam == LatticeFamily::Spinodoid) {
                it << item(QStringLiteral("Cone about X / Y / Z"), val("spinodoid_theta_x") + QStringLiteral(" / ") + val("spinodoid_theta_y") + QStringLiteral(" / ") + val("spinodoid_theta_z") + QStringLiteral(" deg"));
            }
            if (fam == LatticeFamily::FourierSheet || fam == LatticeFamily::FourierSkeletal) {
                it << item(QStringLiteral("Waves (drawn)"), val("fourier_terms"))
                   << item(QStringLiteral("|k| band"), val("fourier_kmin") + QStringLiteral(" … ") + val("fourier_kmax") + QStringLiteral(" per edge"))
                   << item(QStringLiteral("Amplitude decay"), val("fourier_decay"))
                   << item(QStringLiteral("Symmetry"), val("fourier_symmetry"));
            }
            if (waveRows > 0) it << item(QStringLiteral("Wave table"), QString::number(waveRows) + QStringLiteral(" waves"));
            if (fam == LatticeFamily::NoiseSheet || fam == LatticeFamily::NoiseSkeletal)
                it << item(QStringLiteral("Noise smoothing"), val("noise_smooth") + QStringLiteral(" of the edge"));
            if (fam == LatticeFamily::Honeycomb || fam == LatticeFamily::Reentrant) {
                if (dval("honeycomb_custom") > 0.5 || csv.value(QStringLiteral("p_honeycomb_angle")).size() > 0)
                    it << item(QStringLiteral("Wall angle"), val("honeycomb_angle") + QStringLiteral(" deg"))
                       << item(QStringLiteral("Rib / wall"), val("honeycomb_rib"));
                else
                    it << item(QStringLiteral("Shape"), QStringLiteral("family preset"));
            }
            if (isFoamFamily(fam)) {
                const QString mode = val("foam_mode");
                it << item(QStringLiteral("Foam cells"), mode == QLatin1String("edges") ? QStringLiteral("open (edges only)")
                                                       : mode == QLatin1String("windows") ? QStringLiteral("walls with windows")
                                                       : QStringLiteral("closed walls"));
                if (mode == QLatin1String("windows")) it << item(QStringLiteral("Window frame"), val("foam_frame") + QStringLiteral(" bubble sizes"));
                if (P.contains(QStringLiteral("lattice_foam_site_count"))) it << item(QStringLiteral("Bubbles per cell"), val("lattice_foam_site_count"));
            }
            if (fam == LatticeFamily::Mycelium) {
                const QString mode = val("mycelium_mode");
                it << item(QStringLiteral("Mycelium"), mode == QLatin1String("polypore") ? QStringLiteral("polypore (skeletal + binding hyphae)")
                                                     : mode == QLatin1String("mat") ? QStringLiteral("mat (layers + anchors)")
                                                     : mode == QLatin1String("layered") ? QStringLiteral("layered substrate")
                                                     : QStringLiteral("substrate"))
                   << item(QStringLiteral("Hypha length"), val("mycelium_length") + QStringLiteral(" cell edges"))
                   << item(QStringLiteral("Growth unit / branch angle"), val("mycelium_unit") + QStringLiteral(" / ") + val("mycelium_angle") + QStringLiteral(" deg"))
                   << item(QStringLiteral("Fusion distance"), val("mycelium_fuse") + QStringLiteral(" cell edges"))
                   << item(QStringLiteral("Branch / parent thickness"), val("mycelium_taper"));
                if (mode == QLatin1String("polypore")) it << item(QStringLiteral("Binding hyphae"), val("mycelium_binding") + QStringLiteral(" of the length"));
                if (mode == QLatin1String("mat"))      it << item(QStringLiteral("Anchors"), val("mycelium_anchors") + QStringLiteral(" of the branches"));
                if (mode == QLatin1String("layered"))  it << item(QStringLiteral("Substrate at the faces"), val("mycelium_poor"));
                if (P.contains(QStringLiteral("mycelium_fusions")))
                    it << item(QStringLiteral("Hyphae / fusions / free ends"), val("mycelium_hyphae") + QStringLiteral(" / ") + val("mycelium_fusions")
                                                                                 + QStringLiteral(" / ") + val("mycelium_free_ends"))
                       << item(QStringLiteral("Length on closed loops"), QString::number(100.0 * dval("mycelium_backbone_share"), 'f', 0) + QStringLiteral(" %"));
            }
            if (fam == LatticeFamily::CubicAssembly) {
                it << item(QStringLiteral("Struts / plates / shells / windows"),
                           val("assembly_struts") + QStringLiteral(" / ") + val("assembly_plates") + QStringLiteral(" / ")
                           + val("assembly_shells") + QStringLiteral(" / ") + val("assembly_holes"))
                   << item(QStringLiteral("Thickness spread"), val("assembly_spread"));
                if (memberRows > 0) it << item(QStringLiteral("Member table"), QString::number(memberRows)
                                                 + (memberRows == 1 ? QStringLiteral(" member") : QStringLiteral(" members"))
                                                 + QStringLiteral(" (incl. automatic windows)"));
            }
            const QString uni = val("lattice_union");
            LatticeFamily uf;
            const bool plateLike = isPlateFamily(fam) || (parseLatticeFamily(uni, uf) && isPlateFamily(uf));
            if (plateLike && dval("plate_hole") > 0.0) it << item(QStringLiteral("Plate hole"), val("plate_hole") + QStringLiteral(" cell edges"));
            if (!uni.isEmpty() && uni != QLatin1String("none")) it << item(QStringLiteral("United with"), uni);
            const QString sub = val("lattice_subtract");
            if (!sub.isEmpty() && sub != QLatin1String("none")) it << item(QStringLiteral("Carved out"), sub + QStringLiteral(" up to ") + val("lattice_subtract_size"));
        }
        if (dval("lattice_smooth") > 0.0) it << item(QStringLiteral("Smoothing"), val("lattice_smooth") + QStringLiteral(" cell edges"));
        if (dval("lattice_grading") > 0.0) it << item(QStringLiteral("Grading along Z"), val("lattice_grading"));
        if (dval("lattice_jitter") > 0.0) it << item(QStringLiteral("Thickness jitter"), val("lattice_jitter"));
        secGeom = section(QStringLiteral("Geometry"), it);
    }

    // ── Material: the printed solid the cell was solved with ──
    {
        QVariantList it;
        const QString E = P.contains(QStringLiteral("mat_e_gpa")) ? val("mat_e_gpa") : csv.value(QStringLiteral("law_E_GPa"));
        const QString nu = P.contains(QStringLiteral("mat_nu")) ? val("mat_nu") : csv.value(QStringLiteral("law_nu"));
        const QString an = P.contains(QStringLiteral("mat_aniso")) ? val("mat_aniso") : csv.value(QStringLiteral("law_aniso"));
        const QString src = val("mat_source");
        bool okE = false, okR = false;
        const double e = E.toDouble(&okE), r = an.isEmpty() ? 1.0 : an.toDouble(&okR);
        if (src.startsWith(QLatin1String("db:"))) {
            it << item(QStringLiteral("Material"), src.mid(3) + QStringLiteral(" (from the material database)"));
        } else if (!E.isEmpty()) {
            // Dataset rows have no material name: the constants are drawn per row.
            it << item(QStringLiteral("Material"), QStringLiteral("no named material: a printed plastic with constants drawn at random for this cell"));
            if (!m_lawRanges.isEmpty()) it << item(QStringLiteral("Drawn from"), m_lawRanges);
        }
        if (okE && okR && std::abs(r - 1.0) > 1e-6) {
            // Transversely isotropic about the build axis Z: E_z = r E, G_xz = G_yz = r G.
            it << item(QStringLiteral("E along the layers (x, y)"), num(e, 4) + QStringLiteral(" GPa"))
               << item(QStringLiteral("E across the layers (z)"), num(r * e, 4) + QStringLiteral(" GPa  (") + num(r, 3)
                                                                   + QStringLiteral(" × E)"));
        } else if (okE) {
            it << item(QStringLiteral("E (same in all directions)"), num(e, 4) + QStringLiteral(" GPa"));
        }
        if (!nu.isEmpty()) it << item(QStringLiteral("ν"), nu);
        if (!it.isEmpty()) secLaw = section(QStringLiteral("Material"), it);
    }

    // ── Printability ──
    if (P.contains(QStringLiteral("print_verdict"))) {
        QVariantList it;
        it << item(QStringLiteral("Verdict"), val("print_verdict"))
           << item(QStringLiteral("Bodies"), val("print_components"));
        if (P.contains(QStringLiteral("print_connected_directions")))
            it << item(QStringLiteral("Joined in directions"), val("print_connected_directions") + QStringLiteral(" of 3"));
        it
           << item(QStringLiteral("Floating islands"), val("print_islands") + QStringLiteral(" (largest ") + val("print_largest_island_vox") + QStringLiteral(" vox)"))
           << item(QStringLiteral("Thinner than ") + val("print_min_wall_mm") + QStringLiteral(" mm"), pct(dval("print_thin_fraction")))
           << item(QStringLiteral("Unsupported, best direction"), pct(dval("print_unsupported_best")) + QStringLiteral(" along ") + val("print_best_direction"))
           << item(QStringLiteral("Closed pores"), pct(dval("print_closed_pore_fraction")));
        secPrint = section(QStringLiteral("Printability"), it);
    }

    // ── Properties: what the dataset learns (same quantities as dataset.parquet) ──
    if (hasC) {
        QVariantList it;
        // The unit goes into the label, so a row of three values fits on one line.
        auto mpa = [](double pa) { return num(pa / 1e6); };
        const double Ex = 1.0 / S[0][0], Ey = 1.0 / S[1][1], Ez = 1.0 / S[2][2];
        // Pipeline order (xx, yy, zz, xy, yz, xz) with tensor shear: G = 1 / (2 S_ii).
        const double Gxy = 0.5 / S[3][3], Gyz = 0.5 / S[4][4], Gxz = 0.5 / S[5][5];
        it << item(QStringLiteral("E x / y / z, MPa"), mpa(Ex) + QStringLiteral(" / ") + mpa(Ey) + QStringLiteral(" / ") + mpa(Ez))
           << item(QStringLiteral("G xy / yz / xz, MPa"), mpa(Gxy) + QStringLiteral(" / ") + mpa(Gyz) + QStringLiteral(" / ") + mpa(Gxz))
           << item(QStringLiteral("ν xy / xz / yz"), num(-S[1][0] / S[0][0], 3) + QStringLiteral(" / ") + num(-S[2][0] / S[0][0], 3)
                                                       + QStringLiteral(" / ") + num(-S[2][1] / S[1][1], 3));
        // Voigt / Reuss averages and the universal anisotropy index (0 = isotropic),
        // shear entries moved to engineering form.
        const double C44 = C[3][3] / 2, C55 = C[4][4] / 2, C66 = C[5][5] / 2;
        const double S44 = 2 * S[3][3], S55 = 2 * S[4][4], S66 = 2 * S[5][5];
        const double KV = (C[0][0] + C[1][1] + C[2][2] + 2 * (C[0][1] + C[0][2] + C[1][2])) / 9;
        const double GV = (C[0][0] + C[1][1] + C[2][2] - C[0][1] - C[0][2] - C[1][2] + 3 * (C44 + C55 + C66)) / 15;
        const double KR = 1 / (S[0][0] + S[1][1] + S[2][2] + 2 * (S[0][1] + S[0][2] + S[1][2]));
        const double GR = 15 / (4 * (S[0][0] + S[1][1] + S[2][2]) - 4 * (S[0][1] + S[0][2] + S[1][2]) + 3 * (S44 + S55 + S66));
        it << item(QStringLiteral("K Voigt / Reuss, MPa"), mpa(KV) + QStringLiteral(" / ") + mpa(KR));
        const double Eax[3] = { Ex, Ey, Ez };
        const int imax = static_cast<int>(std::max_element(Eax, Eax + 3) - Eax);
        const int imin = static_cast<int>(std::min_element(Eax, Eax + 3) - Eax);
        static const char* axisName[3] = { "x", "y", "z" };
        it << item(QStringLiteral("E max / E min"), num(Eax[imax] / std::max(Eax[imin], 1e-30), 3)
                   + (imax == imin ? QString() : QStringLiteral("  (") + QLatin1String(axisName[imax]) + QStringLiteral(" / ")
                                                   + QLatin1String(axisName[imin]) + QStringLiteral(")")));
        if (GR > 0 && KR > 0) it << item(QStringLiteral("Anisotropy Aᵁ"), num(5 * GV / GR + KV / KR - 6, 3) + QStringLiteral("  (0 = isotropic)"));
        const double Es = dval("mat_e_gpa") * 1e9;
        const double rho = P.contains(QStringLiteral("lattice_achieved_density")) ? P.value(QStringLiteral("lattice_achieved_density")).toDouble() : NAN;
        const double Emean = (Ex + Ey + Ez) / 3.0;
        if (Es > 0.0) {
            it << item(QStringLiteral("Mean E / E solid"), num(Emean / Es, 3));
            const double hs = hsUpperE(Es, dval("mat_nu"), std::isfinite(rho) ? 1.0 - rho : NAN);
            if (std::isfinite(hs) && hs > 0) it << item(QStringLiteral("Mean E / HS bound"), num(Emean / hs, 3));
        }
        if (std::isfinite(sens)) it << item(QStringLiteral("Sensitivity index"), num(sens, 3));
        if (std::isfinite(shareMin)) it << item(QStringLiteral("Solid energy share"), num(shareMin, 4));
        if (iterations >= 0) it << item(QStringLiteral("FFT iterations"), QString::number(iterations));
        secProps = section(QStringLiteral("Properties"), it);

        QVariantList cm;
        static const char* rowName[6] = { "xx", "yy", "zz", "xy", "yz", "xz" };
        for (int i = 0; i < 6; ++i) {
            QStringList v;
            for (int j = 0; j < 6; ++j) v << QString::number(C[i][j] / 1e6, 'f', 1);
            cm << item(QString::fromLatin1(rowName[i]), v.join(QStringLiteral("   ")));
        }
        secC = section(QStringLiteral("Stiffness matrix C, MPa (xx, yy, zz, xy, yz, xz)"), cm);
    }

    // Results first: properties, then what produced them, then the run.
    for (const QVariantMap& sec : { secProps, secLaw, secGeom, secPrint, secC, secRow })
        if (!sec.isEmpty()) m_sections << sec;
}

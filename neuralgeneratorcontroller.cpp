#include "neuralgeneratorcontroller.h"
#include "cellstiffness_gpu.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>
#include <map>

#include "openglwidgetqml.h"
#include "parameters.h"
#include "parent_algorithm.h"
#include "propertysearch.h"

namespace {

const char kDiffusionFile[] = "models/cell_diffusion.bin";
const char kPredictorFile[] = "models/cell_predictor.bin";

// Where a weights file that is not built in may lie. The environment variable (MATVIZ_CELL_DIFFUSION,
// MATVIZ_CELL_PREDICTOR) names the file and nothing else is tried (a name that does not exist switches that network off).
QStringList modelCandidates(const char* variable, const char* file)
{
    const QString named = qEnvironmentVariable(variable);
    if (!named.isEmpty()) return { named };
    const QDir app(QCoreApplication::applicationDirPath());
    QStringList list = {
        app.filePath(QLatin1String(file)),                                     // next to the executable
        app.filePath(QStringLiteral("../Resources/") + QLatin1String(file)),   // inside a macOS bundle
        app.filePath(QStringLiteral("../../../") + QLatin1String(file)),       // next to the bundle
    };
#ifdef MATVIZ_SOURCE_DIR
    list << QStringLiteral(MATVIZ_SOURCE_DIR "/") + QLatin1String(file);       // the source tree the program was built from
#endif
    return list;
}

QString thousands(size_t count)
{
    QString text = QString::number(qulonglong(count));
    for (qsizetype i = text.size() - 3; i > 0; i -= 3) text.insert(i, QChar(0x2009));   // thin space between the thousands
    return text;
}

QString number(double x) { return QString::number(x, 'g', 3); }

} // namespace

NeuralGeneratorController::NeuralGeneratorController(QObject* parent)
    : QObject(parent)
{
    // MATVIZ_CELL_DECODER=<file>: weights of another training run instead of the built-in ones.
    QString path = qEnvironmentVariable("MATVIZ_CELL_DECODER");
    if (path.isEmpty()) path = QStringLiteral(":/models/cell_decoder.bin");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_message = tr("The decoder weights are missing: %1").arg(path);
    } else {
        const QByteArray data = f.readAll();
        std::string error;
        if (!m_decoder.load(reinterpret_cast<const unsigned char*>(data.constData()), size_t(data.size()), &error))
            m_message = tr("Cannot read the decoder weights %1: %2").arg(path, QString::fromStdString(error));
    }
    if (!m_message.isEmpty()) qWarning().noquote() << "[Neural]" << m_message;

    // The diffusion network: the header only, the 94 MB of weights are read before the first cell.
    if (m_decoder.loaded()) {
        const QStringList candidates = modelCandidates("MATVIZ_CELL_DIFFUSION", kDiffusionFile);
        for (const QString& candidate : candidates) {
            if (!QFileInfo::exists(candidate)) continue;
            const QString file = QDir::cleanPath(candidate);
            std::string error;
            if (!m_diffusion.open(file.toStdString(), &error))
                m_diffusionMissing = tr("Cannot read the diffusion weights %1: %2").arg(file, QString::fromStdString(error));
            else if (m_diffusion.codeChannels() != m_decoder.codeChannels() || m_diffusion.codeSize() != m_decoder.codeSize())
                m_diffusionMissing = tr("The diffusion weights %1 write a code of %2³ × %3, the decoder reads %4³ × %5")
                                         .arg(file).arg(m_diffusion.codeSize()).arg(m_diffusion.codeChannels())
                                         .arg(m_decoder.codeSize()).arg(m_decoder.codeChannels());
            else
                m_useDiffusion = true;
            break;
        }
        if (m_useDiffusion) {
            m_steps = m_diffusion.steps();
            qInfo().noquote() << "[Neural] diffusion network:" << diffusionInfo() << "from" << QString::fromStdString(m_diffusion.path());
        } else {
            if (m_diffusionMissing.isEmpty())
                m_diffusionMissing = tr("The diffusion weights were not found (%1; tools/neural/export_cell_diffusion.py writes the file)")
                                         .arg(candidates.size() == 1 ? candidates.first() : QLatin1String(kDiffusionFile));
            qWarning().noquote() << "[Neural]" << m_diffusionMissing << "- the cell is decoded from a random code";
        }
        // The property predictor: 12 MB, read whole. Without it the tab works as before, with no estimate.
        for (const QString& candidate : modelCandidates("MATVIZ_CELL_PREDICTOR", kPredictorFile)) {
            if (!QFileInfo::exists(candidate)) continue;
            const QString file = QDir::cleanPath(candidate);
            std::string error;
            if (!m_predictor.load(file.toStdString(), &error))
                qWarning().noquote() << "[Neural] cannot read the predictor weights" << file << ":" << QString::fromStdString(error);
            else if (m_predictor.cellSize() != m_decoder.cellSize())
                qWarning().noquote() << QString("[Neural] the predictor weights %1 read a cell of %2^3, the decoder writes %3^3")
                                            .arg(file).arg(m_predictor.cellSize()).arg(m_decoder.cellSize());
            else
                m_usePredictor = true;
            break;
        }
        if (m_usePredictor)
            qInfo().noquote() << "[Neural] property predictor:" << predictorInfo() << "from" << QString::fromStdString(m_predictor.path());
        else
            qInfo().noquote() << "[Neural] no property predictor (models/cell_predictor.bin; tools/neural/export_cell_predictor.py writes the file):"
                              << "the properties of a cell are known after a solve only";
    }
    connect(&m_watcher, &QFutureWatcher<Result>::finished, this, &NeuralGeneratorController::finish);
    connect(&m_solveWatcher, &QFutureWatcher<cellstiff::Solution>::finished, this, &NeuralGeneratorController::finishCompare);
}

NeuralGeneratorController::~NeuralGeneratorController()
{
    // The sampling takes seconds and a solve minutes: stop them at their next step instead of holding the program open.
    m_cancel = true;
    m_solveCancel = true;
    if (m_solving) m_solveWatcher.waitForFinished();
    if (!m_busy) return;
    m_watcher.waitForFinished();
    if (int32_t*** v = m_watcher.result().voxels) Parent_Algorithm::Delete3D<int32_t>(v);
}

QString NeuralGeneratorController::modelInfo() const
{
    if (!available()) return QString();
    return QString::fromUtf8("%1 · %2 weights · code %3³ × %4 → cell %5³")
        .arg(QString::fromStdString(m_decoder.name()), thousands(m_decoder.weightCount()))
        .arg(m_decoder.codeSize()).arg(m_decoder.codeChannels()).arg(m_decoder.cellSize());
}

QString NeuralGeneratorController::diffusionInfo() const
{
    if (!m_useDiffusion) return QString();
    QString info = QString::fromUtf8("%1 · %2 weights · %3 steps")
                       .arg(QString::fromStdString(m_diffusion.name()), thousands(m_diffusion.weightCount())).arg(m_diffusion.steps());
    // Guidance: every step asks the network with the request and with its targets hidden and amplifies the difference.
    if (m_diffusion.guidance() != 1.0f) info += QString::fromUtf8(" · guidance %1").arg(double(m_diffusion.guidance()), 0, 'g', 3);
    return info;
}

QString NeuralGeneratorController::predictorInfo() const
{
    if (!m_usePredictor) return QString();
    return QString::fromUtf8("%1 · %2 weights").arg(QString::fromStdString(m_predictor.name()), thousands(m_predictor.weightCount()));
}

std::map<std::string, double> NeuralGeneratorController::formTargets(const QVariantMap& condition, reqcheck::Material* material,
                                                                     reqcheck::Form* form)
{
    if (form) form->sameInAllDirections = condition.value(QStringLiteral("same_in_all_directions"), false).toBool();
    const QVariantMap m = condition.value(QStringLiteral("material")).toMap();
    if (material) *material = { m.value(QStringLiteral("e_gpa"), 2.0).toDouble(), m.value(QStringLiteral("nu"), 0.38).toDouble(),
                                m.value(QStringLiteral("aniso"), 1.0).toDouble() };
    std::map<std::string, double> targets;
    const QVariantMap given = condition.value(QStringLiteral("targets")).toMap();
    for (auto it = given.cbegin(); it != given.cend(); ++it) targets[it.key().toStdString()] = it.value().toDouble();
    if (condition.contains(QStringLiteral("density"))) targets["density"] = condition.value(QStringLiteral("density")).toDouble();
    return targets;
}

QVariantList NeuralGeneratorController::checkRequest(const QVariantMap& condition) const
{
    reqcheck::Material material;
    reqcheck::Form form;
    const std::map<std::string, double> targets = formTargets(condition, &material, &form);
    QVariantList list;
    for (const reqcheck::Issue& issue : reqcheck::check(targets, material, form)) {
        QStringList keys;
        for (const std::string& k : issue.keys) keys << QString::fromStdString(k);
        list << QVariantMap{ { QStringLiteral("keys"), keys }, { QStringLiteral("text"), QString::fromStdString(issue.text) } };
    }
    return list;
}

void NeuralGeneratorController::generate(const QVariantMap& condition)
{
    if (!available() || m_busy || m_solving) return;   // a solve reads the cell this would replace
    m_condition = condition;
    m_cellMm = std::clamp(condition.value(QStringLiteral("cell_mm"), 12.0).toDouble(), 1.0, 1000.0);
    m_smooth = m_useDiffusion ? 0.0 : std::clamp(condition.value(QStringLiteral("smooth")).toDouble(), 0.0, 4.0);
    m_clean = m_useDiffusion && condition.value(QStringLiteral("clean"), true).toBool();
    m_seed = condition.value(QStringLiteral("seed")).toUInt();
    if (m_seed == 0) m_seed = 1u + QRandomGenerator::global()->bounded(999999999u);
    // The request is sent as it is, conflicts or not; they go to the log and the report.
    m_issues.clear();
    for (const QVariant& issue : checkRequest(condition)) {
        m_issues << issue.toMap().value(QStringLiteral("text")).toString();
        qWarning().noquote() << "[Neural] request check:" << m_issues.last();
    }

    // The request of the diffusion network: the ticked targets in the units of the dataset (the form gives
    // moduli in MPa, the dataset holds Pa) and the material.
    std::vector<float> request;
    m_used.clear();
    m_clipped.clear();
    m_unknown.clear();
    if (m_useDiffusion) {
        const QVariantMap material = condition.value(QStringLiteral("material")).toMap();
        const double eGpa = material.value(QStringLiteral("e_gpa"), 2.0).toDouble();
        auto isModulus = [this](const std::string& key) {
            for (const CellDiffusion::Target& t : m_diffusion.targets())
                if (t.key == key) return t.kind == 3;
            return false;
        };
        std::map<std::string, double> targets;
        const QVariantMap given = condition.value(QStringLiteral("targets")).toMap();
        for (auto it = given.cbegin(); it != given.cend(); ++it) {
            const std::string key = it.key().toStdString();
            targets[key] = it.value().toDouble() * (isModulus(key) ? 1e6 : 1.0);
        }
        if (condition.contains(QStringLiteral("density"))) targets["density"] = condition.value(QStringLiteral("density")).toDouble();
        CellDiffusion::RequestNotes notes;
        request = m_diffusion.request(targets, eGpa, material.value(QStringLiteral("nu"), 0.38).toDouble(),
                                      material.value(QStringLiteral("aniso"), 1.0).toDouble(), &notes);
        for (const std::string& key : notes.used) m_used << QString::fromStdString(key);
        for (const std::string& key : notes.unknown) m_unknown << QString::fromStdString(key);
        // A target outside what the training cells span was moved to the edge: say what the span is.
        for (const std::string& key : notes.clipped) {
            const double scale = isModulus(key) ? 1e-6 : 1.0;
            double lo = 0.0, hi = 0.0;
            m_diffusion.targetRange(key, eGpa, &lo, &hi);
            m_clipped << QString::fromUtf8("%1 (asked %2, range %3–%4%5)")
                             .arg(QString::fromStdString(key), number(targets[key] * scale), number(lo * scale), number(hi * scale),
                                  isModulus(key) ? QStringLiteral(" MPa") : QString());
        }
    }

    const CellDecoder* decoder = &m_decoder;
    CellDiffusion* diffusion = m_useDiffusion ? &m_diffusion : nullptr;
    // The predictor reads the cell with the material of the form.
    const CellPredictor* predictor = m_usePredictor ? &m_predictor : nullptr;
    const QVariantMap formMaterial = condition.value(QStringLiteral("material")).toMap();
    const propsearch::Law law{ formMaterial.value(QStringLiteral("e_gpa"), 2.0).toDouble(), formMaterial.value(QStringLiteral("nu"), 0.38).toDouble(),
                               formMaterial.value(QStringLiteral("aniso"), 1.0).toDouble() };
    const quint32 seed = m_seed;
    const double smooth = m_smooth;
    const bool clean = m_clean;
    // The thresholds are read here: the settings belong to the GUI thread.
    const PrintabilityThresholds thresholds = Parameters::printThresholds(m_cellMm / m_decoder.cellSize());
    m_cancel = false;
    m_step = 0;
    m_message.clear();
    m_busy = true;
    emit busyChanged();
    emit progressChanged();
    emit reportChanged();
    m_watcher.setFuture(QtConcurrent::run([this, decoder, diffusion, predictor, law, request, seed, smooth, clean, thresholds]() {
        Result r;
        QElapsedTimer timer;
        timer.start();
        std::vector<float> code;
        if (diffusion) {
            // The code is written from the request: noise, then 50 steps that remove it (two passes of the network per
            // step when the weights carry a guidance: with the request and with its targets hidden).
            std::string error;
            if (!diffusion->load(&error)) {
                r.error = tr("Cannot read the diffusion weights: %1").arg(QString::fromStdString(error));
                return r;
            }
            code = diffusion->sample(request, diffusion->startNoise(seed), [this](int step, int steps) {
                if (m_cancel.load()) return false;
                QMetaObject::invokeMethod(this, [this, step, steps]() {
                    m_step = step + 1;
                    m_steps = steps;
                    emit progressChanged();
                }, Qt::QueuedConnection);
                return true;
            });
            if (code.empty()) {
                if (!m_cancel.load()) r.error = tr("The diffusion network returned no code");
                return r;
            }
            // The network's code has zero mean and unit spread per channel; the decoder reads the codes of the autoencoder.
            const size_t perChannel = code.size() / size_t(diffusion->codeChannels());
            for (int c = 0; c < diffusion->codeChannels(); ++c) {
                const float spread = diffusion->codeSpread()[size_t(c)], mean = diffusion->codeMean()[size_t(c)];
                float* v = code.data() + size_t(c) * perChannel;
                for (size_t i = 0; i < perChannel; ++i) v[i] = v[i] * spread + mean;
            }
            r.usedDiffusion = true;
            r.steps = diffusion->steps();
            r.sampleMs = double(timer.nsecsElapsed()) / 1e6;
            timer.restart();
        } else {
            // No diffusion weights: random numbers with the statistics of real codes.
            code = decoder->randomCode(seed, smooth);
        }
        std::vector<uint8_t> solid = decoder->decode(code);
        r.decodeMs = double(timer.nsecsElapsed()) / 1e6;
        timer.restart();
        const int n = r.n = decoder->cellSize();
        if (clean) {
            r.solidVoxels = size_t(std::count(solid.begin(), solid.end(), uint8_t(1)));
            r.removedVoxels = CellDecoder::keepLargestBody(solid, n, &r.removedPieces);
        }
        if (predictor) {
            // The network's estimate of the properties of the cell as it will be shown (after the cleaning).
            timer.restart();
            r.predicted = predictor->predict(solid, law.eGpa, law.nu, law.aniso);
            r.predictMs = double(timer.nsecsElapsed()) / 1e6;
            timer.restart();
        }
        r.voxels = Parent_Algorithm::Create3D<int32_t>(n, n, n);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                for (int k = 0; k < n; ++k)
                    r.voxels[i][j][k] = solid[(size_t(i) * n + j) * n + k];
        r.print = checkPrintability(r.voxels, n, thresholds);
        r.checkMs = double(timer.nsecsElapsed()) / 1e6;
        return r;
    }));
}

void NeuralGeneratorController::finish()
{
    const Result r = m_watcher.result();
    m_step = 0;
    if (!r.voxels) {
        // No cell: the weights could not be read (or the program is closing). The view keeps what it shows.
        m_message = r.error;
        if (!r.error.isEmpty()) qWarning().noquote() << "[Neural]" << r.error;
        m_busy = false;
        emit busyChanged();
        emit progressChanged();
        emit reportChanged();
        if (m_compareNext) {   // nothing to solve: whoever waits for the comparison must not wait for ever
            m_compareNext = false;
            emit compareFinished();
        }
        return;
    }
    // The hand-over below frees the grid, which a structure generation or a stress solve may
    // still be working on: then the cell is shown when that is done. m_busy stays on meanwhile,
    // so the result is not replaced, and the destructor frees it if the program closes first.
    if (Parameters::voxelsBusy()) {
        if (!m_showWaiting)
            qInfo().noquote() << "[Neural] the cell is made; it goes to the 3D view when the running generation or solve is done";
        m_showWaiting = true;
        Parameters::whenVoxelsFree(this, [this]() { finish(); });
        return;
    }
    m_showWaiting = false;

    const int n = r.n;
    const double voxelMm = m_cellMm / n;

    // The same hand-over as Hdf5ProjectController::pushTo3DView for an opened cell.
    Parameters* p = Parameters::instance();
    p->setSize(n);
    p->setPoints(1);
    // No results file: a solve on this cell must not write into the cell file opened last.
    Parameters::filename.clear();
    Parameters::phaseAssignment.clear();
    Parameters::percolation_rank6 = -1;          // the rank DLCA published belongs to the previous structure
    const QVariantMap material = m_condition.value(QStringLiteral("material")).toMap();
    Parameters::publishCellMaterial({ { QStringLiteral("mat_e_gpa"), material.value(QStringLiteral("e_gpa"), 2.0) },
                                      { QStringLiteral("mat_nu"), material.value(QStringLiteral("nu"), 0.38) },
                                      { QStringLiteral("mat_aniso"), material.value(QStringLiteral("aniso"), 1.0) } },
                                    QStringLiteral("Lattice"),
                                    QStringLiteral("[Neural] solid of the generated cell = %1 (the material set on the Neural tab)"));
    Parameters::lattice_voxel_mm = voxelMm;

    OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance();
    // A stress field of the previous structure does not belong to this one.
    if (ogl && ogl->hasField()) ogl->clearFieldVisualization();
    int32_t*** old = Parameters::voxels;
    Parameters::voxels = r.voxels;
    if (ogl) {
        ogl->setNumColors(1);
        ogl->setVoxels(Parameters::voxels, static_cast<short>(n));
        ogl->setGrainOrientations({});
    }
    if (old) Parent_Algorithm::Delete3D<int32_t>(old);
    if (m_afterShow) m_afterShow();

    // What compare() needs of this cell: its voxels, its material and what was asked, in the units of the form.
    m_cell = cellstiff::occupancy(r.voxels, n);
    m_cellN = n;
    m_cellDensity = double(std::count(m_cell.begin(), m_cell.end(), uint8_t(1))) / double(m_cell.size());
    m_cellLaw = { material.value(QStringLiteral("e_gpa"), 2.0).toDouble(), material.value(QStringLiteral("nu"), 0.38).toDouble(),
                  material.value(QStringLiteral("aniso"), 1.0).toDouble() };
    m_asked.clear();
    const QVariantMap asked = m_condition.value(QStringLiteral("targets")).toMap();
    for (auto it = asked.cbegin(); it != asked.cend(); ++it) m_asked[it.key().toStdString()] = it.value().toDouble();
    m_askedDensity = m_condition.contains(QStringLiteral("density")) ? m_condition.value(QStringLiteral("density")).toDouble()
                                                                     : std::numeric_limits<double>::quiet_NaN();
    m_predicted = r.predicted;
    m_solveInfo.clear();
    setRows(nullptr);

    const PrintabilityReport& pr = r.print;
    QStringList targetNames = m_condition.value(QStringLiteral("targets")).toMap().keys();
    const bool densityAsked = m_condition.contains(QStringLiteral("density"));
    if (densityAsked) targetNames << QStringLiteral("density");
    const int targets = int(targetNames.size());
    const double removedPct = r.solidVoxels > 0 ? 100.0 * double(r.removedVoxels) / double(r.solidVoxels) : 0.0;
    m_report = QVariantMap{
        { QStringLiteral("seed"), m_seed },
        { QStringLiteral("diffusion"), r.usedDiffusion },
        { QStringLiteral("decode_ms"), r.decodeMs },
        { QStringLiteral("check_ms"), r.checkMs },
        { QStringLiteral("density"), pr.solid_fraction },
        { QStringLiteral("bodies"), pr.components },
        { QStringLiteral("islands"), pr.islands },
        { QStringLiteral("directions"), pr.connected_directions },
        { QStringLiteral("thin_pct"), 100.0 * pr.thin_fraction },
        { QStringLiteral("wall_mm"), pr.mean_thickness_vox * voxelMm },
        { QStringLiteral("unsupported_pct"), 100.0 * pr.unsupported_best },
        { QStringLiteral("build_direction"), QString::fromLatin1(PrintabilityReport::directionName(pr.best_direction)) },
        { QStringLiteral("ok"), pr.ok },
        { QStringLiteral("verdict"), QString::fromStdString(pr.verdict) },
        { QStringLiteral("cell_mm"), m_cellMm },
        { QStringLiteral("voxel_mm"), voxelMm },
        { QStringLiteral("targets"), targets },
        { QStringLiteral("target_names"), targetNames },
        { QStringLiteral("issues"), m_issues },
    };
    const QString bodies = pr.components == 1 ? QStringLiteral("1 body") : QString("%1 bodies").arg(pr.components);
    if (r.usedDiffusion) {
        m_report.insert(QStringLiteral("model"), QString::fromStdString(m_diffusion.name()));
        m_report.insert(QStringLiteral("steps"), r.steps);
        m_report.insert(QStringLiteral("sample_ms"), r.sampleMs);
        if (densityAsked) m_report.insert(QStringLiteral("asked_density"), m_condition.value(QStringLiteral("density")).toDouble());
        m_report.insert(QStringLiteral("used"), m_used);
        m_report.insert(QStringLiteral("clipped"), m_clipped);
        m_report.insert(QStringLiteral("unknown"), m_unknown);
        m_report.insert(QStringLiteral("cleaned"), m_clean);
        m_report.insert(QStringLiteral("removed_pieces"), r.removedPieces);
        m_report.insert(QStringLiteral("removed_pct"), removedPct);

        QString line = QString("[Neural] %1, seed %2: %3 steps in %4 s, decoded in %5 ms; density %6")
                           .arg(QString::fromStdString(m_diffusion.name())).arg(m_seed).arg(r.steps)
                           .arg(r.sampleMs / 1e3, 0, 'f', 1).arg(r.decodeMs, 0, 'f', 0).arg(pr.solid_fraction, 0, 'f', 3);
        if (densityAsked) line += QString(" (asked %1)").arg(m_condition.value(QStringLiteral("density")).toDouble(), 0, 'f', 2);
        if (m_clean)
            line += r.removedPieces > 0 ? QString(", %1 loose %2 cleared (%3 % of the solid)").arg(r.removedPieces)
                                              .arg(r.removedPieces == 1 ? QStringLiteral("piece") : QStringLiteral("pieces")).arg(removedPct, 0, 'f', 2)
                                        : QString(", no loose pieces");
        line += QString(", %1, joined in %2 directions, thin %3 %, print check: %4; targets passed: %5")
                    .arg(bodies).arg(pr.connected_directions).arg(100.0 * pr.thin_fraction, 0, 'f', 1)
                    .arg(QString::fromStdString(pr.verdict), m_used.isEmpty() ? QStringLiteral("none") : m_used.join(QStringLiteral(", ")));
        if (!m_clipped.isEmpty()) line += QStringLiteral("; at the edge of the training range: ") + m_clipped.join(QStringLiteral(", "));
        if (!m_unknown.isEmpty()) line += QStringLiteral("; not passed: ") + m_unknown.join(QStringLiteral(", "));
        qInfo().noquote() << line;
    } else {
        m_report.insert(QStringLiteral("smooth"), m_smooth);
        qInfo().noquote() << QString("[Neural] seed %1, code blur %2: decoded in %3 ms, density %4, %5, joined in %6 directions, "
                                     "thin %7 %, print check: %8 (random code; targets set: %9, not used)")
                                 .arg(m_seed).arg(m_smooth).arg(r.decodeMs, 0, 'f', 0).arg(pr.solid_fraction, 0, 'f', 3).arg(bodies)
                                 .arg(pr.connected_directions).arg(100.0 * pr.thin_fraction, 0, 'f', 1)
                                 .arg(QString::fromStdString(pr.verdict)).arg(targets);
    }
    if (!m_predicted.empty()) {
        // The estimate of every property by its key, in the units of the form (moduli in MPa).
        QVariantMap predicted;
        for (const CellPredictor::Target& t : m_predictor.targets()) {
            const auto it = m_predicted.find(t.key);
            if (it != m_predicted.end()) predicted.insert(QString::fromStdString(t.key), it->second * (t.kind == 3 ? 1e-6 : 1.0));
        }
        m_report.insert(QStringLiteral("predictor"), QString::fromStdString(m_predictor.name()));
        m_report.insert(QStringLiteral("predict_ms"), r.predictMs);
        m_report.insert(QStringLiteral("predicted"), predicted);
        auto mpa = [&predicted](const char* key) { return number(predicted.value(QLatin1String(key)).toDouble()); };
        QString line = QString("[Neural] predicted by %1 in %2 ms: E x / y / z %3 / %4 / %5 MPa, G xy / yz / xz %6 / %7 / %8 MPa, density %9")
                           .arg(QString::fromStdString(m_predictor.name())).arg(r.predictMs, 0, 'f', 0)
                           .arg(mpa("Ex"), mpa("Ey"), mpa("Ez"), mpa("Gxy"), mpa("Gyz"), mpa("Gxz"), mpa("density"));
        for (const QVariant& item : std::as_const(m_comparison)) {
            const QVariantMap row = item.toMap();
            const double deviation = row.value(QStringLiteral("predicted_deviation")).toDouble();
            if (!std::isfinite(deviation)) continue;
            const bool relative = row.value(QStringLiteral("relative")).toBool();
            line += QString("; %1 asked %2, predicted %3 (%4%5%6)")
                        .arg(row.value(QStringLiteral("key")).toString(), number(row.value(QStringLiteral("asked")).toDouble()),
                             number(row.value(QStringLiteral("predicted")).toDouble()), deviation > 0 ? QStringLiteral("+") : QString())
                        .arg(deviation, 0, 'f', relative ? 1 : 3).arg(relative ? QStringLiteral(" %") : QString());
        }
        qInfo().noquote() << line;
    }
    m_busy = false;
    emit busyChanged();
    emit progressChanged();
    emit reportChanged();
    emit comparisonChanged();
    if (m_compareNext) {
        m_compareNext = false;
        compare();
    }
}

void NeuralGeneratorController::setRows(const propsearch::Values* obtained)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // The predictor's estimate goes through the same comparison as a solved cell: the same rows, units and deviations.
    std::vector<cellstiff::Row> guess;
    if (!m_predicted.empty()) {
        propsearch::Values values;
        values.fill(nan);
        for (int t = 0; t < propsearch::TargetCount; ++t) {
            const auto it = m_predicted.find(propsearch::targetKey(t).toStdString());
            if (it != m_predicted.end()) values[size_t(t)] = it->second;
        }
        const auto density = m_predicted.find("density");
        guess = cellstiff::compare(m_asked, m_askedDensity, &values, density != m_predicted.end() ? density->second : nan);
    }
    m_comparison.clear();
    const std::vector<cellstiff::Row> rows = cellstiff::compare(m_asked, m_askedDensity, obtained, m_cellDensity);
    for (size_t i = 0; i < rows.size(); ++i) {
        const cellstiff::Row& r = rows[i];
        const bool guessed = i < guess.size() && guess[i].key == r.key;
        m_comparison << QVariantMap{ { QStringLiteral("key"), QString::fromStdString(r.key) },
                                     { QStringLiteral("modulus"), r.modulus },
                                     { QStringLiteral("relative"), r.relative },
                                     { QStringLiteral("asked"), r.asked },
                                     { QStringLiteral("obtained"), r.obtained },
                                     { QStringLiteral("deviation"), r.deviation },
                                     { QStringLiteral("predicted"), guessed ? guess[i].obtained : nan },
                                     { QStringLiteral("predicted_deviation"), guessed ? guess[i].deviation : nan } };
    }
}

void NeuralGeneratorController::compare()
{
    if (m_busy || m_solving || m_cell.empty()) return;
    cellstiff::Settings settings;                               // those of the dataset
    settings.threads = std::max(1, Parameters::num_threads);    // read here: the settings belong to the GUI thread
    m_solveThreads = settings.threads;
    m_solveCancel = false;
    m_solving = true;
    m_solveClock.start();
    m_solveProgress = tr("starting the solve");
    m_solveInfo.clear();
    setRows(nullptr);
    emit solvingChanged();
    emit solveProgressChanged();
    emit comparisonChanged();
    qInfo().noquote() << QString("[Neural] solving the cell of seed %1: 6 loads on a %2^3 grid (on the GPU when its solver is found, else %3 threads)")
                             .arg(m_seed).arg(m_cellN).arg(settings.threads);

    const std::vector<uint8_t> cell = m_cell;
    const int n = m_cellN;
    const propsearch::Law law = m_cellLaw;
    m_solveWatcher.setFuture(QtConcurrent::run([this, cell, n, law, settings]() {
        // The solver reports every iteration, a few per second at most are shown.
        QElapsedTimer clock;
        clock.start();
        qint64 shown = -1000;
        auto progress = [this, &clock, &shown](int pass, int load, int iteration, double error) {
            const qint64 now = clock.elapsed();
            if (now - shown < 150) return;
            shown = now;
            QMetaObject::invokeMethod(this, [this, pass, load, iteration, error]() {
                if (!m_solving) return;
                // The GPU solver runs the six loads together: load = -(1 + loads that have converged).
                m_solveProgress = (load < 0 ? QString::fromUtf8("%1GPU · 6 loads at once, %2 done · step %3 · residual %4 · %5 s")
                                            : QString::fromUtf8("%1load %2 of 6 · iteration %3 · residual %4 · %5 s"))
                                      .arg(pass > 0 ? tr("softer void · ") : QString()).arg(load < 0 ? -load - 1 : load + 1).arg(iteration)
                                      .arg(QString::number(error, 'e', 1)).arg(m_solveClock.elapsed() / 1000);
                emit solveProgressChanged();
            }, Qt::QueuedConnection);
        };
        // The void was propping the cell up: the values of the first solve are shown while the second one runs.
        auto firstPass = [this](const cellstiff::Solution& first) {
            QMetaObject::invokeMethod(this, [this, first]() {
                if (m_solving) showSolution(first, QStringLiteral("first"));
            }, Qt::QueuedConnection);
        };
        // On the GPU when an interpreter with PyTorch is there (looked up once, a few seconds), else - or when that
        // process fails - the program's own solver on the CPU. Same equations and settings, so the same answer.
        const cellstiff::GpuSolver gpu = cellstiff::gpuSolver();
        if (gpu.available()) {
            const cellstiff::Solution s = cellstiff::solveOnGpu(gpu, cell, n, law, settings, progress, firstPass, &m_solveCancel);
            if (s.ok || s.cancelled) return s;
            qWarning().noquote() << QString("[Neural] the GPU solve gave no answer (%1): solving on the CPU").arg(QString::fromStdString(s.error));
        } else {
            qInfo().noquote() << QString("[Neural] no GPU solver (%1): solving on the CPU").arg(gpu.reason);
        }
        return cellstiff::solve(cell, n, law, settings, progress, firstPass, &m_solveCancel);
    }));
}

void NeuralGeneratorController::showSolution(const cellstiff::Solution& solution, const QString& state)
{
    const propsearch::Values v = propsearch::moduli(solution.C, true);
    setRows(&v);
    QVariantMap values;
    for (int t = 0; t < propsearch::TargetCount; ++t)
        values.insert(propsearch::targetKey(t), v[size_t(t)] * (propsearch::isModulus(t) ? 1e-6 : 1.0));
    m_solveInfo = QVariantMap{
        { QStringLiteral("state"), state },
        { QStringLiteral("seconds"), solution.seconds },
        { QStringLiteral("iterations"), solution.iterations },
        { QStringLiteral("threads"), m_solveThreads },
        { QStringLiteral("backend"), QString::fromStdString(solution.backend) },   // empty: the CPU solver
        { QStringLiteral("grid"), m_cellN },
        { QStringLiteral("void_eta"), solution.voidEta },
        { QStringLiteral("share"), solution.share },
        { QStringLiteral("share_first"), solution.shareFirst },
        { QStringLiteral("passes"), solution.passes },
        { QStringLiteral("converged"), solution.converged },
        { QStringLiteral("load_path"), solution.share >= cellstiff::kMinLoadPathShare },
        { QStringLiteral("values"), values },
    };
    if (!solution.error.empty()) m_solveInfo.insert(QStringLiteral("error"), QString::fromStdString(solution.error));
    emit comparisonChanged();
}

void NeuralGeneratorController::finishCompare()
{
    const cellstiff::Solution s = m_solveWatcher.result();
    m_solving = false;
    m_solveProgress.clear();
    if (s.ok) {
        showSolution(s, s.cancelled ? QStringLiteral("stopped") : QStringLiteral("done"));
        QString line = QString("[Neural] seed %1 solved in %2 s: %3 iterations, %4, void %5, the solid holds %6 % of the strain energy")
                           .arg(m_seed).arg(s.seconds, 0, 'f', s.seconds < 100 ? 1 : 0).arg(s.iterations)
                           .arg(s.backend.empty() ? QString("%1 threads").arg(m_solveThreads) : QString("on the GPU (%1)").arg(QString::fromStdString(s.backend)))
                           .arg(QString::number(s.voidEta, 'g', 3)).arg(100.0 * s.share, 0, 'f', 1);
        if (s.passes > 1) line += QString(" (%1 % in the first solve with void %2)").arg(100.0 * s.shareFirst, 0, 'f', 1).arg(number(cellstiff::Settings().voidEta));
        if (s.cancelled) line += QStringLiteral("; stopped before the solve with a softer void ended, the values are of the first solve");
        if (!s.converged) line += QStringLiteral("; some load did not reach the tolerance");
        if (!(s.share >= cellstiff::kMinLoadPathShare)) line += QStringLiteral("; no solid load path in some direction");
        if (!s.error.empty()) line += QStringLiteral("; ") + QString::fromStdString(s.error);
        qInfo().noquote() << line;
        for (const QVariant& item : std::as_const(m_comparison)) {
            const QVariantMap row = item.toMap();
            const bool relative = row.value(QStringLiteral("relative")).toBool();
            const QString unit = row.value(QStringLiteral("modulus")).toBool() ? QStringLiteral(" MPa") : QString();
            const double deviation = row.value(QStringLiteral("deviation")).toDouble();
            const double predicted = row.value(QStringLiteral("predicted")).toDouble();
            qInfo().noquote() << QString("[Neural]   %1: asked %2%4, obtained %3%4, %5%6")
                                     .arg(row.value(QStringLiteral("key")).toString(), -8)
                                     .arg(number(row.value(QStringLiteral("asked")).toDouble()), number(row.value(QStringLiteral("obtained")).toDouble()), unit)
                                     .arg(std::isfinite(deviation) ? QString("%1%2%3").arg(deviation > 0 ? QStringLiteral("+") : QString())
                                                                         .arg(deviation, 0, 'f', relative ? 1 : 3).arg(relative ? QStringLiteral(" %") : QString())
                                                                   : QStringLiteral("deviation unknown"))
                                     .arg(std::isfinite(predicted) ? QString("; the predictor said %1%2").arg(number(predicted), unit) : QString());
        }
    } else {
        // Stopped in the first solve, or the solve could not be done: no values.
        m_solveInfo.clear();
        if (!s.error.empty()) {
            m_solveInfo.insert(QStringLiteral("error"), QString::fromStdString(s.error));
            qWarning().noquote() << "[Neural] the cell could not be solved:" << QString::fromStdString(s.error);
        } else {
            qInfo().noquote() << QString("[Neural] the solve of seed %1 was stopped after %2 s").arg(m_seed).arg(s.seconds, 0, 'f', 0);
        }
        setRows(nullptr);
        emit comparisonChanged();
    }
    emit solvingChanged();
    emit solveProgressChanged();
    emit compareFinished();
}

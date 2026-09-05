#include "stressanalysiscontroller.h"
#include "stressanalysis.h"
#include "stressanalysis_fft.h"
#include "parameters.h"
#include "loadstepmanager.h"
#include "hdf5wrapper.h"
#include "ansyswrapper.h"   // tensor_components enum (SX..SXZ, SEQV) -- shares layout with fftsa::ResCol
#include "openglwidgetqml.h"

#include <QDebug>
#include <QFileDialog>
#include <QtConcurrent/QtConcurrent>
#include <cmath>
#include <algorithm>

StressAnalysisController* StressAnalysisController::s_instance = nullptr;

namespace {
bool isAnsys(const QString& solver) { return solver.compare("ansys", Qt::CaseInsensitive) == 0; }

// Label + tensor_components enum value. ANSYS has all 16 plottable
// components (nodal displacement + stress + strain); FFT has no nodal
// displacement field, so it's missing UX/UY/UZ/USUM. SEQV sits at index 6 in
// both lists so it can serve as the shared default selection.
struct ComponentEntry { const char* label; int value; };
const ComponentEntry kAnsysComponents[] = {
    {"SX", SX}, {"SY", SY}, {"SZ", SZ}, {"SXY", SXY}, {"SYZ", SYZ}, {"SXZ", SXZ}, {"von Mises (SEQV)", SEQV},
    {"EpsX", EpsX}, {"EpsY", EpsY}, {"EpsZ", EpsZ}, {"EpsXY", EpsXY}, {"EpsYZ", EpsYZ}, {"EpsXZ", EpsXZ}, {"eqv. strain", EpsEQV},
    {"UX", UX}, {"UY", UY}, {"UZ", UZ}, {"USUM", USUM},
};
const ComponentEntry kFFTComponents[] = {
    {"SX", SX}, {"SY", SY}, {"SZ", SZ}, {"SXY", SXY}, {"SYZ", SYZ}, {"SXZ", SXZ}, {"von Mises (SEQV)", SEQV},
    {"EpsX", EpsX}, {"EpsY", EpsY}, {"EpsZ", EpsZ}, {"EpsXY", EpsXY}, {"EpsYZ", EpsYZ}, {"EpsXZ", EpsXZ}, {"eqv. strain", EpsEQV},
};
constexpr int kDefaultComponentIndex = 6; // SEQV, in both lists
}

StressAnalysisController::StressAnalysisController(QObject* parent)
    : QObject(parent)
{
    s_instance = this;
    connect(&m_singleShotWatcher, &QFutureWatcher<SingleShotResult>::finished,
            this, &StressAnalysisController::onSingleShotFinished);
    connect(&m_datasetWatcher, &QFutureWatcher<void>::finished,
            this, &StressAnalysisController::onDatasetFinished);
    connect(&m_stiffnessWatcher, &QFutureWatcher<StiffnessMatrixResult>::finished,
            this, &StressAnalysisController::onStiffnessFinished);
}

QVariantList StressAnalysisController::matrixToVariant(const double m[6][6])
{
    QVariantList rows;
    for (int i = 0; i < 6; ++i) {
        QVariantList row;
        for (int j = 0; j < 6; ++j) row.append(m[i][j]);
        rows.append(QVariant(row));
    }
    return rows;
}

QVariantList StressAnalysisController::stiffnessModuli() const
{
    QVariantList out;
    for (double v : m_lastStiffness.moduli) out.append(v);
    return out;
}

QVariantList StressAnalysisController::convergencePoints() const
{
    QVariantList out;
    for (const auto& p : m_convergence) {
        QVariantList pt;
        pt << p.loadIndex << p.iteration << p.error;
        out.append(QVariant(pt));
    }
    return out;
}

void StressAnalysisController::resetConvergence(bool isFFT, int loadCount)
{
    m_convergence.clear();
    m_convergenceIsFFT     = isFFT;
    m_convergenceLoadCount = loadCount;
    m_convergenceTol       = isFFT ? StressAnalysisFFT().fft_tol : 0.0;
    emit convergenceChanged();
}

void StressAnalysisController::appendConvergencePoint(int loadIndex, int iteration, double error)
{
    m_convergence.push_back({loadIndex, iteration, error});
    emit convergenceChanged();
}

QVariantList StressAnalysisController::resultStress() const
{
    QVariantList list;
    for (double v : m_lastResult.macro_stress) list.append(v);
    return list;
}

void StressAnalysisController::setRunning(bool running)
{
    if (m_isRunning == running) return;
    m_isRunning = running;
    emit isRunningChanged();
}

void StressAnalysisController::setError(const QString& message)
{
    m_lastErrorMessage = message;
    qWarning() << "[StressAnalysisController]" << message;
    emit errorChanged();
}

void StressAnalysisController::setNumSamples(int value)
{
    if (value <= 0 || m_numSamples == value) return;
    m_numSamples = value;
    emit numSamplesChanged();
}

void StressAnalysisController::setNumCalib(int value)
{
    if (value <= 0 || m_numCalib == value) return;
    m_numCalib = value;
    emit numCalibChanged();
}

void StressAnalysisController::setStrainVal(double value)
{
    if (value <= 0.0 || m_strainVal == value) return;
    m_strainVal = value;
    emit strainValChanged();
}

QStringList StressAnalysisController::fieldComponents() const
{
    QStringList list;
    if (m_lastResult.ansysField) {
        for (const auto& c : kAnsysComponents) list << QString::fromLatin1(c.label);
    } else if (m_lastResult.fftField) {
        for (const auto& c : kFFTComponents) list << QString::fromLatin1(c.label);
    }
    return list;
}

int StressAnalysisController::currentComponentEnum() const
{
    if (m_lastResult.ansysField) {
        const int n = int(sizeof(kAnsysComponents) / sizeof(kAnsysComponents[0]));
        return kAnsysComponents[std::clamp(m_fieldComponentIndex, 0, n - 1)].value;
    }
    if (m_lastResult.fftField) {
        const int n = int(sizeof(kFFTComponents) / sizeof(kFFTComponents[0]));
        return kFFTComponents[std::clamp(m_fieldComponentIndex, 0, n - 1)].value;
    }
    return SEQV;
}

void StressAnalysisController::setFieldComponentIndex(int index)
{
    if (index < 0) index = 0;
    if (m_fieldComponentIndex == index) return;
    m_fieldComponentIndex = index;
    if (OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance())
        ogl->setFieldComponent(currentComponentEnum());
    emit fieldComponentChanged();
}

double StressAnalysisController::fieldMin() const
{
    const int comp = currentComponentEnum();
    if (m_lastResult.ansysField) {
        const auto& mn = m_lastResult.ansysField->loadstep_results_min;
        return (comp >= 0 && comp < (int)mn.size()) ? mn[comp] : 0.0;
    }
    if (m_lastResult.fftField && comp >= 0 && comp < (int)m_lastResult.fftField->componentMin.size())
        return m_lastResult.fftField->componentMin[comp];
    return 0.0;
}

double StressAnalysisController::fieldMax() const
{
    const int comp = currentComponentEnum();
    if (m_lastResult.ansysField) {
        const auto& mx = m_lastResult.ansysField->loadstep_results_max;
        return (comp >= 0 && comp < (int)mx.size()) ? mx[comp] : 0.0;
    }
    if (m_lastResult.fftField && comp >= 0 && comp < (int)m_lastResult.fftField->componentMax.size())
        return m_lastResult.fftField->componentMax[comp];
    return 0.0;
}

void StressAnalysisController::setShowDeformed(bool show)
{
    if (m_showDeformed == show) return;
    m_showDeformed = show;
    if (OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance())
        ogl->setShowDeformed(show);
    emit showDeformedChanged();
}

void StressAnalysisController::setDeformedScale(double scale)
{
    if (qFuzzyCompare(m_deformedScale + 1.0, scale + 1.0)) return; // +1 guards the scale==0 case
    m_deformedScale = scale;
    if (OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance())
        ogl->setDeformedScale(float(scale));
    emit deformedScaleChanged();
}

// Toggles the 3D view between field coloring and plain grain coloring. Unlike
// clearFieldVisualization() (which drops the field mode entirely), this keeps
// m_lastResult's ansysField/fftField around so flipping back on re-applies the
// same field without re-solving.
void StressAnalysisController::setShowField(bool show)
{
    if (m_showField == show) return;
    m_showField = show;

    if (OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance()) {
        if (show) {
            if (m_lastResult.ansysField)
                ogl->showAnsysField(m_lastResult.ansysField, currentComponentEnum());
            else if (m_lastResult.fftField)
                ogl->showFFTField(m_lastResult.fftField, currentComponentEnum());
            // clearFieldVisualization() force-reset the renderer's showDeformed
            // flag to false -- restore it to what the panel still shows.
            ogl->setShowDeformed(m_showDeformed);
            ogl->setDeformedScale(float(m_deformedScale));
        } else {
            ogl->clearFieldVisualization();
        }
    }
    emit showFieldChanged();
}

// Pushes m_lastResult's field into the 3D view right after a successful
// single-shot solve, defaulting to SEQV and a deformed-scale that targets
// ~15% of the model size (real strains are far too small to see un-scaled).
void StressAnalysisController::pushResultToView()
{
    OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance();
    if (!ogl) return;

    const short numCubes = (short) Parameters::instance()->getSize();

    m_fieldComponentIndex = kDefaultComponentIndex;
    m_showDeformed = false;
    m_showField = true;

    double charDisp = 1e-12;
    if (m_lastResult.ansysField) {
        const auto& mn = m_lastResult.ansysField->loadstep_results_min;
        const auto& mx = m_lastResult.ansysField->loadstep_results_max;
        if (USUM < (int)mn.size() && USUM < (int)mx.size())
            charDisp = std::max(charDisp, double(mx[USUM] - mn[USUM]));
        ogl->showAnsysField(m_lastResult.ansysField, currentComponentEnum());
    } else if (m_lastResult.fftField) {
        const double* eps = m_lastResult.fftField->macroStrain;
        double epsNorm = 0.0;
        for (int i = 0; i < 6; ++i) epsNorm += eps[i] * eps[i];
        charDisp = std::max(charDisp, std::sqrt(epsNorm) * numCubes);
        ogl->showFFTField(m_lastResult.fftField, currentComponentEnum());
    } else {
        ogl->clearFieldVisualization();
        m_showField = false;
        emit showFieldChanged();
        return;
    }

    m_deformedScale = 0.15 * numCubes / charDisp;
    ogl->setShowDeformed(m_showDeformed);
    ogl->setDeformedScale(float(m_deformedScale));

    emit fieldComponentChanged();
    emit showDeformedChanged();
    emit deformedScaleChanged();
    emit showFieldChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Single known load case -- FFT or ANSYS, no HDF5 write. Runs on a
//  QtConcurrent worker thread: solveSingleLoadCase() touches no GUI/QML
//  state, so only its inputs need to be snapshotted here on the main thread
//  and its SingleShotResult (plain data) handed back via QFutureWatcher.
//
//  For FFT, the worker also gets a per-iteration callback so the convergence
//  plot can update live. The lambda captures `this` only to hand it to
//  QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection) -- it never
//  touches controller state directly off the main thread, and Qt safely
//  drops the queued call if this controller is destroyed first.
// ─────────────────────────────────────────────────────────────────────────────
void StressAnalysisController::runSingleShot(const QString& solver, const QVariantList& eps)
{
    if (m_isRunning) return;   // Run button is disabled while running; this is just a safety net.
    if (!Parameters::voxels) {
        setError(tr("No structure generated — press START first"));
        return;
    }
    if (eps.size() != 6) {
        setError(tr("Expected 6 strain tensor components, got %1").arg(eps.size()));
        return;
    }

    for (int i = 0; i < 6; ++i) m_lastEps[i] = eps.at(i).toDouble();

    const bool  ansys     = isAnsys(solver);
    const short numCubes  = (short) Parameters::instance()->getSize();
    const short numPoints = (short) Parameters::instance()->getPoints();
    int32_t***  voxels    = Parameters::voxels;   // snapshot the pointer -- START is disabled while running
    std::array<double, 6> e;
    std::copy(std::begin(m_lastEps), std::end(m_lastEps), e.begin());

    resetConvergence(!ansys, 1);
    setRunning(true);

    QFuture<SingleShotResult> future = QtConcurrent::run([this, ansys, numCubes, numPoints, voxels, e]() {
        if (ansys)
            return StressAnalysis().solveSingleLoadCase(numCubes, numPoints, voxels, e.data());

        auto onIter = [this](int iter, double err) {
            QMetaObject::invokeMethod(this, [this, iter, err]() {
                appendConvergencePoint(0, iter, err);
            }, Qt::QueuedConnection);
        };
        return StressAnalysisFFT().solveSingleLoadCase(numCubes, numPoints, voxels, e.data(), onIter);
    });
    m_singleShotWatcher.setFuture(future);
}

void StressAnalysisController::onSingleShotFinished()
{
    const SingleShotResult r = m_singleShotWatcher.result();
    setRunning(false);

    if (!r.ok) {
        m_hasResult = false;
        m_lastResult = SingleShotResult{};   // drop any stale ansysField/fftField
        if (OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance())
            ogl->clearFieldVisualization();
        if (m_showField) { m_showField = false; emit showFieldChanged(); }
        setError(r.errorMessage.isEmpty() ? tr("Solve failed") : r.errorMessage);
        emit resultChanged();
        return;
    }

    m_lastResult = r;
    m_hasResult = true;
    m_lastErrorMessage.clear();
    pushResultToView();
    emit resultChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Full dataset-build pipeline -- FFT or ANSYS, writes its own HDF5 dataset.
//  Also runs on a worker thread; estimateStressWithANSYS/FFT() no longer
//  touch LoadStepManager themselves (see stressanalysis.cpp/stressanalysis_
//  fft.cpp) -- onDatasetFinished() reloads it here, back on the main thread.
// ─────────────────────────────────────────────────────────────────────────────
void StressAnalysisController::runDataset(const QString& solver)
{
    if (m_isRunning) return;
    if (!Parameters::voxels) {
        setError(tr("No structure generated — press START first"));
        return;
    }

    const bool   ansys      = isAnsys(solver);
    const short  numCubes   = (short) Parameters::instance()->getSize();
    const short  numPoints  = (short) Parameters::instance()->getPoints();
    int32_t***   voxels     = Parameters::voxels;   // snapshot the pointer -- START is disabled while running
    const int    numSamples = m_numSamples;
    const int    numCalib   = m_numCalib;
    const double strainVal  = m_strainVal;

    m_pendingDatasetFilename = Parameters::filename.length() ? Parameters::filename : "current_ls.hdf5";

    setRunning(true);

    QFuture<void> future = QtConcurrent::run([ansys, numCubes, numPoints, voxels, numSamples, numCalib, strainVal]() {
        if (ansys) {
            StressAnalysis sa;
            sa.num_samples = numSamples;
            sa.num_calib   = numCalib;
            sa.strain_val  = strainVal;
            sa.estimateStressWithANSYS(numCubes, numPoints, voxels);
        } else {
            StressAnalysisFFT sa;
            sa.num_samples = numSamples;
            sa.num_calib   = numCalib;
            sa.strain_val  = strainVal;
            sa.estimateStressWithFFT(numCubes, numPoints, voxels);
        }
    });
    m_datasetWatcher.setFuture(future);
}

void StressAnalysisController::onDatasetFinished()
{
    setRunning(false);

    // Dataset mode doesn't populate the single-shot result panel or the 3D
    // field view -- drop any stale single-shot field from a previous run.
    m_hasResult = false;
    m_lastResult = SingleShotResult{};
    if (OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance())
        ogl->clearFieldVisualization();
    if (m_showField) { m_showField = false; emit showFieldChanged(); }
    m_lastErrorMessage.clear();

    // Now back on the main thread: safe to touch the LoadStepManager singleton.
    LoadStepManager::getInstance().LoadFromHDF5(m_pendingDatasetFilename);

    emit resultChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
//  "Stiffness matrix" mode -- FFT or ANSYS, 6 canonical unit-strain solves,
//  no Hill calibration, no dataset build, no HDF5 write. Same threading
//  pattern as runSingleShot(): FFT gets a per-(load,iteration) callback for
//  the live convergence plot, marshalled back to the main thread.
// ─────────────────────────────────────────────────────────────────────────────
void StressAnalysisController::runStiffnessMatrix(const QString& solver)
{
    if (m_isRunning) return;
    if (!Parameters::voxels) {
        setError(tr("No structure generated — press START first"));
        return;
    }

    const bool   ansys     = isAnsys(solver);
    const short  numCubes  = (short) Parameters::instance()->getSize();
    const short  numPoints = (short) Parameters::instance()->getPoints();
    int32_t***   voxels    = Parameters::voxels;   // snapshot the pointer -- START is disabled while running
    const double strainVal = m_strainVal;

    resetConvergence(!ansys, 6);
    setRunning(true);

    QFuture<StiffnessMatrixResult> future = QtConcurrent::run([this, ansys, numCubes, numPoints, voxels, strainVal]() {
        if (ansys)
            return StressAnalysis().computeStiffnessMatrix(numCubes, numPoints, voxels, strainVal);

        auto onIter = [this](int loadIdx, int iter, double err) {
            QMetaObject::invokeMethod(this, [this, loadIdx, iter, err]() {
                appendConvergencePoint(loadIdx, iter, err);
            }, Qt::QueuedConnection);
        };
        return StressAnalysisFFT().computeStiffnessMatrix(numCubes, numPoints, voxels, onIter);
    });
    m_stiffnessWatcher.setFuture(future);
}

void StressAnalysisController::onStiffnessFinished()
{
    const StiffnessMatrixResult r = m_stiffnessWatcher.result();
    setRunning(false);

    m_lastStiffness = r;
    m_hasStiffness  = r.ok;
    if (!r.ok) {
        setError(r.errorMessage.isEmpty() ? tr("Stiffness matrix computation failed") : r.errorMessage);
    } else {
        m_lastErrorMessage.clear();
    }
    emit stiffnessChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Persist the last single-shot result as a one-load-step HDF5 dataset,
//  following the same schema estimateStressWithFFT/ANSYS already write.
// ─────────────────────────────────────────────────────────────────────────────
void StressAnalysisController::saveStiffnessResult()
{
    if (!m_hasStiffness || !m_lastStiffness.ok) {
        setError(tr("No stiffness matrix to save"));
        return;
    }

    const QString filename = Parameters::filename.length() ? Parameters::filename : "current_ls.hdf5";
    const QString solver   = m_lastStiffness.isFFT ? QStringLiteral("fft") : QStringLiteral("ansys");

    const QString group = saveStiffnessMatrixToHDF5(filename, m_lastStiffness, solver, Parameters::seed);
    if (group.isEmpty()) {
        setError(tr("Failed to write the stiffness matrix to %1").arg(filename));
        return;
    }

    qDebug() << "[StressAnalysisController] Saved stiffness matrix to" << filename << group;
    m_lastErrorMessage.clear();
    emit savedToHDF5(filename);
}

void StressAnalysisController::saveSingleShotResult()
{
    if (!canSave()) {
        setError(tr("No single-shot result to save"));
        return;
    }

    const QString filename = Parameters::filename.length() ? Parameters::filename : "current_ls.hdf5";
    HDF5Wrapper hdf5(filename.toStdString());

    int last_set = hdf5.readInt("/", "last_set");
    if (last_set == -1) { last_set = 1; hdf5.write("/", "last_set", last_set); }
    else                { last_set += 1; hdf5.update("/", "last_set", last_set); }
    const std::string prefix = ("/" + QString::number(last_set)).toStdString();

    hdf5.write(prefix, "voxels",    Parameters::voxels, Parameters::instance()->getSize());
    hdf5.write(prefix, "cubeSize",  Parameters::instance()->getSize());
    hdf5.write(prefix, "numPoints", Parameters::instance()->getPoints());

    // Single-shot only tracks the macro (volume-average) stress -- so the
    // one-row "results" table and its max/min are identical to the average.
    std::vector<float> avg(EpsEQV + 1, 0.0f);
    avg[SX]  = float(m_lastResult.macro_stress[0]);
    avg[SY]  = float(m_lastResult.macro_stress[1]);
    avg[SZ]  = float(m_lastResult.macro_stress[2]);
    avg[SXY] = float(m_lastResult.macro_stress[3]);
    avg[SYZ] = float(m_lastResult.macro_stress[4]);
    avg[SXZ] = float(m_lastResult.macro_stress[5]);
    avg[SEQV] = float(m_lastResult.von_mises);

    std::vector<std::vector<float>> results{avg};
    std::vector<float> eps_load(m_lastEps, m_lastEps + 6);

    const std::string ls_str = prefix + "/ls_1";
    hdf5.write(ls_str, "results",        results);
    hdf5.write(ls_str, "results_avg",    avg);
    hdf5.write(ls_str, "results_max",    avg);
    hdf5.write(ls_str, "results_min",    avg);
    hdf5.write(ls_str, "eps_as_loading", eps_load);

    qDebug() << "[StressAnalysisController] Saved single-shot result to" << filename << prefix.c_str();

    LoadStepManager::getInstance().LoadFromHDF5(filename);
    emit savedToHDF5(filename);
}

bool StressAnalysisController::loadFromHDF5(const QString& filePath)
{
    if (filePath.isEmpty()) return false;

    LoadStepManager& lsm = LoadStepManager::getInstance();
    lsm.LoadFromHDF5(filePath);

    HDF5Wrapper hdf5(filePath.toStdString());
    int last_set = hdf5.readInt("/", "last_set");
    if (last_set < 1) last_set = 1;

    std::string prefix = "/" + std::to_string(last_set);
    if (hdf5.datasetExists(prefix, "C_matrix")) {
        auto mat_C = hdf5.readVectorVectorFloat(prefix, "C_matrix");
        auto mat_S = hdf5.readVectorVectorFloat(prefix, "S_matrix");
        auto mat_P = hdf5.readVectorVectorFloat(prefix, "P_matrix");
        auto moduli = hdf5.readVectorFloat(prefix, "Effective_Moduli");

        if (mat_C.size() == 6 && mat_S.size() == 6 && mat_P.size() == 6) {
            for (int i = 0; i < 6; ++i) {
                for (int j = 0; j < 6; ++j) {
                    m_lastStiffness.C[i][j] = mat_C[i][j];
                    m_lastStiffness.S[i][j] = mat_S[i][j];
                    m_lastStiffness.P[i][j] = mat_P[i][j];
                }
            }
            if (moduli.size() >= 6) {
                for (int i = 0; i < 6; ++i) m_lastStiffness.moduli[i] = moduli[i];
            }
            m_lastStiffness.isFFT = (hdf5.readQString(prefix, "solver").compare("fft", Qt::CaseInsensitive) == 0);
            m_lastStiffness.totalIterations = hdf5.readInt(prefix, "iterations_total");
            m_lastStiffness.ok = true;
            m_hasStiffness = true;
            emit stiffnessChanged();
            qDebug() << "[StressAnalysisController] Loaded stiffness matrix from HDF5:" << filePath;
        }
    }

    if (lsm.hasLoadStepData()) {
        const auto& results = lsm.getLoadStepResults();
        const int numCubes = (int)Parameters::instance()->getSize();
        const size_t expectedVoxelCount = static_cast<size_t>(numCubes) * numCubes * numCubes;
        const bool isPerVoxel = (results.size() == expectedVoxelCount);

        m_lastResult = SingleShotResult{};
        m_lastResult.ok = true;

        if (isPerVoxel) {
            auto field = buildFieldFromResults(numCubes, results, lsm.getEpsAsLoading());
            m_lastResult.fftField = field;
        } else {
            auto wr = std::make_shared<ansysWrapper>(true, false);
            wr->local_cs = lsm.getLocalCS();
            wr->loadstep_results = lsm.getLoadStepResults();
            wr->loadstep_results_avg = lsm.getLoadStepResultsAvg();
            wr->loadstep_results_max = lsm.getLoadStepResultsMax();
            wr->loadstep_results_min = lsm.getLoadStepResultsMin();
            const auto& eps_load = lsm.getEpsAsLoading();
            if (!eps_load.empty()) {
                wr->eps_as_loading = {eps_load};
                for (size_t i = 0; i < std::min<size_t>(6, eps_load.size()); ++i) {
                    m_lastEps[i] = eps_load[i];
                }
            }
            wr->createResultNodesHash();
            m_lastResult.ansysField = wr;
        }

        const auto& avg = lsm.getLoadStepResultsAvg();
        if (!avg.empty() && SEQV < (int)avg.size()) {
            m_lastResult.von_mises = avg[SEQV];
            if (SX < (int)avg.size())  m_lastResult.macro_stress[0] = avg[SX];
            if (SY < (int)avg.size())  m_lastResult.macro_stress[1] = avg[SY];
            if (SZ < (int)avg.size())  m_lastResult.macro_stress[2] = avg[SZ];
            if (SXY < (int)avg.size()) m_lastResult.macro_stress[3] = avg[SXY];
            if (SYZ < (int)avg.size()) m_lastResult.macro_stress[4] = avg[SYZ];
            if (SXZ < (int)avg.size()) m_lastResult.macro_stress[5] = avg[SXZ];
        }

        m_hasResult = true;
        pushResultToView();
        emit resultChanged();
        qDebug() << "[StressAnalysisController] Loaded stress/strain field visualization from HDF5:" << filePath;
    }

    return true;
}

void StressAnalysisController::openHDF5File()
{
    QString fileName = QFileDialog::getOpenFileName(
        nullptr, tr("Open HDF5 Result"), "",
        tr("HDF5 Files (*.h5 *.hdf5 *.hdf);;All Files (*.*)"));
    if (!fileName.isEmpty()) {
        loadFromHDF5(fileName);
    }
}

void StressAnalysisController::updateFromWrapper(const std::shared_ptr<ansysWrapper>& wr)
{
    if (!wr) return;
    m_lastResult = SingleShotResult{};
    m_lastResult.ansysField = wr;
    m_lastResult.ok = true;
    if (!wr->loadstep_results_avg.empty() && SEQV < (int)wr->loadstep_results_avg.size()) {
        m_lastResult.von_mises = wr->loadstep_results_avg[SEQV];
        if (SX < (int)wr->loadstep_results_avg.size())  m_lastResult.macro_stress[0] = wr->loadstep_results_avg[SX];
        if (SY < (int)wr->loadstep_results_avg.size())  m_lastResult.macro_stress[1] = wr->loadstep_results_avg[SY];
        if (SZ < (int)wr->loadstep_results_avg.size())  m_lastResult.macro_stress[2] = wr->loadstep_results_avg[SZ];
        if (SXY < (int)wr->loadstep_results_avg.size()) m_lastResult.macro_stress[3] = wr->loadstep_results_avg[SXY];
        if (SYZ < (int)wr->loadstep_results_avg.size()) m_lastResult.macro_stress[4] = wr->loadstep_results_avg[SYZ];
        if (SXZ < (int)wr->loadstep_results_avg.size()) m_lastResult.macro_stress[5] = wr->loadstep_results_avg[SXZ];
    }
    m_hasResult = true;
    emit resultChanged();
    emit fieldComponentChanged();
}

void StressAnalysisController::updateFromFFT(const std::shared_ptr<FieldVisualizationData>& field,
                                             const std::vector<float>& avg,
                                             double vonMises)
{
    if (!field) return;
    m_lastResult = SingleShotResult{};
    m_lastResult.fftField = field;
    m_lastResult.ok = true;
    m_lastResult.von_mises = vonMises;
    if (!avg.empty()) {
        if (m_lastResult.von_mises <= 0.0 && SEQV < (int)avg.size()) {
            m_lastResult.von_mises = avg[SEQV];
        }
        if (SX < (int)avg.size())  m_lastResult.macro_stress[0] = avg[SX];
        if (SY < (int)avg.size())  m_lastResult.macro_stress[1] = avg[SY];
        if (SZ < (int)avg.size())  m_lastResult.macro_stress[2] = avg[SZ];
        if (SXY < (int)avg.size()) m_lastResult.macro_stress[3] = avg[SXY];
        if (SYZ < (int)avg.size()) m_lastResult.macro_stress[4] = avg[SYZ];
        if (SXZ < (int)avg.size()) m_lastResult.macro_stress[5] = avg[SXZ];
    }
    m_hasResult = true;
    emit resultChanged();
    emit fieldComponentChanged();
}

void StressAnalysisController::clearResult()
{
    m_lastResult = SingleShotResult{};
    m_hasResult = false;
    m_showField = false;
    m_showDeformed = false;
    emit resultChanged();
    emit showFieldChanged();
    emit showDeformedChanged();
}


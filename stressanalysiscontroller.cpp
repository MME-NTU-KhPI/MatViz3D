#include "stressanalysiscontroller.h"
#include "stressanalysis.h"
#include "stressanalysis_fft.h"
#include "parameters.h"
#include "loadstepmanager.h"
#include "hdf5wrapper.h"
#include "ansyswrapper.h"   // tensor_components enum (SX..SXZ, SEQV) -- shares layout with fftsa::ResCol
#include "openglwidgetqml.h"

#include <QDebug>
#include <cmath>
#include <algorithm>

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
//  Single known load case -- FFT or ANSYS, no HDF5 write.
// ─────────────────────────────────────────────────────────────────────────────
void StressAnalysisController::runSingleShot(const QString& solver, const QVariantList& eps)
{
    if (!Parameters::voxels) {
        setError(tr("No structure generated — press START first"));
        return;
    }
    if (eps.size() != 6) {
        setError(tr("Expected 6 strain tensor components, got %1").arg(eps.size()));
        return;
    }

    double e[6];
    for (int i = 0; i < 6; ++i) e[i] = eps.at(i).toDouble();

    const short numCubes  = (short) Parameters::instance()->getSize();
    const short numPoints = (short) Parameters::instance()->getPoints();

    setRunning(true);

    SingleShotResult r = isAnsys(solver)
        ? StressAnalysis().solveSingleLoadCase(numCubes, numPoints, Parameters::voxels, e)
        : StressAnalysisFFT().solveSingleLoadCase(numCubes, numPoints, Parameters::voxels, e);

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
    for (int i = 0; i < 6; ++i) m_lastEps[i] = e[i];
    m_hasResult = true;
    m_lastErrorMessage.clear();
    pushResultToView();
    emit resultChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Full dataset-build pipeline -- FFT or ANSYS, writes its own HDF5 dataset.
// ─────────────────────────────────────────────────────────────────────────────
void StressAnalysisController::runDataset(const QString& solver)
{
    if (!Parameters::voxels) {
        setError(tr("No structure generated — press START first"));
        return;
    }

    const short numCubes  = (short) Parameters::instance()->getSize();
    const short numPoints = (short) Parameters::instance()->getPoints();

    setRunning(true);

    if (isAnsys(solver)) {
        StressAnalysis sa;
        sa.num_samples = m_numSamples;
        sa.num_calib   = m_numCalib;
        sa.strain_val  = m_strainVal;
        sa.estimateStressWithANSYS(numCubes, numPoints, Parameters::voxels);
    } else {
        StressAnalysisFFT sa;
        sa.num_samples = m_numSamples;
        sa.num_calib   = m_numCalib;
        sa.strain_val  = m_strainVal;
        sa.estimateStressWithFFT(numCubes, numPoints, Parameters::voxels);
    }

    setRunning(false);

    // Dataset mode doesn't populate the single-shot result panel or the 3D
    // field view -- drop any stale single-shot field from a previous run.
    m_hasResult = false;
    m_lastResult = SingleShotResult{};
    if (OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance())
        ogl->clearFieldVisualization();
    if (m_showField) { m_showField = false; emit showFieldChanged(); }
    m_lastErrorMessage.clear();
    emit resultChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Persist the last single-shot result as a one-load-step HDF5 dataset,
//  following the same schema estimateStressWithFFT/ANSYS already write.
// ─────────────────────────────────────────────────────────────────────────────
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

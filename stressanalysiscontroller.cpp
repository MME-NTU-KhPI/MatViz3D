#include "stressanalysiscontroller.h"
#include "stressanalysis.h"
#include "stressanalysis_fft.h"
#include "parameters.h"
#include "loadstepmanager.h"
#include "hdf5wrapper.h"
#include "ansyswrapper.h"   // tensor_components enum (SX..SXZ, SEQV) -- shares layout with fftsa::ResCol

#include <QDebug>

namespace {
bool isAnsys(const QString& solver) { return solver.compare("ansys", Qt::CaseInsensitive) == 0; }
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
        setError(r.errorMessage.isEmpty() ? tr("Solve failed") : r.errorMessage);
        emit resultChanged();
        return;
    }

    m_lastResult = r;
    for (int i = 0; i < 6; ++i) m_lastEps[i] = e[i];
    m_hasResult = true;
    m_lastErrorMessage.clear();
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

    // Dataset mode doesn't populate the single-shot result panel.
    m_hasResult = false;
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

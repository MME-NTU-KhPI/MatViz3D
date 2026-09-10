#include "hdf5projectcontroller.h"
#include "loadstepmanager.h"
#include "openglwidgetqml.h"
#include "parameters.h"
#include "parent_algorithm.h"
#include "ansyswrapper.h"
#include "stressresult.h"
#include "stressanalysiscontroller.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <cmath>

Hdf5ProjectController* Hdf5ProjectController::s_instance = nullptr;

Hdf5ProjectController::Hdf5ProjectController(QObject* parent)
    : QObject(parent)
{
    s_instance = this;
}

void Hdf5ProjectController::openFileDialog()
{
    QString fileName = QFileDialog::getOpenFileName(
        nullptr, tr("Open MatViz3D HDF5 Project"), "",
        tr("HDF5 Files (*.h5 *.hdf5 *.hdf);;All Files (*.*)"));
    if (!fileName.isEmpty()) {
        openFile(fileName);
    }
}

bool Hdf5ProjectController::openFile(const QString& filePath)
{
    if (filePath.isEmpty()) return false;

    LoadStepManager& lsm = LoadStepManager::getInstance();
    if (!lsm.LoadFromHDF5(filePath)) {
        qWarning() << "Hdf5ProjectController: failed to load" << filePath;
        return false;
    }

    m_filePath = filePath;
    m_isOpen = true;
    m_geomSets = lsm.getGeomSetList();
    m_currentGeomIndex = -1;
    m_syncedGeomIndex = -1;

    if (StressAnalysisController* sa = StressAnalysisController::getInstance()) {
        sa->setFieldComponentIndex(6); // default von Mises (SEQV)
        sa->setShowDeformed(true);
        sa->setShowField(true);
    }

    emit projectChanged();

    if (!m_geomSets.isEmpty()) {
        selectGeomSet(0);
    }

    qDebug() << "Hdf5ProjectController: opened project" << filePath << "with" << m_geomSets.size() << "geometry set(s)";
    return true;
}

QString Hdf5ProjectController::currentGeomName() const
{
    if (m_currentGeomIndex >= 0 && m_currentGeomIndex < m_geomSets.size())
        return m_geomSets[m_currentGeomIndex];
    return QString();
}

QString Hdf5ProjectController::currentLoadStepName() const
{
    if (m_currentLoadStepIndex >= 0 && m_currentLoadStepIndex < m_loadSteps.size())
        return m_loadSteps[m_currentLoadStepIndex];
    return QString();
}

void Hdf5ProjectController::selectGeomSet(int index)
{
    if (index < 0 || index >= m_geomSets.size()) return;
    m_currentGeomIndex = index;

    bool ok = false;
    int geomNum = m_geomSets[index].toInt(&ok);
    if (!ok) geomNum = index + 1;

    reloadGeometryMetadata(geomNum);

    LoadStepManager& lsm = LoadStepManager::getInstance();
    m_loadSteps = lsm.getGeomSetSubList();
    m_currentLoadStepIndex = -1;

    emit geomChanged();

    if (!m_loadSteps.isEmpty()) {
        selectLoadStep(0);
    } else {
        if (m_autoSync3D) {
            pushTo3DView();
        }
    }
}

void Hdf5ProjectController::reloadGeometryMetadata(int geomSetNum)
{
    LoadStepManager& lsm = LoadStepManager::getInstance();
    lsm.LoadGeomSet(geomSetNum);

    m_cubeSize  = lsm.getCubeSize();
    m_numPoints = lsm.getNumPoints();

    if (m_numPoints <= 0) {
        int32_t*** vox = lsm.getVoxelPtr();
        if (vox && m_cubeSize > 0) {
            int maxVal = 1;
            for (int i = 0; i < m_cubeSize; ++i)
                for (int j = 0; j < m_cubeSize; ++j)
                    for (int k = 0; k < m_cubeSize; ++k)
                        if (vox[i][j][k] > maxVal) maxVal = vox[i][j][k];
            m_numPoints = maxVal;
        } else {
            m_numPoints = 1;
        }
    }

    // Probe HDF5 for seed and stiffness matrices
    HDF5Wrapper hdf5(m_filePath.toStdString());
    std::string prefix = "/" + std::to_string(geomSetNum);

    m_seed = hdf5.readInt(prefix, "seed");
    if (m_seed == -1) m_seed = 0;

    // Read full geometry metadata (algorithm, seed, solver, parameters, JSON)
    m_geomMeta = readGeometryMetadataFromHDF5(hdf5, prefix);
    m_algorithm = m_geomMeta.algorithm;
    m_solver = m_geomMeta.solver;
    m_geomParamsSummary = m_geomMeta.summary;
    m_geomParams = m_geomMeta.parameters;
    if (m_geomMeta.seed != 0) {
        m_seed = m_geomMeta.seed;
    }

    m_hasStiffness = false;
    for (int i = 0; i < 6; ++i) {
        m_moduli[i] = 0.0;
        for (int j = 0; j < 6; ++j) {
            m_C[i][j] = 0.0;
            m_S[i][j] = 0.0;
        }
    }

    if (hdf5.datasetExists(prefix, "C_matrix")) {
        auto mat_C = hdf5.readVectorVectorFloat(prefix, "C_matrix");
        auto mat_S = hdf5.readVectorVectorFloat(prefix, "S_matrix");
        auto moduli = hdf5.readVectorFloat(prefix, "Effective_Moduli");

        if (mat_C.size() == 6 && mat_S.size() == 6) {
            for (int i = 0; i < 6; ++i) {
                for (int j = 0; j < 6; ++j) {
                    m_C[i][j] = mat_C[i][j];
                    m_S[i][j] = mat_S[i][j];
                }
            }
            if (moduli.size() >= 6) {
                for (int i = 0; i < 6; ++i) m_moduli[i] = moduli[i];
            }
            m_hasStiffness = true;
        }
    }
}

void Hdf5ProjectController::selectLoadStep(int index)
{
    if (index < 0 || index >= m_loadSteps.size()) return;
    m_currentLoadStepIndex = index;

    const QString& lsName = m_loadSteps[index];
    int stepNum = 1;
    if (lsName.startsWith(QStringLiteral("ls_"))) {
        stepNum = lsName.mid(3).toInt();
    } else {
        stepNum = index + 1;
    }

    bool ok = false;
    int geomNum = m_geomSets[m_currentGeomIndex].toInt(&ok);
    if (!ok) geomNum = m_currentGeomIndex + 1;

    reloadLoadStepData(geomNum, stepNum);

    if (m_autoSync3D) {
        pushTo3DView();
    }

    emit loadStepChanged();
}

void Hdf5ProjectController::reloadLoadStepData(int geomSetNum, int subSetNum)
{
    LoadStepManager& lsm = LoadStepManager::getInstance();
    lsm.LoadGeomSubStep(geomSetNum, subSetNum);

    m_appliedStrain = lsm.getEpsAsLoading();
    m_macroStress   = lsm.getLoadStepResultsAvg();

    if (SEQV < (int)m_macroStress.size()) {
        m_macroVonMises = m_macroStress[SEQV];
    } else {
        m_macroVonMises = 0.0;
    }

    recomputeStatistics();
}

void Hdf5ProjectController::nextStep()
{
    if (m_currentLoadStepIndex + 1 < m_loadSteps.size()) {
        selectLoadStep(m_currentLoadStepIndex + 1);
    }
}

void Hdf5ProjectController::prevStep()
{
    if (m_currentLoadStepIndex > 0) {
        selectLoadStep(m_currentLoadStepIndex - 1);
    }
}

void Hdf5ProjectController::setAutoSync3D(bool autoSync)
{
    if (m_autoSync3D == autoSync) return;
    m_autoSync3D = autoSync;
    emit autoSync3DChanged();
    if (m_autoSync3D) {
        pushTo3DView();
    }
}

void Hdf5ProjectController::pushTo3DView()
{
    LoadStepManager& lsm = LoadStepManager::getInstance();
    if (!lsm.isValid() || m_cubeSize <= 0) return;

    OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance();
    if (!ogl) return;

    // 1. Sync geometry (microstructure voxels & grain orientations) ONLY when
    //    geometry set changes or voxels are not yet uploaded into OpenGLWidgetQML.
    const bool needGeomSync = (m_syncedGeomIndex != m_currentGeomIndex)
                           || (Parameters::voxels == nullptr)
                           || (ogl->getVoxels() == nullptr)
                           || (Parameters::instance()->getSize() != m_cubeSize);

    if (needGeomSync) {
        int32_t*** vox = lsm.getVoxelPtr();
        if (!vox) return;

        Parameters* p = Parameters::instance();
        p->setSize(m_cubeSize);
        p->setPoints(m_numPoints);
        Parameters::filename = m_filePath;
        Parameters::phaseAssignment.clear();

        if (Parameters::voxels) {
            Parent_Algorithm::Delete3D<int32_t>(Parameters::voxels);
            Parameters::voxels = nullptr;
        }
        Parameters::voxels = Parent_Algorithm::Create3D<int32_t>(m_cubeSize, m_cubeSize, m_cubeSize);
        for (int i = 0; i < m_cubeSize; i++)
            for (int j = 0; j < m_cubeSize; j++)
                for (int k = 0; k < m_cubeSize; k++)
                    Parameters::voxels[i][j][k] = vox[i][j][k];

        ogl->setNumColors(m_numPoints);
        ogl->setVoxels(Parameters::voxels, m_cubeSize);

        // Sync grain orientations
        const auto& local_cs = lsm.getLocalCS();
        if (!local_cs.empty()) {
            std::vector<std::array<float, 3>> orientations;
            orientations.reserve(local_cs.size());
            for (const auto& row : local_cs) {
                if (row.size() >= 3) {
                    orientations.push_back({row[0], row[1], row[2]});
                }
            }
            ogl->setGrainOrientations(orientations);
        } else {
            ogl->setGrainOrientations({});
        }

        m_syncedGeomIndex = m_currentGeomIndex;
    }

    // 2. Push stress/strain/displacement field from load step results
    if (lsm.hasLoadStepData()) {
        const auto& results = lsm.getLoadStepResults();
        const size_t expectedVoxelCount = static_cast<size_t>(m_cubeSize) * m_cubeSize * m_cubeSize;
        const bool isPerVoxel = (results.size() == expectedVoxelCount);

        StressAnalysisController* sa = StressAnalysisController::getInstance();
        const bool deformed = sa ? sa->showDeformed() : true;
        const int comp = (sa && sa->currentComponentEnum() >= 0) ? sa->currentComponentEnum() : SEQV;

        if (isPerVoxel) {
            auto field = buildFieldFromResults(m_cubeSize, results, lsm.getEpsAsLoading());
            double epsNorm = 0.0;
            for (int i = 0; i < 6; ++i) epsNorm += field->macroStrain[i] * field->macroStrain[i];
            double charDisp = std::max(1e-12, std::sqrt(epsNorm) * m_cubeSize);
            float targetScale = float(0.15 * m_cubeSize / charDisp);

            if (needGeomSync || (sa && sa->deformedScale() <= 0.0)) {
                if (sa) sa->setDeformedScale(targetScale);
            }
            const float scale = (sa && sa->deformedScale() > 0.0) ? float(sa->deformedScale()) : targetScale;

            ogl->showFFTField(field, comp, deformed, scale);

            if (sa) {
                sa->updateFromFFT(field, lsm.getLoadStepResultsAvg(), m_macroVonMises);
                sa->syncFieldState(true, deformed, scale);
            }
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
            }
            wr->createResultNodesHash();

            double charDisp = 1e-12;
            if (USUM < (int)wr->loadstep_results_max.size() && USUM < (int)wr->loadstep_results_min.size()) {
                charDisp = std::max(charDisp, double(wr->loadstep_results_max[USUM] - wr->loadstep_results_min[USUM]));
            }
            float targetScale = float(0.15 * m_cubeSize / charDisp);

            if (needGeomSync || (sa && sa->deformedScale() <= 0.0)) {
                if (sa) sa->setDeformedScale(targetScale);
            }
            const float scale = (sa && sa->deformedScale() > 0.0) ? float(sa->deformedScale()) : targetScale;

            ogl->showAnsysField(wr, comp, deformed, scale);

            if (sa) {
                sa->updateFromWrapper(wr);
                sa->syncFieldState(true, deformed, scale);
            }
        }
    } else {
        ogl->clearFieldVisualization();
        if (StressAnalysisController* sa = StressAnalysisController::getInstance()) {
            sa->clearResult();
            sa->syncFieldState(false, false, 1.0);
        }
    }

    ogl->update();

    qDebug() << "Hdf5ProjectController: synchronized 3D viewport with geom" << currentGeomName() << "step" << currentLoadStepName();
}

QVariantList Hdf5ProjectController::stiffnessModuli() const
{
    QVariantList list;
    for (int i = 0; i < 6; ++i) list.append(m_moduli[i]);
    return list;
}

QVariantList Hdf5ProjectController::stiffnessC() const
{
    QVariantList rows;
    for (int i = 0; i < 6; ++i) {
        QVariantList row;
        for (int j = 0; j < 6; ++j) row.append(m_C[i][j]);
        rows.append(QVariant(row));
    }
    return rows;
}

QVariantList Hdf5ProjectController::stiffnessS() const
{
    QVariantList rows;
    for (int i = 0; i < 6; ++i) {
        QVariantList row;
        for (int j = 0; j < 6; ++j) row.append(m_S[i][j]);
        rows.append(QVariant(row));
    }
    return rows;
}

QVariantList Hdf5ProjectController::appliedStrain() const
{
    QVariantList list;
    for (float v : m_appliedStrain) list.append(v);
    return list;
}

QVariantList Hdf5ProjectController::macroStress() const
{
    QVariantList list;
    for (float v : m_macroStress) list.append(v);
    return list;
}

QStringList Hdf5ProjectController::availableProperties() const
{
    return DeformedStateAnalyzer::availableProperties();
}

QString Hdf5ProjectController::selectedPropertyName() const
{
    auto props = availableProperties();
    if (m_selectedPropertyIndex >= 0 && m_selectedPropertyIndex < props.size())
        return props[m_selectedPropertyIndex];
    return QStringLiteral("von Mises Stress");
}

QString Hdf5ProjectController::statMode() const
{
    return (m_statMode == DeformedStateAnalyzer::StatMode::PerGrainMean)
               ? QStringLiteral("PerGrain") : QStringLiteral("FullVolume");
}

void Hdf5ProjectController::selectProperty(int index)
{
    if (index < 0 || index >= availableProperties().size()) return;
    if (m_selectedPropertyIndex == index) return;
    m_selectedPropertyIndex = index;
    emit propertyChanged();
    recomputeStatistics();
}

void Hdf5ProjectController::setStatMode(const QString& mode)
{
    DeformedStateAnalyzer::StatMode newMode = (mode.compare("PerGrain", Qt::CaseInsensitive) == 0)
                                                 ? DeformedStateAnalyzer::StatMode::PerGrainMean
                                                 : DeformedStateAnalyzer::StatMode::FullVolume;
    if (m_statMode == newMode) return;
    m_statMode = newMode;
    emit statModeChanged();
    recomputeStatistics();
}

void Hdf5ProjectController::setBinCount(int count)
{
    if (count <= 0 || m_binCount == count) return;
    m_binCount = count;
    emit binCountChanged();
    recomputeStatistics();
}

void Hdf5ProjectController::recomputeStatistics()
{
    LoadStepManager& lsm = LoadStepManager::getInstance();
    const auto& results = lsm.getLoadStepResults();

    if (results.empty()) {
        m_extractedValues.clear();
        m_descStats = DeformedStateAnalyzer::DescriptiveStats{};
        m_histResult = DeformedStateAnalyzer::HistogramResult{};
        m_descStatsList.clear();
        m_chartTitle.clear();
        m_axisXLabel.clear();
        emit statsChanged();
        return;
    }

    auto prop = static_cast<DeformedStateAnalyzer::Property>(m_selectedPropertyIndex);
    m_extractedValues = DeformedStateAnalyzer::extractValues(
        results, lsm.getVoxelPtr(), m_cubeSize, prop, m_statMode);

    m_descStats = DeformedStateAnalyzer::computeStats(m_extractedValues);
    m_histResult = DeformedStateAnalyzer::computeHistogramAndKDE(m_extractedValues, m_binCount);
    m_descStatsList = DeformedStateAnalyzer::statsToVariantList(m_descStats, prop);

    m_chartTitle = DeformedStateAnalyzer::propertyTitle(prop);
    QString unit = DeformedStateAnalyzer::propertyUnits(prop);
    m_axisXLabel = DeformedStateAnalyzer::propertyLabel(prop) + (unit.isEmpty() ? QString() : (" [" + unit + "]"));

    emit statsChanged();
}

void Hdf5ProjectController::exportCSV(const QString& filePath)
{
    LoadStepManager& lsm = LoadStepManager::getInstance();
    DeformedStateAnalyzer::exportToCsv(filePath, lsm.getLoadStepResults(), lsm.getVoxelPtr(), m_cubeSize);
}

QString Hdf5ProjectController::toLocalFile(const QUrl& fileUrl) const
{
    return fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
}

bool Hdf5ProjectController::exportSvg(const QUrl& fileUrl, bool dark, bool withStats)
{
    if (!hasData()) {
        qWarning() << "Hdf5ProjectController::exportSvg: no data to export";
        return false;
    }

    const QString path = toLocalFile(fileUrl);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Hdf5ProjectController::exportSvg: cannot write" << path;
        return false;
    }

    QString svg = DeformedStateAnalyzer::generateSvg(
        m_chartTitle, m_axisXLabel, m_histResult, m_descStats, dark, withStats);

    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << svg;
    f.close();

    qDebug() << "Hdf5ProjectController: wrote SVG to" << path;
    return true;
}

bool Hdf5ProjectController::hasVoxels() const
{
    return LoadStepManager::getInstance().hasVoxels();
}

bool Hdf5ProjectController::reproduceGeometry()
{
    if (m_geomMeta.algorithm.isEmpty() && m_geomParams.isEmpty()) {
        qWarning() << "Hdf5ProjectController: No geometry metadata available to reproduce";
        return false;
    }
    return applyGeometryMetadataToParameters(m_geomMeta);
}

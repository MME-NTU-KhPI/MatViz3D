#ifndef HDF5PROJECTCONTROLLER_H
#define HDF5PROJECTCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QUrl>
#include <vector>
#include "deformed_state_analyzer.h"
#include "hdf5wrapper.h"
#include "stressresult.h"

class Hdf5ProjectController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString     filePath              READ filePath              NOTIFY projectChanged)
    Q_PROPERTY(bool        isOpen                READ isOpen                NOTIFY projectChanged)
    Q_PROPERTY(QStringList geomSets              READ geomSets              NOTIFY projectChanged)
    Q_PROPERTY(int         currentGeomIndex      READ currentGeomIndex      WRITE selectGeomSet      NOTIFY geomChanged)
    Q_PROPERTY(QString     currentGeomName       READ currentGeomName       NOTIFY geomChanged)
    Q_PROPERTY(QStringList loadSteps             READ loadSteps             NOTIFY geomChanged)
    Q_PROPERTY(int         currentLoadStepIndex  READ currentLoadStepIndex  WRITE selectLoadStep     NOTIFY loadStepChanged)
    Q_PROPERTY(QString     currentLoadStepName   READ currentLoadStepName   NOTIFY loadStepChanged)
    Q_PROPERTY(int         cubeSize              READ cubeSize              NOTIFY geomChanged)
    Q_PROPERTY(int         numPoints             READ numPoints             NOTIFY geomChanged)
    Q_PROPERTY(int         seed                  READ seed                  NOTIFY geomChanged)
    Q_PROPERTY(QString     algorithm             READ algorithm             NOTIFY geomChanged)
    Q_PROPERTY(QString     solver                READ solver                NOTIFY geomChanged)
    Q_PROPERTY(QString     geomParamsSummary     READ geomParamsSummary     NOTIFY geomChanged)
    Q_PROPERTY(QVariantMap geomParams            READ geomParams            NOTIFY geomChanged)
    Q_PROPERTY(bool        hasVoxels             READ hasVoxels             NOTIFY geomChanged)
    Q_PROPERTY(bool        hasStiffness          READ hasStiffness          NOTIFY geomChanged)
    Q_PROPERTY(QVariantList stiffnessModuli      READ stiffnessModuli      NOTIFY geomChanged)
    Q_PROPERTY(QVariantList stiffnessC           READ stiffnessC           NOTIFY geomChanged)
    Q_PROPERTY(QVariantList stiffnessS           READ stiffnessS           NOTIFY geomChanged)
    Q_PROPERTY(QVariantList appliedStrain        READ appliedStrain        NOTIFY loadStepChanged)
    Q_PROPERTY(QVariantList macroStress          READ macroStress          NOTIFY loadStepChanged)
    Q_PROPERTY(double      macroVonMises         READ macroVonMises         NOTIFY loadStepChanged)
    Q_PROPERTY(bool        autoSync3D            READ autoSync3D            WRITE setAutoSync3D      NOTIFY autoSync3DChanged)

    // Statistics properties
    Q_PROPERTY(QStringList  availableProperties   READ availableProperties   NOTIFY modeChanged)
    Q_PROPERTY(int          selectedPropertyIndex READ selectedPropertyIndex WRITE selectProperty   NOTIFY propertyChanged)
    Q_PROPERTY(QString      selectedPropertyName  READ selectedPropertyName  NOTIFY propertyChanged)
    Q_PROPERTY(QString      statMode              READ statMode              WRITE setStatMode        NOTIFY statModeChanged)
    Q_PROPERTY(int          binCount              READ binCount              WRITE setBinCount        NOTIFY binCountChanged)
    Q_PROPERTY(QVariantList histogramPoints       READ histogramPoints       NOTIFY statsChanged)
    Q_PROPERTY(QVariantList kdePoints             READ kdePoints             NOTIFY statsChanged)
    Q_PROPERTY(QVariantList descriptiveStats      READ descriptiveStats      NOTIFY statsChanged)
    Q_PROPERTY(double       axisXMin              READ axisXMin              NOTIFY statsChanged)
    Q_PROPERTY(double       axisXMax              READ axisXMax              NOTIFY statsChanged)
    Q_PROPERTY(int          axisYMax              READ axisYMax              NOTIFY statsChanged)
    Q_PROPERTY(int          histogramPeak         READ histogramPeak         NOTIFY statsChanged)
    Q_PROPERTY(double       kdeMax                READ kdeMax                NOTIFY statsChanged)
    Q_PROPERTY(bool         hasData               READ hasData               NOTIFY statsChanged)
    Q_PROPERTY(QString      chartTitle            READ chartTitle            NOTIFY statsChanged)
    Q_PROPERTY(QString      axisXLabel            READ axisXLabel            NOTIFY statsChanged)

public:
    explicit Hdf5ProjectController(QObject* parent = nullptr);
    static Hdf5ProjectController* getInstance() { return s_instance; }

    Q_INVOKABLE void openFileDialog();
    Q_INVOKABLE bool openFile(const QString& filePath);
    Q_INVOKABLE void selectGeomSet(int index);
    Q_INVOKABLE void selectGeometry(int index) { selectGeomSet(index); }
    Q_INVOKABLE void selectLoadStep(int index);
    Q_INVOKABLE void nextStep();
    Q_INVOKABLE void prevStep();
    Q_INVOKABLE void selectProperty(int index);
    Q_INVOKABLE void setStatMode(const QString& mode);
    Q_INVOKABLE void setBinCount(int count);
    Q_INVOKABLE void setAutoSync3D(bool autoSync);
    Q_INVOKABLE void pushTo3DView();
    Q_INVOKABLE void exportCSV(const QString& filePath);
    Q_INVOKABLE bool exportSvg(const QUrl& fileUrl, bool dark, bool withStats);
    Q_INVOKABLE QString toLocalFile(const QUrl& fileUrl) const;
    Q_INVOKABLE bool reproduceGeometry();

    QString     filePath() const { return m_filePath; }
    bool        isOpen() const { return m_isOpen; }
    QStringList geomSets() const { return m_geomSets; }
    int         currentGeomIndex() const { return m_currentGeomIndex; }
    QString     currentGeomName() const;
    QStringList loadSteps() const { return m_loadSteps; }
    int         currentLoadStepIndex() const { return m_currentLoadStepIndex; }
    QString     currentLoadStepName() const;
    int         cubeSize() const { return m_cubeSize; }
    int         numPoints() const { return m_numPoints; }
    int         seed() const { return m_seed; }
    QString     algorithm() const { return m_algorithm; }
    QString     solver() const { return m_solver; }
    QString     geomParamsSummary() const { return m_geomParamsSummary; }
    QVariantMap geomParams() const { return m_geomParams; }
    bool        hasVoxels() const;
    bool        hasStiffness() const { return m_hasStiffness; }
    QVariantList stiffnessModuli() const;
    QVariantList stiffnessC() const;
    QVariantList stiffnessS() const;
    QVariantList appliedStrain() const;
    QVariantList macroStress() const;
    double      macroVonMises() const { return m_macroVonMises; }
    bool        autoSync3D() const { return m_autoSync3D; }

    QStringList  availableProperties() const;
    int          selectedPropertyIndex() const { return m_selectedPropertyIndex; }
    QString      selectedPropertyName() const;
    QString      statMode() const;
    int          binCount() const { return m_binCount; }
    QVariantList histogramPoints() const { return m_histResult.barPoints; }
    QVariantList kdePoints() const { return m_histResult.kdePoints; }
    QVariantList descriptiveStats() const { return m_descStatsList; }
    double       axisXMin() const { return m_histResult.axisXMin; }
    double       axisXMax() const { return m_histResult.axisXMax; }
    int          axisYMax() const { return m_histResult.axisYMax; }
    int          histogramPeak() const { return m_histResult.histPeak; }
    double       kdeMax() const { return m_histResult.kdeMax; }
    bool         hasData() const { return !m_histResult.barPoints.isEmpty(); }
    QString      chartTitle() const { return m_chartTitle; }
    QString      axisXLabel() const { return m_axisXLabel; }

signals:
    void projectChanged();
    void geomChanged();
    void loadStepChanged();
    void autoSync3DChanged();
    void modeChanged();
    void propertyChanged();
    void statModeChanged();
    void binCountChanged();
    void statsChanged();

private:
    void reloadGeometryMetadata(int geomSetNum);
    void reloadLoadStepData(int geomSetNum, int subSetNum);
    void recomputeStatistics();

    static Hdf5ProjectController* s_instance;

    QString     m_filePath;
    bool        m_isOpen = false;
    QStringList m_geomSets;
    int         m_currentGeomIndex = -1;
    int         m_syncedGeomIndex = -1;
    QStringList m_loadSteps;
    int         m_currentLoadStepIndex = -1;

    int         m_cubeSize  = 0;
    int         m_numPoints = 0;
    int         m_seed      = 0;
    QString     m_algorithm;
    QString     m_solver;
    QString     m_geomParamsSummary;
    QVariantMap m_geomParams;
    GeomMetadata m_geomMeta;

    bool        m_hasStiffness = false;
    double      m_C[6][6] = {{0}};
    double      m_S[6][6] = {{0}};
    double      m_moduli[6] = {0};

    std::vector<float> m_appliedStrain;
    std::vector<float> m_macroStress;
    double             m_macroVonMises = 0.0;

    bool m_autoSync3D = true;

    // Statistics state
    int                                    m_selectedPropertyIndex = 0;
    DeformedStateAnalyzer::StatMode        m_statMode = DeformedStateAnalyzer::StatMode::FullVolume;
    int                                    m_binCount = 30;
    QVector<float>                         m_extractedValues;
    DeformedStateAnalyzer::DescriptiveStats m_descStats;
    DeformedStateAnalyzer::HistogramResult  m_histResult;
    QVariantList                           m_descStatsList;
    QString                                m_chartTitle;
    QString                                m_axisXLabel;
};

#endif // HDF5PROJECTCONTROLLER_H

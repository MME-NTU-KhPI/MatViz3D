#ifndef STRESSANALYSISCONTROLLER_H
#define STRESSANALYSISCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QFutureWatcher>
#include <vector>
#include "stressresult.h"

// Backs the "Stress Analysis" window (StressAnalysisView.qml). Lets the user
// pick a solver (FFT / ANSYS) and either solve one known load case (for a
// quick check or FFT-vs-ANSYS verification) or run the full dataset-build
// pipeline (StressAnalysisFFT::estimateStressWithFFT / StressAnalysis::estimateStressWithANSYS).
class StressAnalysisController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isRunning  READ isRunning  NOTIFY isRunningChanged)
    Q_PROPERTY(bool hasResult  READ hasResult  NOTIFY resultChanged)
    Q_PROPERTY(bool canSave    READ canSave    NOTIFY resultChanged)

    Q_PROPERTY(QVariantList resultStress     READ resultStress     NOTIFY resultChanged)
    Q_PROPERTY(double       resultVonMises   READ resultVonMises   NOTIFY resultChanged)
    Q_PROPERTY(int          resultIterations READ resultIterations NOTIFY resultChanged)
    Q_PROPERTY(double       resultError      READ resultError      NOTIFY resultChanged)

    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY errorChanged)

    Q_PROPERTY(int    numSamples READ numSamples WRITE setNumSamples NOTIFY numSamplesChanged)
    Q_PROPERTY(int    numCalib   READ numCalib   WRITE setNumCalib   NOTIFY numCalibChanged)
    Q_PROPERTY(double strainVal  READ strainVal  WRITE setStrainVal  NOTIFY strainValChanged)

    // 3D field visualization (stress/strain/displacement coloring + deformed shape).
    Q_PROPERTY(QStringList fieldComponents      READ fieldComponents                                 NOTIFY resultChanged)
    Q_PROPERTY(int         fieldComponentIndex  READ fieldComponentIndex WRITE setFieldComponentIndex NOTIFY fieldComponentChanged)
    Q_PROPERTY(double      fieldMin             READ fieldMin                                         NOTIFY fieldComponentChanged)
    Q_PROPERTY(double      fieldMax             READ fieldMax                                         NOTIFY fieldComponentChanged)
    Q_PROPERTY(bool        showDeformed         READ showDeformed        WRITE setShowDeformed        NOTIFY showDeformedChanged)
    Q_PROPERTY(double      deformedScale        READ deformedScale       WRITE setDeformedScale        NOTIFY deformedScaleChanged)
    // Toggles the 3D view between field coloring and plain grain coloring.
    // Unlike showDeformed/fieldComponentIndex, flipping this back on re-applies
    // the field from m_lastResult -- it doesn't require re-solving.
    Q_PROPERTY(bool        showField            READ showField           WRITE setShowField           NOTIFY showFieldChanged)

    // "Stiffness matrix" mode: quick-test S/C/P + effective moduli, without
    // Hill calibration or a full dataset build.
    Q_PROPERTY(bool         hasStiffness      READ hasStiffness      NOTIFY stiffnessChanged)
    Q_PROPERTY(bool         stiffnessIsFFT    READ stiffnessIsFFT    NOTIFY stiffnessChanged)
    Q_PROPERTY(QVariantList stiffnessS        READ stiffnessS        NOTIFY stiffnessChanged)
    Q_PROPERTY(QVariantList stiffnessC        READ stiffnessC        NOTIFY stiffnessChanged)
    Q_PROPERTY(QVariantList stiffnessP        READ stiffnessP        NOTIFY stiffnessChanged)
    Q_PROPERTY(QVariantList stiffnessModuli   READ stiffnessModuli   NOTIFY stiffnessChanged)
    Q_PROPERTY(int          stiffnessTotalIterations READ stiffnessTotalIterations NOTIFY stiffnessChanged)

    // Live FFT convergence (iteration -> equilibrium error), fed by whichever
    // FFT solve last ran -- the single-shot solve (one series, loadIndex 0)
    // or the stiffness-matrix solve (up to 6 series, one per canonical load).
    // Empty / convergenceIsFFT==false when the last run was ANSYS (no
    // per-iteration error to plot -- ANSYS is a direct FE solve).
    Q_PROPERTY(QVariantList convergencePoints    READ convergencePoints    NOTIFY convergenceChanged)
    Q_PROPERTY(bool         convergenceIsFFT     READ convergenceIsFFT     NOTIFY convergenceChanged)
    Q_PROPERTY(int          convergenceLoadCount READ convergenceLoadCount NOTIFY convergenceChanged)
    Q_PROPERTY(double       convergenceTol       READ convergenceTol       NOTIFY convergenceChanged)

public:
    explicit StressAnalysisController(QObject* parent = nullptr);
    static StressAnalysisController* getInstance() { return s_instance; }

    // solver: "fft" or "ansys" (case-insensitive).
    Q_INVOKABLE void runSingleShot(const QString& solver, const QVariantList& eps);
    Q_INVOKABLE void runDataset(const QString& solver);
    Q_INVOKABLE void runStiffnessMatrix(const QString& solver);
    Q_INVOKABLE void saveSingleShotResult();
    Q_INVOKABLE void saveStiffnessResult();
    Q_INVOKABLE bool loadFromHDF5(const QString& filePath);
    Q_INVOKABLE void openHDF5File();
    Q_INVOKABLE void clearResult();
    void updateFromWrapper(const std::shared_ptr<ansysWrapper>& wr);
    void updateFromFFT(const std::shared_ptr<FieldVisualizationData>& field, const std::vector<float>& avg, double vonMises);

    const StiffnessMatrixResult& lastStiffness() const { return m_lastStiffness; }

    bool   isRunning()  const { return m_isRunning; }
    bool   hasResult()  const { return m_hasResult; }
    bool   canSave()    const { return m_hasResult && m_lastResult.ok; }

    QVariantList resultStress()     const;
    double       resultVonMises()   const { return m_lastResult.von_mises; }
    int          resultIterations() const { return m_lastResult.iterations; }
    double       resultError()      const { return m_lastResult.error; }

    QString lastErrorMessage() const { return m_lastErrorMessage; }

    int    numSamples() const { return m_numSamples; }
    int    numCalib()   const { return m_numCalib; }
    double strainVal()  const { return m_strainVal; }

    Q_INVOKABLE void setNumSamples(int value);
    Q_INVOKABLE void setNumCalib(int value);
    Q_INVOKABLE void setStrainVal(double value);

    QStringList fieldComponents() const;
    int         fieldComponentIndex() const { return m_fieldComponentIndex; }
    Q_INVOKABLE void setFieldComponentIndex(int index);
    double      fieldMin() const;
    double      fieldMax() const;
    bool        showDeformed() const { return m_showDeformed; }
    Q_INVOKABLE void setShowDeformed(bool show);
    double      deformedScale() const { return m_deformedScale; }
    Q_INVOKABLE void setDeformedScale(double scale);
    bool        showField() const { return m_showField; }
    Q_INVOKABLE void setShowField(bool show);
    void syncFieldState(bool showField, bool showDeformed, double scale);
    int  currentComponentEnum() const;

    bool         hasStiffness()           const { return m_hasStiffness; }
    bool         stiffnessIsFFT()         const { return m_lastStiffness.isFFT; }
    QVariantList stiffnessS()             const { return matrixToVariant(m_lastStiffness.S); }
    QVariantList stiffnessC()             const { return matrixToVariant(m_lastStiffness.C); }
    QVariantList stiffnessP()             const { return matrixToVariant(m_lastStiffness.P); }
    QVariantList stiffnessModuli()        const;
    int          stiffnessTotalIterations() const { return m_lastStiffness.totalIterations; }

    QVariantList convergencePoints()    const;
    bool         convergenceIsFFT()     const { return m_convergenceIsFFT; }
    int          convergenceLoadCount() const { return m_convergenceLoadCount; }
    double       convergenceTol()       const { return m_convergenceTol; }

signals:
    void isRunningChanged();
    void resultChanged();
    void errorChanged();
    void numSamplesChanged();
    void numCalibChanged();
    void strainValChanged();
    void savedToHDF5(const QString& filePath);
    void fieldComponentChanged();
    void showDeformedChanged();
    void deformedScaleChanged();
    void showFieldChanged();
    void stiffnessChanged();
    void convergenceChanged();

private:
    void setRunning(bool running);
    void setError(const QString& message);
    // Pushes m_lastResult's field into the 3D view and picks a default
    // component/deformed-scale. Called after a successful runSingleShot().
    void pushResultToView();
    static QVariantList matrixToVariant(const double m[6][6]);

    // Runs on the main thread once the background solve finishes (queued via
    // QFutureWatcher::finished -- Qt marshals this back automatically since
    // both watchers live on the thread that constructed this controller).
    void onSingleShotFinished();
    void onDatasetFinished();
    void onStiffnessFinished();

    // Clears the convergence buffer and (re)tags it for a new FFT run.
    // loadCount is 1 for a single-shot solve, 6 for a stiffness-matrix run.
    void resetConvergence(bool isFFT, int loadCount);
    // Appends one (loadIndex, iteration, error) sample. Only ever called on
    // the main thread -- worker threads reach it via
    // QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection), which Qt
    // safely no-ops if this controller is destroyed before the call runs.
    void appendConvergencePoint(int loadIndex, int iteration, double error);

    static StressAnalysisController* s_instance;
    bool   m_isRunning = false;
    bool   m_hasResult = false;
    SingleShotResult m_lastResult;
    double m_lastEps[6] = {0};

    // solveSingleLoadCase()/estimateStressWith*() touch no GUI/QML state and
    // are safe to run off the main thread (verified: no OpenGLWidgetQML or
    // LoadStepManager access inside them). Only the *ANSYS/FFT dataset* path
    // used to call LoadStepManager::getInstance().LoadFromHDF5() internally --
    // that's been moved to onDatasetFinished() so the (unsynchronized)
    // singleton is only ever touched from the main thread.
    QFutureWatcher<SingleShotResult>      m_singleShotWatcher;
    QFutureWatcher<void>                  m_datasetWatcher;
    QFutureWatcher<StiffnessMatrixResult> m_stiffnessWatcher;
    QString                          m_pendingDatasetFilename;

    QString m_lastErrorMessage;

    int    m_numSamples = 300;
    int    m_numCalib   = 150;
    double m_strainVal  = 1e-04;

    int    m_fieldComponentIndex = 6; // SEQV (von Mises)
    bool   m_showDeformed        = false;
    double m_deformedScale       = 1.0;
    bool   m_showField           = false;

    bool                   m_hasStiffness = false;
    StiffnessMatrixResult  m_lastStiffness;

    // (loadIndex, iteration, error) samples for the live convergence plot,
    // in arrival order (loadIndex distinguishes the up-to-6 series when a
    // stiffness-matrix run is in progress).
    struct ConvergencePoint { int loadIndex; int iteration; double error; };
    std::vector<ConvergencePoint> m_convergence;
    bool   m_convergenceIsFFT     = false;
    int    m_convergenceLoadCount = 0;
    double m_convergenceTol       = 0.0;
};

#endif // STRESSANALYSISCONTROLLER_H

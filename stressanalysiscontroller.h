#ifndef STRESSANALYSISCONTROLLER_H
#define STRESSANALYSISCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>
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

public:
    explicit StressAnalysisController(QObject* parent = nullptr);

    // solver: "fft" or "ansys" (case-insensitive).
    Q_INVOKABLE void runSingleShot(const QString& solver, const QVariantList& eps);
    Q_INVOKABLE void runDataset(const QString& solver);
    Q_INVOKABLE void saveSingleShotResult();

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

private:
    void setRunning(bool running);
    void setError(const QString& message);
    // Maps m_fieldComponentIndex -> tensor_components enum value, using
    // whichever solver produced m_lastResult (ANSYS/FFT component lists differ).
    int  currentComponentEnum() const;
    // Pushes m_lastResult's field into the 3D view and picks a default
    // component/deformed-scale. Called after a successful runSingleShot().
    void pushResultToView();

    bool   m_isRunning = false;
    bool   m_hasResult = false;
    SingleShotResult m_lastResult;
    double m_lastEps[6] = {0};

    QString m_lastErrorMessage;

    int    m_numSamples = 300;
    int    m_numCalib   = 150;
    double m_strainVal  = 1e-04;

    int    m_fieldComponentIndex = 0;
    bool   m_showDeformed        = false;
    double m_deformedScale       = 1.0;
    bool   m_showField           = false;
};

#endif // STRESSANALYSISCONTROLLER_H

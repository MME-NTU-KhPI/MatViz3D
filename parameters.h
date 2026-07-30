#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <QObject>
#include <QString>
#include <vector>
#include "texturelibrary.h"

class Parameters : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int size READ getSize WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(int points READ getPoints WRITE setPoints NOTIFY pointsChanged)
    Q_PROPERTY(QString algorithm READ getAlgorithm WRITE setAlgorithm NOTIFY algorithmChanged)
    Q_PROPERTY(unsigned int seed READ getSeed WRITE setSeed NOTIFY seedChanged)
    Q_PROPERTY(QString filename READ getFilename WRITE setFilename NOTIFY filenameChanged)
    Q_PROPERTY(int num_threads READ getNumThreads WRITE setNumThreads NOTIFY numThreadsChanged)
    Q_PROPERTY(QString working_directory READ getWorkingDirectory WRITE setWorkingDirectory NOTIFY workingDirectoryChanged)
    Q_PROPERTY(float wave_coefficient READ getWaveCoefficient WRITE setWaveCoefficient NOTIFY waveCoefficientChanged)

    Q_PROPERTY(float halfaxis_a READ getHalfAxisA WRITE setHalfAxisA NOTIFY halfAxisAChanged)
    Q_PROPERTY(float halfaxis_b READ getHalfAxisB WRITE setHalfAxisB NOTIFY halfAxisBChanged)
    Q_PROPERTY(float halfaxis_c READ getHalfAxisC WRITE setHalfAxisC NOTIFY halfAxisCChanged)

    Q_PROPERTY(float orientation_angle_a READ getOrientationAngleA WRITE setOrientationAngleA NOTIFY orientationAngleAChanged)
    Q_PROPERTY(float orientation_angle_b READ getOrientationAngleB WRITE setOrientationAngleB NOTIFY orientationAngleBChanged)
    Q_PROPERTY(float orientation_angle_c READ getOrientationAngleC WRITE setOrientationAngleC NOTIFY orientationAngleCChanged)

    Q_PROPERTY(QString pointsMode READ getPointsMode WRITE setPointsMode NOTIFY pointsModeChanged)
    Q_PROPERTY(bool isAnimation READ getIsAnimation() WRITE setIsAnimation() NOTIFY isAnimationChanged)

    Q_PROPERTY(bool hasProbParameters READ getHasProbParameters WRITE setHasProbParameters NOTIFY hasProbParametersChanged)
    Q_PROPERTY(double ellipse_order READ getEllipseOrder WRITE setEllipseOrder NOTIFY ellipseOrderChanged)

    Q_PROPERTY(QString material  READ getMaterial  WRITE setMaterial  NOTIFY materialChanged)
    Q_PROPERTY(QString material1 READ getMaterial1 WRITE setMaterial1 NOTIFY material1Changed)
    Q_PROPERTY(QString material2 READ getMaterial2 WRITE setMaterial2 NOTIFY material2Changed)

    Q_PROPERTY(float wave_spread          READ getWaveSpread         WRITE setWaveSpread         NOTIFY waveSpreadChanged)
    Q_PROPERTY(float stefan_number        READ getStefanNumber       WRITE setStefanNumber       NOTIFY stefanNumberChanged)
    Q_PROPERTY(int   initial_nuclei_count READ getInitialNucleiCount WRITE setInitialNucleiCount NOTIFY initialNucleiCountChanged)
    Q_PROPERTY(int   num_rnd_loads        READ getNumRndLoads        WRITE setNumRndLoads        NOTIFY numRndLoadsChanged)

public:
    explicit Parameters(QObject* parent = nullptr);

    static Parameters* instance()
    {
        static Parameters instance;
        return &instance;
    }

    int getSize() const { return size; }
    Q_INVOKABLE void setSize(int value);

    int getPoints() const { return points; }
    Q_INVOKABLE void setPoints(int value);

    QString getAlgorithm() const { return algorithm; }
    Q_INVOKABLE void setAlgorithm(const QString& value);

    unsigned int getSeed() const { return seed; }
    Q_INVOKABLE void setSeed(unsigned int value);

    QString getFilename() const { return filename; }
    Q_INVOKABLE void setFilename(const QString& value);

    int getNumThreads() const { return num_threads; }
    Q_INVOKABLE void setNumThreads(int value);

    QString getWorkingDirectory() const { return working_directory; }
    Q_INVOKABLE void setWorkingDirectory(const QString& value);

    float getWaveCoefficient() const { return wave_coefficient; }
    Q_INVOKABLE void setWaveCoefficient(float value);

    float getHalfAxisA() const { return halfaxis_a; }
    Q_INVOKABLE void setHalfAxisA(float value);

    float getHalfAxisB() const { return halfaxis_b; }
    Q_INVOKABLE void setHalfAxisB(float value);

    float getHalfAxisC() const { return halfaxis_c; }
    Q_INVOKABLE void setHalfAxisC(float value);

    float getOrientationAngleA() const { return orientation_angle_a; }
    Q_INVOKABLE void setOrientationAngleA(float value);

    float getOrientationAngleB() const { return orientation_angle_b; }
    Q_INVOKABLE void setOrientationAngleB(float value);

    float getOrientationAngleC() const { return orientation_angle_c; }
    Q_INVOKABLE void setOrientationAngleC(float value);

    QString getPointsMode() const { return points_mode; }
    Q_INVOKABLE void setPointsMode(const QString& value);

    bool getIsAnimation() const { return isAnimation; }
    Q_INVOKABLE void setIsAnimation(bool value);

    bool getHasProbParameters() const { return hasProbParameters; }
    Q_INVOKABLE void setHasProbParameters(bool value);

    double getEllipseOrder() const { return ellipse_order; }
    Q_INVOKABLE void setEllipseOrder(double value);

    QString getMaterial()  const { return m_material; }
    QString getMaterial1() const { return m_material1; }
    QString getMaterial2() const { return m_material2; }

    Q_INVOKABLE void setMaterial(const QString& value);
    Q_INVOKABLE void setMaterial1(const QString& value);
    Q_INVOKABLE void setMaterial2(const QString& value);

    Q_INVOKABLE void processPointInput(const QString &text);

    float getWaveSpread()         const { return wave_spread; }
    float getStefanNumber()       const { return stefan_number; }
    int   getInitialNucleiCount() const { return initial_nuclei_count; }
    int   getNumRndLoads()        const { return num_rnd_loads; }

    Q_INVOKABLE void setWaveSpread(float value);
    Q_INVOKABLE void setStefanNumber(float value);
    Q_INVOKABLE void setInitialNucleiCount(int value);
    Q_INVOKABLE void setNumRndLoads(int value);

    static Parameters* m_instance;

    static int32_t*** voxels;

    static unsigned int seed;
    static QString filename;
    static int num_threads;
    static QString working_directory;
    static float wave_coefficient;
    static float wave_spread;
    static int initial_nuclei_count;
    static float halfaxis_a;
    static float halfaxis_b;
    static float halfaxis_c;
    static float orientation_angle_a;
    static float orientation_angle_b;
    static float orientation_angle_c;
    static float stefan_number;

    static std::vector<TextureLibrary::Component> textureComponents;

signals:
    void sizeChanged();
    void pointsChanged();
    void algorithmChanged();
    void seedChanged();
    void filenameChanged();
    void numThreadsChanged();
    void workingDirectoryChanged();
    void waveCoefficientChanged();

    void halfAxisAChanged();
    void halfAxisBChanged();
    void halfAxisCChanged();

    void orientationAngleAChanged();
    void orientationAngleBChanged();
    void orientationAngleCChanged();

    void pointsModeChanged();
    void isAnimationChanged();
    void initialConditionSelectionChanged();

    void hasProbParametersChanged();
    void ellipseOrderChanged();

    void materialChanged();
    void material1Changed();
    void material2Changed();

    void waveSpreadChanged();
    void stefanNumberChanged();
    void initialNucleiCountChanged();
    void numRndLoadsChanged();

private:
    static int size;
    static int points;
    static QString algorithm;

    // static unsigned int seed;
    // static QString filename;
    // static int num_threads;
    // static QString working_directory;
    // static float wave_coefficient;
    // static float halfaxis_a;
    // static float halfaxis_b;
    // static float halfaxis_c;
    // static float orientation_angle_a;
    // static float orientation_angle_b;
    // static float orientation_angle_c;

    static QString points_mode; // "count" / "density"
    static bool isAnimation;
    static bool nogui;
    static bool hasProbParameters;
    static double ellipse_order;
    static unsigned int num_rnd_loads;

    static QString m_material;
    static QString m_material1;
    static QString m_material2;
};

#endif // PARAMETERS_H

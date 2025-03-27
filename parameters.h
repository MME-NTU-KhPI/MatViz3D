#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <QObject>
#include <QString>

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

    static Parameters* m_instance;
    static int size;
    static int points;
    static QString algorithm;
    static unsigned int seed;
    static QString filename;
    static int num_threads;
    static QString working_directory;
    static float wave_coefficient;
    static float halfaxis_a;
    static float halfaxis_b;
    static float halfaxis_c;
    static float orientation_angle_a;
    static float orientation_angle_b;
    static float orientation_angle_c;

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

private:
    // static Parameters* m_instance;
    // static int size;
    // static int points;
    // static QString algorithm;
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
};

#endif // PARAMETERS_H

#include "parameters.h"
#include "openglwidgetqml.h"
#include <QDir>

Parameters* Parameters::m_instance = nullptr;

int32_t*** Parameters::voxels;
int Parameters::size = 1;
int Parameters::points = 0;
QString Parameters::algorithm = "";
unsigned int Parameters::seed = 0;
QString Parameters::filename = "";
int Parameters::num_threads = 1;
QString Parameters::working_directory = "";
float Parameters::wave_coefficient = 0.0f;

float Parameters::halfaxis_a = 0.0f;
float Parameters::halfaxis_b = 0.0f;
float Parameters::halfaxis_c = 0.0f;

float Parameters::orientation_angle_a = 0.0f;
float Parameters::orientation_angle_b = 0.0f;
float Parameters::orientation_angle_c = 0.0f;

QString Parameters::points_mode = "count";
bool Parameters::isAnimation = false;

Parameters::Parameters(QObject* parent) : QObject(parent) {}

void Parameters::processPointInput(const QString &text)
{
    bool ok = false;
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();

    if (getPointsMode() == "count")
    {
        points = text.toInt(&ok);
        if (ok)
        {
            ogl->setNumColors(points);
        }
    }
    else if (getPointsMode() == "density")
    {
        double concentration = text.toDouble(&ok);
        if (ok)
        {
            int volume = std::pow(size, 3);
            points = static_cast<int>(concentration * volume / 100.0);
            ogl->setNumColors(points);
        }
        qDebug() << "Calculated Points:" << points;
    }

    emit initialConditionSelectionChanged();
}


void Parameters::setSize(int value) {
    if (size != value) {
        qDebug() << "Новий розмір: " << value;
        size = value;
        emit sizeChanged();
    }
}

void Parameters::setPoints(int value) {
    if (points != value) {
        qDebug() << "Кількість точок: " << value;
        points = value;
        emit pointsChanged();
    }
}

void Parameters::setAlgorithm(const QString& value) {
    if (algorithm != value) {
        algorithm = value;
        emit algorithmChanged();
    }
}

void Parameters::setSeed(unsigned int value) {
    if (seed != value) {
        seed = value;
        emit seedChanged();
    }
}

void Parameters::setFilename(const QString& value) {
    if (filename != value) {
        filename = value;
        emit filenameChanged();
    }
}

void Parameters::setNumThreads(int value) {
    if (num_threads != value) {
        num_threads = value;
        emit numThreadsChanged();
    }
}

void Parameters::setWorkingDirectory(const QString& value) {
    if (working_directory != value) {
        working_directory = value;
        emit workingDirectoryChanged();
    }
}

void Parameters::setWaveCoefficient(float value) {
    if (wave_coefficient != value) {
        wave_coefficient = value;
        emit waveCoefficientChanged();
    }
}

void Parameters::setHalfAxisA(float value) {
    if (halfaxis_a != value) {
        halfaxis_a = value;
        emit halfAxisAChanged();
    }
}

void Parameters::setHalfAxisB(float value) {
    if (halfaxis_b != value) {
        halfaxis_b = value;
        emit halfAxisBChanged();
    }
}

void Parameters::setHalfAxisC(float value) {
    if (halfaxis_c != value) {
        halfaxis_c = value;
        emit halfAxisCChanged();
    }
}

void Parameters::setOrientationAngleA(float value) {
    if (orientation_angle_a != value) {
        orientation_angle_a = value;
        emit orientationAngleAChanged();
    }
}

void Parameters::setOrientationAngleB(float value) {
    if (orientation_angle_b != value) {
        orientation_angle_b = value;
        emit orientationAngleBChanged();
    }
}

void Parameters::setOrientationAngleC(float value) {
    if (orientation_angle_c != value) {
        orientation_angle_c = value;
        emit orientationAngleCChanged();
    }
}

void Parameters::setPointsMode(const QString& value) {
    if (points_mode != value) {
        points_mode = value;
        emit pointsModeChanged();
    }
}

void Parameters::setIsAnimation(bool value) {
    if (isAnimation != value) {
        isAnimation = value;
        emit isAnimationChanged();
    }
}

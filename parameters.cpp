#include "parameters.h"
#include "openglwidgetqml.h"
#include "dbmanager.h"
#include "tensorcontroller.h"

#include <QDir>
#include <QVariantList>
#include <QVariantMap>
#include <QDebug>

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
// -------------------------------
void Parameters::loadMaterialListFromDatabase()
{
    if (!db) {
        qWarning() << "DBManager not set in Parameters!";
        return;
    }

    materialList.clear();

    QString sql = "SELECT Material FROM material_properties";
    QVariantList rows = db->executeSelectQuery(sql);

    for (const QVariant& v : rows) {
        QVariantMap row = v.toMap();
        materialList.append(row["Material"].toString());
    }

    emit materialListChanged();
    qDebug() << "Material list loaded:" << materialList;
}

// -------------------------------
// 2) Встановлення матеріалу і зчитування констант
// -------------------------------
void Parameters::setSelectedMaterial(const QString& name)
{
    if (!db) {
        qWarning() << "DBManager not set in Parameters!";
        return;
    }

    selectedMaterial = name;
    emit selectedMaterialChanged();

    QString sql = QString("SELECT c11, c12, c44 FROM material_properties WHERE Material='%1'")
                      .arg(name);

    QVariantList rows = db->executeSelectQuery(sql);

    if (rows.isEmpty()) {
        qWarning() << "Material not found in DB:" << name;
        return;
    }

    QVariantMap row = rows.first().toMap();

    double C11 = row["c11"].toDouble();
    double C12 = row["c12"].toDouble();
    double C44 = row["c44"].toDouble();

    TensorController::instance()->setC11(C11);
    TensorController::instance()->setC12(C12);
    TensorController::instance()->setC44(C44);

    // Можеш зберігати їх глобально, якщо треба
    qDebug() << "Selected material:" << name
             << "C11=" << C11 << "C12=" << C12 << "C44=" << C44;
}

void Parameters::setDependence(const QString &dep)
{
    if (m_dependence == dep)
        return;

    m_dependence = dep;
    emit dependenceChanged();
    qDebug() << "[Parameters] dependence set to:" << dep;
}


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

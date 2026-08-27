#include "parameters.h"
#include "openglwidgetqml.h"
#include "dbmanager.h"
#include <QDebug>
#include <QDir>

Parameters* Parameters::m_instance = nullptr;

int32_t*** Parameters::voxels;
int Parameters::size = 10;
int Parameters::points = 10;
QString Parameters::algorithm = "";
unsigned int Parameters::seed = 0;
QString Parameters::filename = "";
int Parameters::num_threads = 1;
QString Parameters::working_directory = "";
float Parameters::wave_coefficient = 0.0f;
float Parameters::wave_spread;
int Parameters::initial_nuclei_count = 1;
unsigned int Parameters::num_rnd_loads = 0;
QString Parameters::prob_preset = "Sphere (Circle)";
float Parameters::halfaxis_a = 1.5f;
float Parameters::halfaxis_b = 1.5f;
float Parameters::halfaxis_c = 1.5f;

float Parameters::orientation_angle_a = 0.0f;
float Parameters::orientation_angle_b = 0.0f;
float Parameters::orientation_angle_c = 0.0f;

QString Parameters::points_mode = "count";
bool Parameters::isAnimation = false;
bool Parameters::isGifRecording = false;

bool   Parameters::hasProbParameters = false;
double Parameters::ellipse_order     = 2.0; // 2.0 = standard ellipsoid
float Parameters::stefan_number = 100.0f;

QString Parameters::m_material  = "bcc";
QString Parameters::m_material1  = "fcc";
QString Parameters::m_material2  = "bcc";

double  Parameters::minkowski_p = 2.0;   // Euclidean == the classical Voronoi
bool    Parameters::is_periodic = false;

QString Parameters::db_material = "";
double  Parameters::mat_c11 = 168.40;    // GPa, Cu -- the solvers' historical default
double  Parameters::mat_c12 = 121.40;
double  Parameters::mat_c44 = 75.40;
QString Parameters::mat_type = "fcc";

QString Parameters::texture_preset   = "random";
double  Parameters::texture_scatter  = 11.0;
QString Parameters::lattice_override = "";

// Composite defaults: a perfect square lattice of circular unidirectional
// fibers at 40 % volume fraction -- the textbook RVE the imperfection knobs
// perturb away from.
QString Parameters::composite_dim         = "1d";
QString Parameters::composite_packing     = "square";
double  Parameters::fiber_volume_fraction = 0.40;
int     Parameters::fibers_per_row        = 3;
double  Parameters::fiber_aspect_ratio    = 1.0;
double  Parameters::fiber_angle_scatter   = 0.0;
double  Parameters::fiber_center_jitter   = 0.0;
bool    Parameters::fiber_allow_overlap   = false;
QString Parameters::matrix_material       = "Epoxy";
QString Parameters::fiber_material        = "C-fiber";

QString Parameters::stressSolver = "ansys";
QString Parameters::stressMode   = "dataset";
double  Parameters::stressEps[6] = {0, 0, 0, 0, 0, 0};

std::vector<TextureLibrary::Component> Parameters::textureComponents;
PhaseAssignment Parameters::phaseAssignment;

Parameters::Parameters(QObject* parent) : QObject(parent) {}

void Parameters::processPointInput(const QString &text)
{
    bool ok = false;
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();

    if (getPointsMode() == "count")
    {
        points = text.toInt(&ok);
        if (ok && ogl != nullptr)
        {
            ogl->setNumColors(points);
        }
    }
    else if (getPointsMode() == "density")
    {
        double concentration = text.toDouble(&ok);
        if (ok)
        {
            const double volume = std::pow(static_cast<double>(size), 3);
            // Rounded, not truncated: pointsDisplayValue() converts the other
            // way, and truncation made a count lose a point on every trip
            // through the concentration field.
            points = static_cast<int>(std::lround(concentration * volume / 100.0));
            if (ogl)
                ogl->setNumColors(points);
        }
        qDebug() << "Calculated Points:" << points;
    }

    emit initialConditionSelectionChanged();
}

QString Parameters::pointsDisplayValue() const
{
    if (getPointsMode() != "density" || size <= 0)
        return QString::number(points);

    const double volume = std::pow(static_cast<double>(size), 3);
    return QString::number(points * 100.0 / volume, 'g', 6);
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

void Parameters::setProbPreset(const QString& value)
{
    QString v = value.trimmed();
    prob_preset = v;

    if (v.contains("Sphere", Qt::CaseInsensitive) || v.contains("Circle", Qt::CaseInsensitive)) {
        setHalfAxisA(1.5f);
        setHalfAxisB(1.5f);
        setHalfAxisC(1.5f);
        setOrientationAngleA(0.0f);
        setOrientationAngleB(0.0f);
        setOrientationAngleC(0.0f);
        setEllipseOrder(2.0);
    } else if (v.contains("Prolate", Qt::CaseInsensitive) || v.contains("Needle", Qt::CaseInsensitive)) {
        setHalfAxisA(3.0f);
        setHalfAxisB(1.0f);
        setHalfAxisC(1.0f);
        setOrientationAngleA(0.0f);
        setOrientationAngleB(0.0f);
        setOrientationAngleC(0.0f);
        setEllipseOrder(2.0);
    } else if (v.contains("Oblate", Qt::CaseInsensitive) || v.contains("Disc", Qt::CaseInsensitive)) {
        setHalfAxisA(1.0f);
        setHalfAxisB(3.0f);
        setHalfAxisC(3.0f);
        setOrientationAngleA(0.0f);
        setOrientationAngleB(0.0f);
        setOrientationAngleC(0.0f);
        setEllipseOrder(2.0);
    } else if (v.contains("Triaxial", Qt::CaseInsensitive) || v.contains("Ellipse", Qt::CaseInsensitive)) {
        setHalfAxisA(3.0f);
        setHalfAxisB(2.0f);
        setHalfAxisC(1.0f);
        setOrientationAngleA(0.0f);
        setOrientationAngleB(0.0f);
        setOrientationAngleC(0.0f);
        setEllipseOrder(2.0);
    } else if (v.contains("Superellipsoid", Qt::CaseInsensitive) || v.contains("Cube", Qt::CaseInsensitive)) {
        setHalfAxisA(1.5f);
        setHalfAxisB(1.5f);
        setHalfAxisC(1.5f);
        setOrientationAngleA(0.0f);
        setOrientationAngleB(0.0f);
        setOrientationAngleC(0.0f);
        setEllipseOrder(4.0);
    }

    emit probPresetChanged();
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

void Parameters::setIsGifRecording(bool value)
{
    if (isGifRecording == value) return;
    isGifRecording = value;
    emit isGifRecordingChanged();
}

void Parameters::setHasProbParameters(bool value) {
    if (hasProbParameters != value) {
        hasProbParameters = value;
        emit hasProbParametersChanged();
    }
}

void Parameters::setEllipseOrder(double value) {
    if (ellipse_order != value) {
        ellipse_order = value;
        emit ellipseOrderChanged();
    }
}

void Parameters::setMaterial(const QString& value)
{
    if (m_material != value) {
        m_material = value;
        emit materialChanged();
    }
}

void Parameters::setMaterial1(const QString& value)
{
    if (m_material1 != value) {
        m_material1 = value;
        emit material1Changed();
    }
}

void Parameters::setMaterial2(const QString& value)
{
    if (m_material2 != value) {
        m_material2 = value;
        emit material2Changed();
    }
}

void Parameters::setMinkowskiP(double value)
{
    if (minkowski_p != value) {
        minkowski_p = value;
        emit minkowskiPChanged();
    }
}

void Parameters::setIsPeriodic(bool value)
{
    if (is_periodic != value) {
        is_periodic = value;
        emit isPeriodicChanged();
    }
}

void Parameters::setDbMaterial(const QString& value)
{
    if (db_material == value)
        return;

    db_material = value;

    double c11 = 0, c12 = 0, c44 = 0;
    QString type;
    if (value.isEmpty()) {
        // Back to the built-in Cu constants.
        mat_c11 = 168.40; mat_c12 = 121.40; mat_c44 = 75.40; mat_type = "fcc";
    } else if (DBManager::cubicConstants(value, c11, c12, c44, type)) {
        // A row with every constant left at 0 (a hand-added material nobody
        // filled in) would hand the solvers a singular stiffness, so keep the
        // previous constants and say so rather than producing garbage.
        if (c11 > 0.0 && c44 > 0.0) {
            mat_c11 = c11; mat_c12 = c12; mat_c44 = c44;
            mat_type = type.isEmpty() ? QStringLiteral("fcc") : type.toLower();
            qInfo().noquote() << QString("material: %1 (%2)  C11=%3 C12=%4 C44=%5 GPa")
                                     .arg(value, mat_type)
                                     .arg(mat_c11).arg(mat_c12).arg(mat_c44);
        } else {
            qWarning() << "material" << value
                       << "has no elastic constants in the database; keeping"
                       << mat_c11 << mat_c12 << mat_c44 << "GPa";
        }
    } else {
        qWarning() << "material" << value
                   << "not found in material_properties.db; keeping"
                   << mat_c11 << mat_c12 << mat_c44 << "GPa";
    }

    // The lattice may have flipped fcc <-> bcc, and the presets differ per
    // lattice, so the texture has to follow the material.
    rebuildTextureFromPreset();

    emit dbMaterialChanged();
}

void Parameters::setTexturePreset(const QString& value)
{
    // Accept both the UI labels ("Scattered cube", "Custom (editor)") and the
    // CLI spellings ("scattered_cube", "custom"); store one normalised form.
    QString norm = value.trimmed().toLower();
    norm.replace(' ', '_').replace('-', '_');
    if (norm.startsWith("custom"))       norm = "custom";
    else if (norm.startsWith("recryst")) norm = "recrystallization";

    if (texture_preset == norm)
        return;

    texture_preset = norm;
    rebuildTextureFromPreset();
    emit textureSettingsChanged();
}

QString Parameters::texturePresetLabel()
{
    // Must match textureParamFields()'s option list exactly.
    if (texture_preset == "extrusion")         return QStringLiteral("Extrusion");
    if (texture_preset == "rolling")           return QStringLiteral("Rolling");
    if (texture_preset == "recrystallization") return QStringLiteral("Recrystallization");
    if (texture_preset == "shear")             return QStringLiteral("Shear");
    if (texture_preset == "scattered_cube")    return QStringLiteral("Scattered cube");
    if (texture_preset == "custom")            return QStringLiteral("Custom (editor)");
    return QStringLiteral("Random");
}

void Parameters::setTextureScatter(double value)
{
    if (texture_scatter == value)
        return;

    texture_scatter = value;
    rebuildTextureFromPreset();
    emit textureSettingsChanged();
}

void Parameters::markTextureCustom()
{
    if (texture_preset == "custom")
        return;
    texture_preset = "custom";
    emit textureSettingsChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Composite (fiber-reinforced RVE)
// ─────────────────────────────────────────────────────────────────────────────

void Parameters::setCompositeDim(const QString& value)
{
    // Accepts the panel labels ("2D (fibers along X,Y)") and the CLI spellings
    // ("2d", "2") alike -- the leading digit is the whole answer.
    const QString norm = value.trimmed().toLower();
    QString stored = composite_dim;
    if      (norm.startsWith('1')) stored = "1d";
    else if (norm.startsWith('2')) stored = "2d";
    else if (norm.startsWith('3')) stored = "3d";
    else {
        qWarning() << "composite_dim expects 1D, 2D or 3D; got" << value
                   << "-- keeping" << composite_dim;
        return;
    }

    if (composite_dim == stored)
        return;
    composite_dim = stored;
    emit compositeSettingsChanged();
}

void Parameters::setCompositePacking(const QString& value)
{
    const QString norm = value.trimmed().toLower();
    const QString stored = norm.startsWith("hex") ? QStringLiteral("hexagonal")
                                                  : QStringLiteral("square");
    if (composite_packing == stored)
        return;
    composite_packing = stored;
    emit compositeSettingsChanged();
}

// Must match compositeParamFields()'s option lists exactly, or the combo box
// falls back to its first entry and silently disagrees with what will run.
QString Parameters::compositeDimLabel()
{
    if (composite_dim == "2d") return QStringLiteral("2D (fibers along X,Y)");
    if (composite_dim == "3d") return QStringLiteral("3D (fibers along X,Y,Z)");
    return QStringLiteral("1D (fibers along Z)");
}

QString Parameters::compositePackingLabel()
{
    return composite_packing == "hexagonal" ? QStringLiteral("Hexagonal")
                                            : QStringLiteral("Square");
}

int Parameters::compositeDimensions()
{
    if (composite_dim == "2d") return 2;
    if (composite_dim == "3d") return 3;
    return 1;
}

bool Parameters::compositeHexagonal()
{
    return composite_packing == "hexagonal";
}

void Parameters::setFiberVolumeFraction(double value)
{
    if (fiber_volume_fraction == value) return;
    fiber_volume_fraction = value;
    emit compositeSettingsChanged();
}

void Parameters::setFibersPerRow(int value)
{
    if (fibers_per_row == value) return;
    fibers_per_row = value;
    emit compositeSettingsChanged();
}

void Parameters::setFiberAspectRatio(double value)
{
    if (fiber_aspect_ratio == value) return;
    fiber_aspect_ratio = value;
    emit compositeSettingsChanged();
}

void Parameters::setFiberAngleScatter(double value)
{
    if (fiber_angle_scatter == value) return;
    fiber_angle_scatter = value;
    emit compositeSettingsChanged();
}

void Parameters::setFiberCenterJitter(double value)
{
    if (fiber_center_jitter == value) return;
    fiber_center_jitter = value;
    emit compositeSettingsChanged();
}

void Parameters::setFiberAllowOverlap(bool value)
{
    if (fiber_allow_overlap == value) return;
    fiber_allow_overlap = value;
    emit compositeSettingsChanged();
}

void Parameters::setMatrixMaterial(const QString& value)
{
    if (matrix_material == value) return;
    matrix_material = value;
    emit compositeSettingsChanged();
}

void Parameters::setFiberMaterial(const QString& value)
{
    if (fiber_material == value) return;
    fiber_material = value;
    emit compositeSettingsChanged();
}

void Parameters::cubicConstantsPa(double& c11, double& c12, double& c44)
{
    c11 = mat_c11 * 1e9;
    c12 = mat_c12 * 1e9;
    c44 = mat_c44 * 1e9;
}

bool Parameters::materialStiffnessPa(const QString& name, double C[6][6])
{
    double G[6][6] = {{0}};
    QString type;

    if (name.isEmpty() || !DBManager::stiffnessMatrix(name, G, type)) {
        if (!name.isEmpty())
            qWarning() << "material" << name
                       << "has no usable elastic constants in material_properties.db";
        return false;
    }

    // The table stores GPa, every solver works in Pa.
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) C[i][j] = G[i][j] * 1e9;
    return true;
}

void Parameters::setLatticeOverride(const QString& value)
{
    const QString norm = value.trimmed().toLower();
    if (lattice_override == norm)
        return;
    lattice_override = norm;
    rebuildTextureFromPreset();
    emit textureSettingsChanged();
}

TextureLibrary::Lattice Parameters::materialLattice()
{
    const QString type = lattice_override.isEmpty() ? mat_type : lattice_override;
    return (type.compare("bcc", Qt::CaseInsensitive) == 0)
               ? TextureLibrary::Lattice::BCC
               : TextureLibrary::Lattice::FCC;
}

void Parameters::rebuildTextureFromPreset()
{
    if (texture_preset == "custom")
        return;   // owned by the texture editor

    TextureLibrary::Process proc;
    if      (texture_preset == "random")            proc = TextureLibrary::Process::Random;
    else if (texture_preset == "extrusion")         proc = TextureLibrary::Process::Extrusion;
    else if (texture_preset == "rolling")           proc = TextureLibrary::Process::Rolling;
    else if (texture_preset == "recrystallization") proc = TextureLibrary::Process::Recrystallization;
    else if (texture_preset == "shear")             proc = TextureLibrary::Process::Shear;
    else if (texture_preset == "scattered_cube")    proc = TextureLibrary::Process::ScatteredCube;
    else {
        qWarning() << "unknown texture preset" << texture_preset << "-- using random";
        proc = TextureLibrary::Process::Random;
    }

    // Random is the pipeline's "no components" state: leaving the vector empty
    // is what every existing caller already treats as uniformly random, and it
    // keeps runs made before this option existed reproducible.
    if (proc == TextureLibrary::Process::Random) {
        textureComponents.clear();
        return;
    }

    textureComponents = TextureLibrary::componentsForProcess(proc, materialLattice(),
                                                             texture_scatter);
}

void Parameters::setWaveSpread(float value)
{
    if (wave_spread != value) {
        wave_spread = value;
        emit waveSpreadChanged();
    }
}

void Parameters::setStefanNumber(float value)
{
    if (stefan_number != value) {
        stefan_number = value;
        emit stefanNumberChanged();
    }
}

void Parameters::setInitialNucleiCount(int value)
{
    if (initial_nuclei_count != value) {
        initial_nuclei_count = value;
        emit initialNucleiCountChanged();
    }
}

void Parameters::setNumRndLoads(unsigned int value)
{
    if (num_rnd_loads != value) {
        num_rnd_loads = value;
        emit numRndLoadsChanged();
    }
}

void Parameters::setStressSolver(const QString& value) {
    if (stressSolver != value) {
        stressSolver = value;
        emit stressSolverChanged();
    }
}

void Parameters::setStressMode(const QString& value) {
    if (stressMode != value) {
        stressMode = value;
        emit stressModeChanged();
    }
}

void Parameters::setStressEps(const double value[6]) {
    bool changed = false;
    for (int i = 0; i < 6; ++i)
        if (stressEps[i] != value[i]) { stressEps[i] = value[i]; changed = true; }
    if (changed)
        emit stressEpsChanged();
}

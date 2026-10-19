#include "parameters.h"
#include "latticefamily.h"
#include "fft_solver_session.hpp"
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
QString Parameters::csv_output = "";
float Parameters::wave_coefficient = 0.0f;
float Parameters::wave_spread;
int Parameters::initial_nuclei_count = 1;
bool Parameters::is_wave_generation = false;
double Parameters::wave_peak_fraction = 0.20;
double Parameters::wave_end_fraction = 0.60;
unsigned int Parameters::num_rnd_loads = 0;
QString Parameters::prob_preset = "Sphere (Circle)";
QString Parameters::polycrystall_neighborhood = "Moore (26)";
QString Parameters::prob_matrix_mode = "Volume Sampling";
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
bool    Parameters::is_thin_layer = false;
QString Parameters::layer_direction = "+Z";
QString Parameters::voronoi_metric_preset = "Sphere (Circle)";
double  Parameters::voronoi_mxx = 1.0;
double  Parameters::voronoi_myy = 1.0;
double  Parameters::voronoi_mzz = 1.0;
double  Parameters::voronoi_mxy = 0.0;
double  Parameters::voronoi_myz = 0.0;
double  Parameters::voronoi_mxz = 0.0;

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
QString Parameters::lattice_family         = "gyroid_sheet";
int     Parameters::cells_per_edge         = 2;
double  Parameters::relative_density       = 0.30;
double  Parameters::lattice_grading        = 0.0;
double  Parameters::lattice_jitter         = 0.0;
double  Parameters::lattice_achieved_density = 0.0;
double  Parameters::lattice_wall_voxels    = 0.0;
std::vector<std::array<float,5>> Parameters::lattice_wave_terms;
double  Parameters::spinodoid_theta_x      = 90.0;
double  Parameters::spinodoid_theta_y      = 90.0;
double  Parameters::spinodoid_theta_z      = 90.0;
int     Parameters::fourier_terms          = 24;
double  Parameters::fourier_kmin           = 1.0;
double  Parameters::fourier_kmax           = 4.0;
double  Parameters::fourier_decay          = 1.0;
QString Parameters::fourier_file;
QString Parameters::fourier_symmetry = QStringLiteral("none");
double  Parameters::plate_hole = 0.0;
QString Parameters::lattice_union;
double  Parameters::noise_smooth           = 0.06;
double  Parameters::mat_e_gpa              = 0.0;
double  Parameters::mat_nu                 = 0.38;
double  Parameters::mat_aniso              = 1.0;
bool    Parameters::hdf5_fields            = true;
double  Parameters::lattice_cell_mm        = 10.0;
bool    Parameters::lattice_uniform_wall   = false;
double  Parameters::print_min_wall_mm      = 0.8;
double  Parameters::print_max_overhang_deg = 45.0;
double  Parameters::print_max_thin_fraction        = 0.02;
int     Parameters::print_max_island_vox           = 8;
int     Parameters::print_max_components           = 1;
double  Parameters::print_max_unsupported_fraction = 0.05;
bool    Parameters::require_printable      = false;
PrintabilityReport Parameters::printability;
double  Parameters::lattice_voxel_mm       = 0.0;
QString Parameters::fft_scheme             = "auto";
double  Parameters::fft_tol                = 1e-5;
int     Parameters::fft_max_iter           = 1000;

QString Parameters::stressSolver = "ansys";
QString Parameters::stressMode   = "dataset";
double  Parameters::stressEps[6] = {0, 0, 0, 0, 0, 0};

std::vector<TextureLibrary::Component> Parameters::textureComponents;
PhaseAssignment Parameters::phaseAssignment;

Parameters::Parameters(QObject* parent) : QObject(parent) {}

void Parameters::resetDefaults()
{
    size = 10;
    points = 10;
    algorithm = "";
    seed = 0;
    filename = "";
    num_threads = 1;
    working_directory = "";
    csv_output = "";
    wave_coefficient = 0.0f;
    wave_spread = 0.0f;
    initial_nuclei_count = 1;
    is_wave_generation = false;
    wave_peak_fraction = 0.20;
    wave_end_fraction = 0.60;
    num_rnd_loads = 0;
    prob_preset = "Sphere (Circle)";
    polycrystall_neighborhood = "Moore (26)";
    prob_matrix_mode = "Volume Sampling";
    halfaxis_a = 1.5f;
    halfaxis_b = 1.5f;
    halfaxis_c = 1.5f;
    orientation_angle_a = 0.0f;
    orientation_angle_b = 0.0f;
    orientation_angle_c = 0.0f;
    points_mode = "count";
    isAnimation = false;
    isGifRecording = false;
    hasProbParameters = false;
    ellipse_order = 2.0;
    stefan_number = 100.0f;
    m_material = "bcc";
    m_material1 = "fcc";
    m_material2 = "bcc";
    minkowski_p = 2.0;
    is_periodic = false;
    is_thin_layer = false;
    layer_direction = "+Z";
    voronoi_metric_preset = "Sphere (Circle)";
    voronoi_mxx = 1.0;
    voronoi_myy = 1.0;
    voronoi_mzz = 1.0;
    voronoi_mxy = 0.0;
    voronoi_myz = 0.0;
    voronoi_mxz = 0.0;
    db_material = "";
    mat_c11 = 168.40;
    mat_c12 = 121.40;
    mat_c44 = 75.40;
    mat_type = "fcc";
    texture_preset = "random";
    texture_scatter = 11.0;
    lattice_override = "";
    composite_dim = "1d";
    composite_packing = "square";
    fiber_volume_fraction = 0.40;
    fibers_per_row = 3;
    fiber_aspect_ratio = 1.0;
    fiber_angle_scatter = 0.0;
    fiber_center_jitter = 0.0;
    fiber_allow_overlap = false;
    matrix_material = "Epoxy";
    fiber_material = "C-fiber";
    lattice_family   = "gyroid_sheet";
    cells_per_edge   = 2;
    relative_density = 0.30;
    lattice_grading  = 0.0;
    lattice_jitter   = 0.0;
    lattice_achieved_density = 0.0;
    lattice_wall_voxels      = 0.0;
    lattice_wave_terms.clear();
    spinodoid_theta_x = 90.0;
    spinodoid_theta_y = 90.0;
    spinodoid_theta_z = 90.0;
    fourier_terms = 24;
    fourier_kmin  = 1.0;
    fourier_kmax  = 4.0;
    fourier_decay = 1.0;
    fourier_file.clear();
    fourier_symmetry = QStringLiteral("none");
    plate_hole = 0.0;
    lattice_union.clear();
    noise_smooth = 0.06;
    mat_e_gpa = 0.0;
    mat_nu = 0.38;
    mat_aniso = 1.0;
    hdf5_fields = true;
    lattice_cell_mm = 10.0;
    lattice_uniform_wall = false;
    print_min_wall_mm = 0.8;
    print_max_overhang_deg = 45.0;
    print_max_thin_fraction = 0.02;
    print_max_island_vox = 8;
    print_max_components = 1;
    print_max_unsupported_fraction = 0.05;
    require_printable = false;
    printability = PrintabilityReport();
    lattice_voxel_mm = 0.0;
    fft_scheme = "auto";
    fft_tol = 1e-5;
    fft_max_iter = 1000;
    stressSolver = "ansys";
    stressMode = "dataset";
    for (int i = 0; i < 6; ++i) stressEps[i] = 0.0;
    textureComponents.clear();
    phaseAssignment.clear();
}

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

static bool s_inProbPreset = false;

void Parameters::setProbPreset(const QString& value)
{
    QString v = value.trimmed();
    prob_preset = v;
    s_inProbPreset = true;

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

    s_inProbPreset = false;
    emit probPresetChanged();
}

void Parameters::setPolycrystallNeighborhood(const QString& value)
{
    QString v = value.trimmed();
    QString resolved;
    if (v.contains("neumann", Qt::CaseInsensitive) || v == "6") {
        resolved = "von Neumann (6)";
    } else if (v.contains("radial", Qt::CaseInsensitive) || v == "18") {
        resolved = "Radial (18)";
    } else {
        resolved = "Moore (26)";
    }
    if (polycrystall_neighborhood != resolved) {
        polycrystall_neighborhood = resolved;
        emit polycrystallNeighborhoodChanged();
    }
}

void Parameters::setProbMatrixMode(const QString& value)
{
    QString v = value.trimmed();
    if (prob_matrix_mode != v) {
        prob_matrix_mode = v;
        emit probMatrixModeChanged();
    }
}

void Parameters::setHalfAxisA(float value) {
    if (halfaxis_a != value) {
        halfaxis_a = value;
        if (!s_inProbPreset && prob_preset != "Custom") {
            prob_preset = "Custom";
            emit probPresetChanged();
        }
        emit halfAxisAChanged();
    }
}

void Parameters::setHalfAxisB(float value) {
    if (halfaxis_b != value) {
        halfaxis_b = value;
        if (!s_inProbPreset && prob_preset != "Custom") {
            prob_preset = "Custom";
            emit probPresetChanged();
        }
        emit halfAxisBChanged();
    }
}

void Parameters::setHalfAxisC(float value) {
    if (halfaxis_c != value) {
        halfaxis_c = value;
        if (!s_inProbPreset && prob_preset != "Custom") {
            prob_preset = "Custom";
            emit probPresetChanged();
        }
        emit halfAxisCChanged();
    }
}

void Parameters::setOrientationAngleA(float value) {
    if (orientation_angle_a != value) {
        orientation_angle_a = value;
        if (!s_inProbPreset && prob_preset != "Custom") {
            prob_preset = "Custom";
            emit probPresetChanged();
        }
        emit orientationAngleAChanged();
    }
}

void Parameters::setOrientationAngleB(float value) {
    if (orientation_angle_b != value) {
        orientation_angle_b = value;
        if (!s_inProbPreset && prob_preset != "Custom") {
            prob_preset = "Custom";
            emit probPresetChanged();
        }
        emit orientationAngleBChanged();
    }
}

void Parameters::setOrientationAngleC(float value) {
    if (orientation_angle_c != value) {
        orientation_angle_c = value;
        if (!s_inProbPreset && prob_preset != "Custom") {
            prob_preset = "Custom";
            emit probPresetChanged();
        }
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
        if (!s_inProbPreset && prob_preset != "Custom") {
            prob_preset = "Custom";
            emit probPresetChanged();
        }
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

static bool s_inMetricPreset = false;

void Parameters::setMinkowskiP(double value)
{
    if (minkowski_p != value) {
        minkowski_p = value;
        if (!s_inMetricPreset && voronoi_metric_preset != "Custom") {
            voronoi_metric_preset = "Custom";
            emit voronoiMetricPresetChanged();
        }
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

void Parameters::setIsThinLayer(bool value)
{
    if (is_thin_layer != value) {
        is_thin_layer = value;
        emit isThinLayerChanged();
    }
}

QString Parameters::normalizeLayerDirection(const QString& value)
{
    QString s = value.trimmed().toUpper();
    if (s == "-Z" || s == "Z-" || s.contains("TOP"))
        return "-Z";
    if (s == "+X" || s == "X+" || s == "X" || s.contains("LEFT"))
        return "+X";
    if (s == "-X" || s == "X-" || s.contains("RIGHT"))
        return "-X";
    if (s == "+Y" || s == "Y+" || s == "Y" || s.contains("FRONT"))
        return "+Y";
    if (s == "-Y" || s == "Y-" || s.contains("BACK"))
        return "-Y";
    return "+Z";
}

void Parameters::setLayerDirection(const QString& value)
{
    QString resolved = normalizeLayerDirection(value);
    if (layer_direction != resolved) {
        layer_direction = resolved;
        emit layerDirectionChanged();
    }
}

void Parameters::setVoronoiMetricPreset(const QString& value)
{
    QString v = value.trimmed();

    s_inMetricPreset = true;
    if (v.contains("Sphere", Qt::CaseInsensitive) || v.contains("Circle", Qt::CaseInsensitive) ||
        v.contains("Isotropic", Qt::CaseInsensitive) || v.contains("Equiaxed", Qt::CaseInsensitive) ||
        v == "sphere" || v == "isotropic") {
        voronoi_metric_preset = "Sphere (Circle)";
        setVoronoiMxx(1.0);
        setVoronoiMyy(1.0);
        setVoronoiMzz(1.0);
        setVoronoiMxy(0.0);
        setVoronoiMyz(0.0);
        setVoronoiMxz(0.0);
        setMinkowskiP(2.0);
    } else if (v.contains("Prolate", Qt::CaseInsensitive) || v.contains("Needle", Qt::CaseInsensitive) ||
               v == "prolate" || v == "needle") {
        voronoi_metric_preset = "Prolate (Needle)";
        setVoronoiMxx(1.0);
        setVoronoiMyy(9.0);
        setVoronoiMzz(9.0);
        setVoronoiMxy(0.0);
        setVoronoiMyz(0.0);
        setVoronoiMxz(0.0);
        setMinkowskiP(2.0);
    } else if (v.contains("Oblate", Qt::CaseInsensitive) || v.contains("Disc", Qt::CaseInsensitive) ||
               v == "oblate" || v == "disc") {
        voronoi_metric_preset = "Oblate (Disc)";
        setVoronoiMxx(9.0);
        setVoronoiMyy(1.0);
        setVoronoiMzz(1.0);
        setVoronoiMxy(0.0);
        setVoronoiMyz(0.0);
        setVoronoiMxz(0.0);
        setMinkowskiP(2.0);
    } else if (v.contains("Triaxial", Qt::CaseInsensitive) || v == "triaxial") {
        voronoi_metric_preset = "Triaxial Ellipsoid";
        setVoronoiMxx(1.0);
        setVoronoiMyy(2.25);
        setVoronoiMzz(9.0);
        setVoronoiMxy(0.0);
        setVoronoiMyz(0.0);
        setVoronoiMxz(0.0);
        setMinkowskiP(2.0);
    } else if (v.contains("Superellipsoid", Qt::CaseInsensitive) || v.contains("Cube", Qt::CaseInsensitive) ||
               v == "superellipsoid" || v == "cube") {
        voronoi_metric_preset = "Superellipsoid (Cube)";
        setVoronoiMxx(1.0);
        setVoronoiMyy(1.0);
        setVoronoiMzz(1.0);
        setVoronoiMxy(0.0);
        setVoronoiMyz(0.0);
        setVoronoiMxz(0.0);
        setMinkowskiP(4.0);
    } else if (v.contains("Columnar (Z", Qt::CaseInsensitive) || v.contains("Columnar Z", Qt::CaseInsensitive) ||
               v == "columnar_z") {
        voronoi_metric_preset = "Columnar (Z-axis)";
        setVoronoiMxx(4.0);
        setVoronoiMyy(4.0);
        setVoronoiMzz(1.0);
        setVoronoiMxy(0.0);
        setVoronoiMyz(0.0);
        setVoronoiMxz(0.0);
        setMinkowskiP(2.0);
    } else if (v.contains("Columnar (X", Qt::CaseInsensitive) || v.contains("Columnar X", Qt::CaseInsensitive) ||
               v == "columnar_x") {
        voronoi_metric_preset = "Columnar (X-axis)";
        setVoronoiMxx(1.0);
        setVoronoiMyy(4.0);
        setVoronoiMzz(4.0);
        setVoronoiMxy(0.0);
        setVoronoiMyz(0.0);
        setVoronoiMxz(0.0);
        setMinkowskiP(2.0);
    } else if (v.contains("Rolled", Qt::CaseInsensitive) || v.contains("Orthotropic", Qt::CaseInsensitive) ||
               v == "rolled") {
        voronoi_metric_preset = "Rolled (Orthotropic)";
        setVoronoiMxx(1.0);
        setVoronoiMyy(2.0);
        setVoronoiMzz(5.0);
        setVoronoiMxy(0.0);
        setVoronoiMyz(0.0);
        setVoronoiMxz(0.0);
        setMinkowskiP(2.0);
    } else if (v.contains("Sheared", Qt::CaseInsensitive) || v.contains("45", Qt::CaseInsensitive) ||
               v == "sheared") {
        voronoi_metric_preset = "Sheared (45° XY)";
        setVoronoiMxx(2.5);
        setVoronoiMyy(2.5);
        setVoronoiMzz(1.0);
        setVoronoiMxy(1.5);
        setVoronoiMyz(0.0);
        setVoronoiMxz(0.0);
        setMinkowskiP(2.0);
    } else {
        voronoi_metric_preset = "Custom";
    }
    s_inMetricPreset = false;

    emit voronoiMetricPresetChanged();
}

void Parameters::setVoronoiMxx(double value)
{
    if (voronoi_mxx != value) {
        voronoi_mxx = value;
        if (!s_inMetricPreset && voronoi_metric_preset != "Custom") {
            voronoi_metric_preset = "Custom";
            emit voronoiMetricPresetChanged();
        }
        emit voronoiMetricChanged();
    }
}

void Parameters::setVoronoiMyy(double value)
{
    if (voronoi_myy != value) {
        voronoi_myy = value;
        if (!s_inMetricPreset && voronoi_metric_preset != "Custom") {
            voronoi_metric_preset = "Custom";
            emit voronoiMetricPresetChanged();
        }
        emit voronoiMetricChanged();
    }
}

void Parameters::setVoronoiMzz(double value)
{
    if (voronoi_mzz != value) {
        voronoi_mzz = value;
        if (!s_inMetricPreset && voronoi_metric_preset != "Custom") {
            voronoi_metric_preset = "Custom";
            emit voronoiMetricPresetChanged();
        }
        emit voronoiMetricChanged();
    }
}

void Parameters::setVoronoiMxy(double value)
{
    if (voronoi_mxy != value) {
        voronoi_mxy = value;
        if (!s_inMetricPreset && voronoi_metric_preset != "Custom") {
            voronoi_metric_preset = "Custom";
            emit voronoiMetricPresetChanged();
        }
        emit voronoiMetricChanged();
    }
}

void Parameters::setVoronoiMyz(double value)
{
    if (voronoi_myz != value) {
        voronoi_myz = value;
        if (!s_inMetricPreset && voronoi_metric_preset != "Custom") {
            voronoi_metric_preset = "Custom";
            emit voronoiMetricPresetChanged();
        }
        emit voronoiMetricChanged();
    }
}

void Parameters::setVoronoiMxz(double value)
{
    if (voronoi_mxz != value) {
        voronoi_mxz = value;
        if (!s_inMetricPreset && voronoi_metric_preset != "Custom") {
            voronoi_metric_preset = "Custom";
            emit voronoiMetricPresetChanged();
        }
        emit voronoiMetricChanged();
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

void Parameters::setIsWaveGeneration(bool value)
{
    if (is_wave_generation != value) {
        is_wave_generation = value;
        emit isWaveGenerationChanged();
    }
}

void Parameters::setWavePeakFraction(double value)
{
    if (std::abs(wave_peak_fraction - value) > 1e-9) {
        wave_peak_fraction = value;
        emit wavePeakFractionChanged();
    }
}

void Parameters::setWaveEndFraction(double value)
{
    if (std::abs(wave_end_fraction - value) > 1e-9) {
        wave_end_fraction = value;
        emit waveEndFractionChanged();
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

// ── Lattice ──────────────────────────────────────────────────────────────────

void Parameters::setLatticeFamily(const QString& value)
{
    LatticeFamily f;
    if (!parseLatticeFamily(value, f)) {
        qWarning() << "lattice_family expects one of" << latticeFamilyKeys().join(", ")
                   << "; got" << value << "-- keeping" << lattice_family;
        return;
    }
    const QString stored = latticeFamilyKey(f);
    if (lattice_family == stored)
        return;
    lattice_family = stored;
    emit latticeSettingsChanged();
}

// Must match latticeSchema()'s option list exactly, or the combo box falls
// back to its first entry and silently disagrees with what will run.
QString Parameters::latticeFamilyLabelOf(const QString& key)
{
    LatticeFamily f;
    if (parseLatticeFamily(key, f))
        return latticeFamilyLabel(f);
    return latticeFamilyLabel(LatticeFamily::GyroidSheet);
}

void Parameters::setCellsPerEdge(int value)
{
    if (cells_per_edge == value) return;
    cells_per_edge = value;
    emit latticeSettingsChanged();
}

void Parameters::setRelativeDensity(double value)
{
    if (relative_density == value) return;
    relative_density = value;
    emit latticeSettingsChanged();
}

void Parameters::setLatticeGrading(double value)
{
    if (lattice_grading == value) return;
    lattice_grading = value;
    emit latticeSettingsChanged();
}

void Parameters::setLatticeJitter(double value)
{
    if (lattice_jitter == value) return;
    lattice_jitter = value;
    emit latticeSettingsChanged();
}

// ── FFT scheme ───────────────────────────────────────────────────────────────

void Parameters::setFftScheme(const QString& value)
{
    const QString norm = value.trimmed().toLower();
    if (norm != "auto" && norm != "basic" && norm != "cg") {
        qWarning() << "fft_scheme expects auto, basic or cg; got" << value
                   << "-- keeping" << fft_scheme;
        return;
    }
    if (fft_scheme == norm)
        return;
    fft_scheme = norm;
    emit fftSchemeChanged();
}

void Parameters::setFftTol(double value)
{
    if (!(value > 0.0) || value >= 1.0) {
        qWarning() << "fft_tol expects a value in (0, 1); got" << value << "-- keeping" << fft_tol;
        return;
    }
    if (fft_tol == value) return;
    fft_tol = value;
    emit fftSchemeChanged();
}

void Parameters::setFftMaxIter(int value)
{
    if (value < 1) {
        qWarning() << "fft_max_iter expects a positive integer; got" << value << "-- keeping" << fft_max_iter;
        return;
    }
    if (fft_max_iter == value) return;
    fft_max_iter = value;
    emit fftSchemeChanged();
}

// ── Spinodoid cones and Fourier series ──────────────────────────────────────

static bool validTheta(double v) { return v == 0.0 || (v >= 5.0 && v <= 90.0); }

void Parameters::setSpinodoidThetaX(double value)
{
    if (!validTheta(value)) { qWarning() << "spinodoid_theta_x expects 0 or 5..90 degrees; got" << value; return; }
    if (spinodoid_theta_x == value) return;
    spinodoid_theta_x = value;
    emit latticeSettingsChanged();
}

void Parameters::setSpinodoidThetaY(double value)
{
    if (!validTheta(value)) { qWarning() << "spinodoid_theta_y expects 0 or 5..90 degrees; got" << value; return; }
    if (spinodoid_theta_y == value) return;
    spinodoid_theta_y = value;
    emit latticeSettingsChanged();
}

void Parameters::setSpinodoidThetaZ(double value)
{
    if (!validTheta(value)) { qWarning() << "spinodoid_theta_z expects 0 or 5..90 degrees; got" << value; return; }
    if (spinodoid_theta_z == value) return;
    spinodoid_theta_z = value;
    emit latticeSettingsChanged();
}

void Parameters::setFourierTerms(int value)
{
    if (value < 2 || value > 256) { qWarning() << "fourier_terms expects 2..256; got" << value; return; }
    if (fourier_terms == value) return;
    fourier_terms = value;
    emit latticeSettingsChanged();
}

void Parameters::setFourierKmin(double value)
{
    if (!(value >= 1.0)) { qWarning() << "fourier_kmin expects a value >= 1; got" << value; return; }
    if (fourier_kmin == value) return;
    fourier_kmin = value;
    emit latticeSettingsChanged();
}

void Parameters::setFourierKmax(double value)
{
    if (!(value >= 1.0)) { qWarning() << "fourier_kmax expects a value >= 1; got" << value; return; }
    if (fourier_kmax == value) return;
    fourier_kmax = value;
    emit latticeSettingsChanged();
}

void Parameters::setFourierDecay(double value)
{
    if (value < 0.0 || value > 4.0) { qWarning() << "fourier_decay expects 0..4; got" << value; return; }
    if (fourier_decay == value) return;
    fourier_decay = value;
    emit latticeSettingsChanged();
}

void Parameters::setFourierFile(const QString& value)
{
    const QString v = value.trimmed();
    if (fourier_file == v) return;
    fourier_file = v;
    emit latticeSettingsChanged();
}

void Parameters::setFourierSymmetry(const QString& value)
{
    const QString v = value.trimmed().toLower();
    if (v != QLatin1String("none") && v != QLatin1String("cubic")) {
        qWarning() << "fourier_symmetry expects none or cubic; got" << value;
        return;
    }
    if (fourier_symmetry == v) return;
    fourier_symmetry = v;
    emit latticeSettingsChanged();
}

void Parameters::setPlateHole(double value)
{
    if (value < 0.0 || value > 0.9) { qWarning() << "plate_hole expects 0..0.9 cell edges; got" << value; return; }
    if (plate_hole == value) return;
    plate_hole = value;
    emit latticeSettingsChanged();
}

void Parameters::setLatticeUnion(const QString& value)
{
    const QString v = value.trimmed();
    QString stored;
    if (!v.isEmpty() && v.compare(QLatin1String("none"), Qt::CaseInsensitive) != 0) {
        LatticeFamily f;
        if (!parseLatticeFamily(v, f)) {
            qWarning() << "lattice_union expects none or one of" << latticeFamilyKeys().join(", ")
                       << "; got" << value << "-- keeping" << lattice_union;
            return;
        }
        stored = latticeFamilyKey(f);
    }
    if (lattice_union == stored) return;
    lattice_union = stored;
    emit latticeSettingsChanged();
}

// ── Cell size and printability thresholds ───────────────────────────────────

void Parameters::setLatticeCellMm(double value)
{
    if (!(value > 0.0)) { qWarning() << "lattice_cell_mm expects a positive size in mm; got" << value; return; }
    if (lattice_cell_mm == value) return;
    lattice_cell_mm = value;
    emit latticeSettingsChanged();
}

void Parameters::setPrintMinWallMm(double value)
{
    if (!(value > 0.0)) { qWarning() << "print_min_wall_mm expects a positive thickness in mm; got" << value; return; }
    if (print_min_wall_mm == value) return;
    print_min_wall_mm = value;
    emit latticeSettingsChanged();
}

void Parameters::setPrintMaxOverhangDeg(double value)
{
    if (value < 0.0 || value > 89.0) { qWarning() << "print_max_overhang_deg expects 0..89 degrees; got" << value; return; }
    if (print_max_overhang_deg == value) return;
    print_max_overhang_deg = value;
    emit latticeSettingsChanged();
}

static bool validFraction(double v) { return v >= 0.0 && v <= 1.0; }

void Parameters::setPrintMaxThinFraction(double value)
{
    if (!validFraction(value)) { qWarning() << "print_max_thin_fraction expects 0..1; got" << value; return; }
    if (print_max_thin_fraction == value) return;
    print_max_thin_fraction = value;
    emit latticeSettingsChanged();
}

void Parameters::setPrintMaxIslandVox(int value)
{
    if (value < 0) { qWarning() << "print_max_island_vox expects a non-negative voxel count; got" << value; return; }
    if (print_max_island_vox == value) return;
    print_max_island_vox = value;
    emit latticeSettingsChanged();
}

void Parameters::setPrintMaxComponents(int value)
{
    if (value < 0) { qWarning() << "print_max_components expects a non-negative count (0 = no limit); got" << value; return; }
    if (print_max_components == value) return;
    print_max_components = value;
    emit latticeSettingsChanged();
}

void Parameters::setPrintMaxUnsupportedFraction(double value)
{
    if (!validFraction(value)) { qWarning() << "print_max_unsupported_fraction expects 0..1; got" << value; return; }
    if (print_max_unsupported_fraction == value) return;
    print_max_unsupported_fraction = value;
    emit latticeSettingsChanged();
}

void Parameters::setRequirePrintable(bool value)
{
    if (require_printable == value) return;
    require_printable = value;
    emit latticeSettingsChanged();
}

PrintabilityThresholds Parameters::printThresholds(double voxel_mm)
{
    PrintabilityThresholds t;
    t.voxel_mm                 = voxel_mm;
    t.min_wall_mm              = print_min_wall_mm;
    t.max_overhang_deg         = print_max_overhang_deg;
    t.max_thin_fraction        = print_max_thin_fraction;
    t.max_island_vox           = print_max_island_vox;
    t.max_components           = print_max_components;
    t.max_unsupported_fraction = print_max_unsupported_fraction;
    t.periodic                 = true;
    return t;
}

void Parameters::setLatticeUniformWall(bool value)
{
    if (lattice_uniform_wall == value) return;
    lattice_uniform_wall = value;
    emit latticeSettingsChanged();
}

void Parameters::setNoiseSmooth(double value)
{
    if (!(value >= 0.005 && value <= 0.5)) { qWarning() << "noise_smooth expects 0.005..0.5 of the cube edge; got" << value; return; }
    if (noise_smooth == value) return;
    noise_smooth = value;
    emit latticeSettingsChanged();
}

// ── Material as numbers ──────────────────────────────────────────────────────

void Parameters::setMatEGpa(double value)
{
    if (value < 0.0) { qWarning() << "mat_e_gpa expects 0 (use the database) or a positive modulus in GPa; got" << value; return; }
    if (mat_e_gpa == value) return;
    mat_e_gpa = value;
    emit latticeSettingsChanged();
}

void Parameters::setMatNu(double value)
{
    if (!(value > -1.0 && value < 0.5)) { qWarning() << "mat_nu expects a Poisson's ratio in (-1, 0.5); got" << value; return; }
    if (mat_nu == value) return;
    mat_nu = value;
    emit latticeSettingsChanged();
}

void Parameters::setMatAniso(double value)
{
    if (!(value >= 0.1 && value <= 3.0)) { qWarning() << "mat_aniso expects E_across / E_in-plane in [0.1, 3]; got" << value; return; }
    if (mat_aniso == value) return;
    mat_aniso = value;
    emit latticeSettingsChanged();
}

void Parameters::setHdf5Fields(bool value)
{
    if (hdf5_fields == value) return;
    hdf5_fields = value;
    emit latticeSettingsChanged();
}

QString Parameters::overrideMaterialName()
{
    return QString("E=%1GPa nu=%2 aniso=%3").arg(mat_e_gpa).arg(mat_nu).arg(mat_aniso);
}

bool Parameters::overrideStiffnessPa(double C[6][6])
{
    if (!materialOverrideActive()) return false;
    const double E  = mat_e_gpa * 1e9;
    const double nu = mat_nu;
    const double r  = mat_aniso;
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) C[i][j] = 0.0;
    if (std::abs(r - 1.0) < 1e-12) {
        isotropicVoigt(E, nu, C);
        return true;
    }
    // Transversely isotropic about axis 3: compliance in Voigt order
    // 11, 22, 33, 23, 13, 12, then inverted.
    const double Ep = E, Et = r * E;
    const double Gp = E / (2.0 * (1.0 + nu)), Gt = r * Gp;
    fftsa::Mat6 S{}; for (auto& row : S) row.fill(0.0);
    S[0][0] = S[1][1] = 1.0 / Ep;
    S[2][2] = 1.0 / Et;
    S[0][1] = S[1][0] = -nu / Ep;
    S[0][2] = S[2][0] = S[1][2] = S[2][1] = -nu / Ep;   // nu_13/E_1 = nu_31/E_3
    S[3][3] = S[4][4] = 1.0 / Gt;
    S[5][5] = 1.0 / Gp;
    fftsa::Mat6 Cm{};
    if (!fftsa::invert6x6(S, Cm)) return false;
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) C[i][j] = Cm[i][j];
    return Cm[0][0] > 0.0 && Cm[2][2] > 0.0 && Cm[3][3] > 0.0;
}

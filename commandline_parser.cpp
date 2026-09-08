#include <QDebug>
#include <omp.h>

#include <cmath>
#include <ctime>
#include "cpuinfo.hpp"
#include "commandline_parser.h"
#include "parameters.h"
#include "texturelibrary.h"

Commandline_Parser::Commandline_Parser()
{

}

void Commandline_Parser::setupParser(QCommandLineParser &parser)
{
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption("size","Set the size of cube", "size"));
    parser.addOption(QCommandLineOption("points","Set the number of points", "points"));
    parser.addOption(QCommandLineOption("concentration","Set the concentration of initial points in the cube(%)", "concentration"));
    parser.addOption(QCommandLineOption("algorithm", "Set the algorithm of generation", "algorithm"));
    parser.addOption(QCommandLineOption("seed","Set the seed of generation","seed"));
    parser.addOption(QCommandLineOption("np", "Set the number of processors for single or multi-threaded execution of algorithms.", "num_threads"));
    parser.addOption(QCommandLineOption("wave_coefficient", "Coefficient for wave generation", "value"));
    parser.addOption(QCommandLineOption("halfaxis_a", "The length of the semi-axis A for the Probability algorithm", "value"));
    parser.addOption(QCommandLineOption("halfaxis_b", "The length of the semi-axis B for the Probability algorithm", "value"));
    parser.addOption(QCommandLineOption("halfaxis_c", "The length of the semi-axis C for the Probability algorithm", "value"));
    parser.addOption(QCommandLineOption("orientation_angle_a", "Rotation angle of the x-axis for the Probability algorithm", "value"));
    parser.addOption(QCommandLineOption("orientation_angle_b", "Rotation angle of the y-axis for the Probability algorithm", "value"));
    parser.addOption(QCommandLineOption("orientation_angle_c", "Rotation angle of the z-axis for the Probability algorithm", "value"));
    parser.addOption(QCommandLineOption("ellipse_order", "The degree of the superellipse equation", "value"));
    parser.addOption(QCommandLineOption("stefan_number", "Thermodynamic Stefan number (cooling limit) for Probability algorithm", "value"));
    parser.addOption(QCommandLineOption("wave_generation", "Enable continuous wave nucleation (transformation-fraction controlled)"));
    parser.addOption(QCommandLineOption("initial_nuclei", "Number of initial nuclei present at step 0 for wave nucleation", "count"));
    parser.addOption(QCommandLineOption("wave_peak_fraction", "Solid volume fraction where nucleation rate peaks (0..1, default 0.20)", "fraction"));
    parser.addOption(QCommandLineOption("wave_end_fraction", "Solid volume fraction where 100% of nuclei are placed (0..1, default 0.60)", "fraction"));
    parser.addOption(QCommandLineOption("prob_preset", "Shape preset for Probability algorithm (e.g. 'Sphere (Circle)', 'Prolate (Needle)', 'Oblate (Disc)', 'Triaxial Ellipsoid', 'Superellipsoid (Cube)')", "preset"));
    parser.addOption(QCommandLineOption("prob_matrix_mode", "Probability matrix calculation method: 'Volume Sampling' (or 'volume') | 'Surface Flux' (or 'surface')", "mode"));
    parser.addOption(QCommandLineOption("minkowski_p",
                                        "Minkowski exponent p for the Voronoi algorithm: 1 = Manhattan "
                                        "(octahedral grains), 2 = Euclidean, large = Chebyshev (cuboidal). "
                                        "Default 2", "value"));
    parser.addOption(QCommandLineOption("periodic",
                                        "Generate a periodic cell: grains wrap across opposite faces"));
    parser.addOption(QCommandLineOption("voronoi_metric_preset",
                                        "Preset for Voronoi shape / metric tensor ('Sphere (Circle)', 'Prolate (Needle)', 'Oblate (Disc)', 'Triaxial Ellipsoid', 'Superellipsoid (Cube)', 'Columnar Z', 'Rolled', 'Sheared')",
                                        "preset"));
    parser.addOption(QCommandLineOption("voronoi_mxx", "Metric tensor component M_xx (M11) for Voronoi algorithm (default 1.0)", "value"));
    parser.addOption(QCommandLineOption("voronoi_myy", "Metric tensor component M_yy (M22) for Voronoi algorithm (default 1.0)", "value"));
    parser.addOption(QCommandLineOption("voronoi_mzz", "Metric tensor component M_zz (M33) for Voronoi algorithm (default 1.0)", "value"));
    parser.addOption(QCommandLineOption("voronoi_mxy", "Metric tensor component M_xy (M12) for Voronoi algorithm (default 0.0)", "value"));
    parser.addOption(QCommandLineOption("voronoi_myz", "Metric tensor component M_yz (M23) for Voronoi algorithm (default 0.0)", "value"));
    parser.addOption(QCommandLineOption("voronoi_mxz", "Metric tensor component M_xz (M13) for Voronoi algorithm (default 0.0)", "value"));
    parser.addOption(QCommandLineOption("voronoi_metric", "Metric tensor components: 'mxx,myy,mzz' or 'mxx,myy,mzz,mxy,myz,mxz'", "mxx,myy,mzz..."));
    // ── Composite (fiber-reinforced RVE) ──────────────────────────────────
    parser.addOption(QCommandLineOption("composite_dim",
                                        "Reinforcement dimensionality for the Composite algorithm: "
                                        "1d = fibers along Z, 2d = along X and Y, 3d = along X, Y and Z. "
                                        "Default 1d", "dim"));
    parser.addOption(QCommandLineOption("composite_packing",
                                        "Fiber arrangement in the cross-section: square | hexagonal. "
                                        "Default square", "packing"));
    parser.addOption(QCommandLineOption("fiber_volume_fraction",
                                        "Target fiber volume fraction (0..1). The fiber semi-axes are "
                                        "solved so the structure actually reaches it; the value is "
                                        "clamped to the packing limit unless --fiber_allow_overlap. "
                                        "Default 0.4", "value"));
    parser.addOption(QCommandLineOption("fibers_per_row",
                                        "Fibers per row in the cross-section lattice. Default 3", "n"));
    parser.addOption(QCommandLineOption("fiber_aspect_ratio",
                                        "Fiber cross-section a/b (major over minor semi-axis); 1 = circular. "
                                        "The area is held fixed, so this changes shape at constant volume "
                                        "fraction. Default 1", "value"));
    parser.addOption(QCommandLineOption("fiber_angle_scatter",
                                        "Per-fiber in-plane rotation of the ellipse, full width in degrees "
                                        "(0 = all aligned, 180 = fully random). Default 0", "degrees"));
    parser.addOption(QCommandLineOption("fiber_center_jitter",
                                        "Random shift of each fiber center, in half-pitches (0 = perfect "
                                        "lattice, 1 = up to half a pitch). Default 0", "value"));
    parser.addOption(QCommandLineOption("fiber_allow_overlap",
                                        "Let jittered fibers overlap and merge instead of rejection-sampling "
                                        "their centers; also lifts the packing limit on the volume fraction"));
    parser.addOption(QCommandLineOption("matrix_material",
                                        "Matrix constituent for the Composite algorithm, from "
                                        "material_properties.db (e.g. Epoxy, Al, Cu). Default Epoxy", "name"));
    parser.addOption(QCommandLineOption("fiber_material",
                                        "Fiber constituent for the Composite algorithm, from "
                                        "material_properties.db (e.g. C-fiber, E-glass, SiC, W). Its axis 3 "
                                        "is aligned with the fiber, so a transversely isotropic row is stiff "
                                        "along the fiber. Default C-fiber", "name"));

    parser.addOption(QCommandLineOption("material",
                                        "Material from material_properties.db (e.g. Cu, Fe, W). Supplies the "
                                        "cubic constants both stress solvers use and the lattice the texture "
                                        "presets are built for", "name"));
    parser.addOption(QCommandLineOption("autostart","Running a program with auto-generation of a cube"));
    parser.addOption(QCommandLineOption("animate",
                                        "Grow the structure iteration by iteration instead of in one shot"));
    parser.addOption(QCommandLineOption("nogui","Running a program with no GUI"));
    parser.addOption(QCommandLineOption("solver","Solver for --run_stress_calc: ansys | fft (default ansys)", "solver"));
    parser.addOption(QCommandLineOption("stress_mode",
                                        "Stress calculation mode: single | dataset | stiffness (default dataset). "
                                        "stiffness computes S/C/P/moduli only (6 solves, no Hill calibration or "
                                        "300-sample run) and writes them to HDF5, same schema as dataset mode.", "mode"));
    parser.addOption(QCommandLineOption("eps",
                                        "Strain tensor for --stress_mode single: exx,eyy,ezz,exy,eyz,exz", "values"));
    parser.addOption(QCommandLineOption("output",
                                        "HDF5 file results are written to (default current_ls.hdf5)", "file"));
    parser.addOption(QCommandLineOption("num_rnd_loads", "Set number of random loads (as eps) for stress analis", "num_rnd_loads"));
    parser.addOption(QCommandLineOption("run_stress_calc", "Run FEM to estimate stresses and strains"));
    parser.addOption(QCommandLineOption("working_directory", "Set path where ansys working directory will be stored","working_directory"));

    // ── Crystallographic texture ──────────────────────────────────────────
    parser.addOption(QCommandLineOption("texture",
                                        "Texture preset: random | extrusion | rolling | recrystallization | shear | scattered_cube", "preset"));
    parser.addOption(QCommandLineOption("lattice",
                                        "Crystal lattice for the texture preset: fcc | bcc (default fcc)", "lattice"));
    parser.addOption(QCommandLineOption("scatter",
                                        "Texture scatter (spread) in degrees, default 11", "degrees"));
}

namespace {

// Maps --texture to TextureLibrary::Process. Returns false on an unknown name.
bool parseProcess(const QString& name, TextureLibrary::Process& out)
{
    const QString n = name.trimmed().toLower();
    if (n == "random")                                     out = TextureLibrary::Process::Random;
    else if (n == "extrusion")                             out = TextureLibrary::Process::Extrusion;
    else if (n == "rolling")                               out = TextureLibrary::Process::Rolling;
    else if (n == "recrystallization" || n == "recryst")   out = TextureLibrary::Process::Recrystallization;
    else if (n == "shear")                                 out = TextureLibrary::Process::Shear;
    else if (n == "scattered_cube" || n == "scatteredcube" ||
             n == "scattered-cube")                        out = TextureLibrary::Process::ScatteredCube;
    else return false;
    return true;
}

bool parseLattice(const QString& name, TextureLibrary::Lattice& out)
{
    const QString n = name.trimmed().toLower();
    if (n == "fcc")      out = TextureLibrary::Lattice::FCC;
    else if (n == "bcc") out = TextureLibrary::Lattice::BCC;
    else return false;
    return true;
}

} // namespace

void Commandline_Parser::processOptions(const QCommandLineParser& parser)
{
    Parameters* params = Parameters::instance();

    // ── Typed parse helpers ───────────────────────────────────────────────
    // Each helper returns true on success so callers can chain or ignore.

    auto parseInt = [&](const QString& opt, auto setter) -> bool {
        if (!parser.isSet(opt)) return false;
        const QString str = parser.value(opt);
        bool ok = false;
        const int value = str.toInt(&ok);
        if (!ok) {
            qFatal("Option --%s expects an integer; got \"%s\"",
                   qPrintable(opt), qPrintable(str));
        }
        setter(value);
        qInfo() << opt << ":" << value;
        return true;
    };

    auto parseFloat = [&](const QString& opt, auto setter) -> bool {
        if (!parser.isSet(opt)) return false;
        const QString str = parser.value(opt);
        bool ok = false;
        const float value = str.toFloat(&ok);
        if (!ok) {
            qFatal("Option --%s expects a float; got \"%s\"",
                   qPrintable(opt), qPrintable(str));
        }
        setter(value);
        qInfo() << opt << ":" << value;
        return true;
    };

    auto parseDouble = [&](const QString& opt, auto setter) -> bool {
        if (!parser.isSet(opt)) return false;
        const QString str = parser.value(opt);
        bool ok = false;
        const double value = str.toDouble(&ok);
        if (!ok) {
            qFatal("Option --%s expects a double; got \"%s\"",
                   qPrintable(opt), qPrintable(str));
        }
        setter(value);
        qInfo() << opt << ":" << value;
        return true;
    };

    auto parseString = [&](const QString& opt, auto setter) -> bool {
        if (!parser.isSet(opt)) return false;
        const QString value = parser.value(opt);
        setter(value);
        qInfo() << opt << ":" << value;
        return true;
    };

    // Positive-value guard: catches --size -5 and --np 0, which used to pass
    // parsing and blow up much later.
    auto requirePositive = [&](const QString& opt, double value) {
        if (value <= 0.0)
            qFatal("Option --%s expects a positive value; got %s",
                   qPrintable(opt), qPrintable(QString::number(value)));
    };

    // ── Cube geometry ─────────────────────────────────────────────────────
    parseInt("size",   [&](int v) { requirePositive("size", v);   params->setSize(v); });
    parseInt("points", [&](int v) { requirePositive("points", v); params->setPoints(v); });

    if (parser.isSet("concentration")) {
        if (parser.isSet("points")) {
            qWarning() << "Both --points and --concentration were given;"
                       << "--concentration wins and --points is ignored";
        }
        const QString str = parser.value("concentration");
        bool ok = false;
        const float pct = str.toFloat(&ok);
        if (!ok) {
            qFatal("Option --concentration expects a float; got \"%s\"",
                   qPrintable(str));
        }
        if (pct <= 0.0f || pct > 100.0f)
            qFatal("Option --concentration expects a value in (0, 100]; got %s",
                   qPrintable(str));

        // Double, and rounded rather than truncated, to match
        // Parameters::processPointInput(): computed in float, 0.375% of 20^3
        // lands on 29.999... and truncated to 29 instead of the exact 30.
        const double volume = std::pow(static_cast<double>(params->getSize()), 3);
        const int derived = static_cast<int>(
            std::lround(static_cast<double>(pct) / 100.0 * volume));
        if (derived <= 0)
            qFatal("Option --concentration resolved to %d points; raise --size or --concentration",
                   derived);

        params->setPoints(derived);
        qInfo() << "concentration:" << pct << "% -> points:" << derived;
    }

    // points must fit inside the cube, otherwise generation misbehaves silently
    if (params->getSize() > 0 && params->getPoints() > std::pow(params->getSize(), 3)) {
        qFatal("Initial points (%d) exceed the cube volume (%.0f); lower --points or raise --size",
               params->getPoints(), std::pow(params->getSize(), 3));
    }

    // ── Probability preset & half-axes ───────────────────────────────────
    parseString("prob_preset",      [&](const QString& v) { params->setProbPreset(v); });

    // ── Ellipsoid half-axes ───────────────────────────────────────────────
    parseFloat("halfaxis_a", [&](float v) { params->setHalfAxisA(v); });
    parseFloat("halfaxis_b", [&](float v) { params->setHalfAxisB(v); });
    parseFloat("halfaxis_c", [&](float v) { params->setHalfAxisC(v); });

    params->setHasProbParameters(
        parser.isSet("halfaxis_a") ||
        parser.isSet("halfaxis_b") ||
        parser.isSet("halfaxis_c") ||
        parser.isSet("orientation_angle_a") ||
        parser.isSet("orientation_angle_b") ||
        parser.isSet("orientation_angle_c"));

    // ── Orientation angles ────────────────────────────────────────────────
    parseFloat("orientation_angle_a", [&](float v) { params->setOrientationAngleA(v); });
    parseFloat("orientation_angle_b", [&](float v) { params->setOrientationAngleB(v); });
    parseFloat("orientation_angle_c", [&](float v) { params->setOrientationAngleC(v); });

    // ── Algorithm options ─────────────────────────────────────────────────
    parseDouble("ellipse_order",    [&](double v) { params->setEllipseOrder(v); });
    parseFloat("stefan_number",     [&](float v)  { params->setStefanNumber(v); });
    parseDouble("minkowski_p",      [&](double v) {
        if (v <= 0.0)
            qFatal("Option --minkowski_p expects a positive value; got %s",
                   qPrintable(QString::number(v)));
        params->setMinkowskiP(v);
    });
    params->setIsPeriodic(parser.isSet("periodic"));

    parseString("voronoi_metric_preset", [&](const QString& v) { params->setVoronoiMetricPreset(v); });
    if (!parser.isSet("voronoi_metric_preset") && parser.isSet("prob_preset") &&
        parser.value("algorithm").compare("Voronoi", Qt::CaseInsensitive) == 0) {
        params->setVoronoiMetricPreset(parser.value("prob_preset"));
    }

    parseDouble("voronoi_mxx", [&](double v) { params->setVoronoiMxx(v); });
    parseDouble("voronoi_myy", [&](double v) { params->setVoronoiMyy(v); });
    parseDouble("voronoi_mzz", [&](double v) { params->setVoronoiMzz(v); });
    parseDouble("voronoi_mxy", [&](double v) { params->setVoronoiMxy(v); });
    parseDouble("voronoi_myz", [&](double v) { params->setVoronoiMyz(v); });
    parseDouble("voronoi_mxz", [&](double v) { params->setVoronoiMxz(v); });

    if (parser.isSet("voronoi_metric")) {
        const QStringList parts = parser.value("voronoi_metric").split(',');
        if (parts.size() >= 3) {
            bool ok1 = false, ok2 = false, ok3 = false;
            double mxx = parts[0].trimmed().toDouble(&ok1);
            double myy = parts[1].trimmed().toDouble(&ok2);
            double mzz = parts[2].trimmed().toDouble(&ok3);
            if (ok1 && ok2 && ok3) {
                params->setVoronoiMxx(mxx);
                params->setVoronoiMyy(myy);
                params->setVoronoiMzz(mzz);
            }
            if (parts.size() >= 6) {
                bool ok4 = false, ok5 = false, ok6 = false;
                double mxy = parts[3].trimmed().toDouble(&ok4);
                double myz = parts[4].trimmed().toDouble(&ok5);
                double mxz = parts[5].trimmed().toDouble(&ok6);
                if (ok4 && ok5 && ok6) {
                    params->setVoronoiMxy(mxy);
                    params->setVoronoiMyz(myz);
                    params->setVoronoiMxz(mxz);
                }
            }
        }
    }

    // ── Composite (fiber-reinforced RVE) ──────────────────────────────────
    parseString("composite_dim", [&](const QString& v) {
        const QString n = v.trimmed().toLower();
        if (!n.startsWith('1') && !n.startsWith('2') && !n.startsWith('3'))
            qFatal("Option --composite_dim expects 1d, 2d or 3d; got \"%s\"", qPrintable(v));
        params->setCompositeDim(v);
    });
    parseString("composite_packing", [&](const QString& v) {
        const QString n = v.trimmed().toLower();
        if (n != "square" && n != "hexagonal" && n != "hex")
            qFatal("Option --composite_packing expects square or hexagonal; got \"%s\"",
                   qPrintable(v));
        params->setCompositePacking(v);
    });
    parseDouble("fiber_volume_fraction", [&](double v) {
        if (v <= 0.0 || v >= 1.0)
            qFatal("Option --fiber_volume_fraction expects a value in (0, 1); got %s",
                   qPrintable(QString::number(v)));
        params->setFiberVolumeFraction(v);
    });
    parseInt("fibers_per_row", [&](int v) {
        requirePositive("fibers_per_row", v);
        params->setFibersPerRow(v);
    });
    parseDouble("fiber_aspect_ratio", [&](double v) {
        if (v <= 0.0)
            qFatal("Option --fiber_aspect_ratio expects a positive value; got %s",
                   qPrintable(QString::number(v)));
        params->setFiberAspectRatio(v);
    });
    parseDouble("fiber_angle_scatter", [&](double v) {
        if (v < 0.0 || v > 180.0)
            qFatal("Option --fiber_angle_scatter expects degrees in [0, 180]; got %s",
                   qPrintable(QString::number(v)));
        params->setFiberAngleScatter(v);
    });
    parseDouble("fiber_center_jitter", [&](double v) {
        if (v < 0.0 || v > 1.0)
            qFatal("Option --fiber_center_jitter expects a value in [0, 1]; got %s",
                   qPrintable(QString::number(v)));
        params->setFiberCenterJitter(v);
    });
    params->setFiberAllowOverlap(parser.isSet("fiber_allow_overlap"));
    parseString("matrix_material", [&](const QString& v) { params->setMatrixMaterial(v); });
    parseString("fiber_material",  [&](const QString& v) { params->setFiberMaterial(v); });

    if (parser.isSet("animate"))
        params->setIsAnimation(true);
    if (parser.isSet("wave_generation"))
        params->setIsWaveGeneration(true);
    parseInt   ("initial_nuclei",      [&](int    v) { params->setInitialNucleiCount(v); });
    parseFloat ("wave_peak_fraction",  [&](float  v) { params->setWavePeakFraction(v); });
    parseFloat ("wave_end_fraction",   [&](float  v) { params->setWaveEndFraction(v); });
    parseFloat ("wave_coefficient",    [&](float  v) { params->setWaveCoefficient(v); });

    if (!parser.isSet("wave_coefficient"))
        params->setWaveCoefficient(0.1f);

    if (parser.isSet("prob_matrix_mode")) {
        const QString m = parser.value("prob_matrix_mode").trimmed().toLower();
        if (m == "surface" || m == "surface flux" || m == "surface_flux")
            params->setProbMatrixMode("Surface Flux");
        else
            params->setProbMatrixMode("Volume Sampling");
    }

    parseString("algorithm", [&](const QString& v) { params->setAlgorithm(v); });

    // ── RNG seed ──────────────────────────────────────────────────────────
    // toUInt, not toInt: the seed is unsigned and values above 2^31-1 are legal.
    if (parser.isSet("seed")) {
        bool ok = false;
        const unsigned int seed = parser.value("seed").toUInt(&ok);
        if (!ok)
            qFatal("Option --seed expects a non-negative integer; got \"%s\"",
                   qPrintable(parser.value("seed")));
        params->setSeed(seed);
    } else {
        params->setSeed(static_cast<unsigned int>(std::time(nullptr)));
    }
    qInfo() << "Random seed:" << params->getSeed();

    // ── Threading ─────────────────────────────────────────────────────────
    if (parser.isSet("np")) {
        bool ok = false;
        const int np = parser.value("np").toInt(&ok);
        if (!ok) qFatal("Option --np expects an integer");
        requirePositive("np", np);
        params->setNumThreads(np);
    } else {
        int cores = CpuInfo::getPhysicalCores();
        params->setNumThreads(cores > 0 ? cores : omp_get_max_threads());
        qDebug() << "Physical CPU cores:" << params->getNumThreads();
    }
    qInfo() << "Number of threads:" << params->getNumThreads();

    // ── Material ──────────────────────────────────────────────────────────
    // Before the texture block: the material's Type column decides which
    // lattice the presets are built for, unless --lattice overrides it.
    parseString("material", [&](const QString& v) { params->setDbMaterial(v); });

    if (parser.isSet("lattice")) {
        TextureLibrary::Lattice lat;
        if (!parseLattice(parser.value("lattice"), lat))
            qFatal("Option --lattice expects fcc or bcc; got \"%s\"",
                   qPrintable(parser.value("lattice")));
        params->setLatticeOverride(parser.value("lattice").trimmed().toLower());
    }

    // ── Crystallographic texture ──────────────────────────────────────────
    // Fills the same Parameters::textureComponents that the Texture Editor writes,
    // so ansysWrapper and the viewport pick it up through the usual path.
    if (parser.isSet("texture")) {
        TextureLibrary::Process proc;
        if (!parseProcess(parser.value("texture"), proc)) {
            qFatal("Option --texture expects one of: random, extrusion, rolling, "
                   "recrystallization, shear, scattered_cube; got \"%s\"",
                   qPrintable(parser.value("texture")));
        }

        if (parser.isSet("scatter")) {
            bool ok = false;
            const double scatter = parser.value("scatter").toDouble(&ok);
            if (!ok || scatter < 0.0)
                qFatal("Option --scatter expects a non-negative number of degrees; got \"%s\"",
                       qPrintable(parser.value("scatter")));
            params->setTextureScatter(scatter);
        }

        // Routed through Parameters (rather than writing textureComponents
        // directly) so the GUI panel shows the preset the CLI selected.
        params->setTexturePreset(parser.value("texture"));

        const bool bcc = Parameters::materialLattice() == TextureLibrary::Lattice::BCC;
        qInfo() << "texture:" << parser.value("texture")
                << " lattice:" << (bcc ? "bcc" : "fcc")
                << " scatter:" << params->getTextureScatter() << "deg"
                << " components:" << Parameters::textureComponents.size();
    } else {
        if (parser.isSet("lattice") || parser.isSet("scatter"))
            qWarning() << "--lattice and --scatter have no effect without --texture";
        Parameters::textureComponents.clear();
    }

    if (parser.isSet("num_rnd_loads")) {
        bool ok = false;
        const unsigned int n = parser.value("num_rnd_loads").toUInt(&ok);
        if (!ok)
            qFatal("Option --num_rnd_loads expects a non-negative integer; got \"%s\"",
                   qPrintable(parser.value("num_rnd_loads")));
        params->setNumRndLoads(n);
        qInfo() << "num_rnd_loads :" << n;
    }

    if (parser.isSet("solver")) {
        const QString s = parser.value("solver").trimmed().toLower();
        if (s != "ansys" && s != "fft")
            qFatal("Option --solver expects ansys or fft; got \"%s\"",
                   qPrintable(parser.value("solver")));
        params->setStressSolver(s);
        qInfo() << "solver :" << s;
    }

    if (parser.isSet("stress_mode")) {
        const QString m = parser.value("stress_mode").trimmed().toLower();
        if (m != "single" && m != "dataset" && m != "stiffness")
            qFatal("Option --stress_mode expects single, dataset or stiffness; got \"%s\"",
                   qPrintable(parser.value("stress_mode")));
        params->setStressMode(m);
        qInfo() << "stress_mode :" << m;
    }

    if (parser.isSet("eps")) {
        const QStringList parts = parser.value("eps").split(',');
        if (parts.size() != 6)
            qFatal("Option --eps expects 6 comma-separated values, got %d", parts.size());
        double e[6];
        for (int i = 0; i < 6; ++i) {
            bool ok = false;
            e[i] = parts[i].trimmed().toDouble(&ok);
            if (!ok)
                qFatal("Option --eps: component %d is not a number: \"%s\"",
                       i + 1, qPrintable(parts[i]));
        }
        params->setStressEps(e);
    }

    parseString("output", [&](const QString& v) {
        params->setFilename(v);
    });

    parseString("working_directory", [&](const QString& v) {
        params->setWorkingDirectory(v);
    });
}

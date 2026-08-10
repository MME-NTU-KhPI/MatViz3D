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
    parser.addOption(QCommandLineOption("autostart","Running a program with auto-generation of a cube"));
    parser.addOption(QCommandLineOption("nogui","Running a program with no GUI"));
    parser.addOption(QCommandLineOption("solver","Solver for --run_stress_calc: ansys | fft (default ansys)", "solver"));
    parser.addOption(QCommandLineOption("stress_mode",
                                        "Stress calculation mode: single | dataset | stiffness (default dataset). "
                                        "stiffness computes S/C/P/moduli only (6 solves, no Hill calibration or "
                                        "300-sample run) and writes them to HDF5, same schema as dataset mode.", "mode"));
    parser.addOption(QCommandLineOption("eps",
                                        "Strain tensor for --stress_mode single: exx,eyy,ezz,exy,eyz,exz", "values"));
    parser.addOption(QCommandLineOption("output", "Specify output file for generated cube", "directory"));
    parser.addOption(QCommandLineOption("num_rnd_loads", "Set number of random loads (as eps) for stress analis", "num_rnd_loads"));
    parser.addOption(QCommandLineOption("run_stress_calc", "Run FEM to estimate stresses and strains"));
    parser.addOption(QCommandLineOption("working_directory", "Set path where ansys working directory will be stored","working_directory"));

    // ── Crystallographic texture ──────────────────────────────────────────
    parser.addOption(QCommandLineOption("texture",
                                        "Texture preset: random | extrusion | rolling | recrystallization | shear", "preset"));
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

        const int derived = static_cast<int>(
            (pct / 100.0f) * std::pow(params->getSize(), 3));
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
    parseFloat ("wave_coefficient", [&](float  v) { params->setWaveCoefficient(v); });

    if (!parser.isSet("wave_coefficient"))
        params->setWaveCoefficient(0.1f);

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

    // ── Crystallographic texture ──────────────────────────────────────────
    // Fills the same Parameters::textureComponents that the Texture Editor writes,
    // so ansysWrapper and the viewport pick it up through the usual path.
    if (parser.isSet("texture")) {
        TextureLibrary::Process proc;
        if (!parseProcess(parser.value("texture"), proc)) {
            qFatal("Option --texture expects one of: random, extrusion, rolling, "
                   "recrystallization, shear; got \"%s\"",
                   qPrintable(parser.value("texture")));
        }

        TextureLibrary::Lattice lat = TextureLibrary::Lattice::FCC;
        if (parser.isSet("lattice") && !parseLattice(parser.value("lattice"), lat)) {
            qFatal("Option --lattice expects fcc or bcc; got \"%s\"",
                   qPrintable(parser.value("lattice")));
        }

        double scatter = 11.0;
        if (parser.isSet("scatter")) {
            bool ok = false;
            scatter = parser.value("scatter").toDouble(&ok);
            if (!ok || scatter < 0.0)
                qFatal("Option --scatter expects a non-negative number of degrees; got \"%s\"",
                       qPrintable(parser.value("scatter")));
        }

        Parameters::textureComponents =
            TextureLibrary::componentsForProcess(proc, lat, scatter);

        qInfo() << "texture:" << parser.value("texture")
                << " lattice:" << (lat == TextureLibrary::Lattice::FCC ? "fcc" : "bcc")
                << " scatter:" << scatter << "deg"
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

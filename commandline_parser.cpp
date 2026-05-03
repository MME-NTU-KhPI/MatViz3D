#include <QDebug>
#include <omp.h>

#include "cpuinfo.hpp"
#include "commandline_parser.h"
//#include "stressanalysis.h"
#include "parameters.h"

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
    parser.addOption(QCommandLineOption("output", "Specify output file for generated cube", "directory"));
    parser.addOption(QCommandLineOption("num_rnd_loads", "Set number of random loads (as eps) for stress analis", "num_rnd_loads"));
    parser.addOption(QCommandLineOption("run_stress_calc", "Run FEM to estimate stresses and strains"));
    parser.addOption(QCommandLineOption("working_directory", "Set path where ansys working directory will be stored","working_directory"));
}
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

    // ── Cube geometry ─────────────────────────────────────────────────────
    parseInt("size",   [&](int v) { params->setSize(v); });
    parseInt("points", [&](int v) { params->setPoints(v); });

    if (parser.isSet("concentration")) {
        const QString str = parser.value("concentration");
        bool ok = false;
        const float pct = str.toFloat(&ok);
        if (!ok) {
            qFatal("Option --concentration expects a float; got \"%s\"",
                   qPrintable(str));
        }
        const int derived = static_cast<int>(
            (pct / 100.0f) * std::pow(params->getSize(), 3));
        params->setPoints(derived);
        qInfo() << "concentration:" << pct << "% -> points:" << derived;
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
    if (parser.isSet("seed")) {
        bool ok = false;
        const unsigned int seed =
            static_cast<unsigned int>(parser.value("seed").toInt(&ok));
        if (!ok)
            qFatal("Option --seed expects an integer");
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
        params->setNumThreads(np);
    } else {
        int cores = CpuInfo::getPhysicalCores();
        params->setNumThreads(cores > 0 ? cores : omp_get_max_threads());
        qDebug() << "Physical CPU cores:" << params->getNumThreads();
    }
    qInfo() << "Number of threads:" << params->getNumThreads();

    // ── Output / paths ────────────────────────────────────────────────────
    // TODO fix rnd_loads
    //    parseInt("num_rnd_loads", [&](int v) {
    //       params->setNumRndLoads(std::max(0, v));
    //    });

    parseString("output", [&](const QString& v) {
        params->setFilename(v);
    });

    parseString("working_directory", [&](const QString& v) {
        params->setWorkingDirectory(v);
    });
}

#include "console.h"
#include "stressanalysis.h"
#include "QDebug"

Console::Console()
{

}

void Console::setupParser(QCommandLineParser &parser)
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
    parser.addOption(QCommandLineOption("autostart","Running a program with auto-generation of a cube"));
    parser.addOption(QCommandLineOption("nogui","Running a program with no GUI"));
    parser.addOption(QCommandLineOption("output", "Specify output file for generated cube", "directory"));
    parser.addOption(QCommandLineOption("num_rnd_loads", "Set number of random loads (as eps) for stress analis", "num_rnd_loads"));
    parser.addOption(QCommandLineOption("run_stress_calc", "Run FEM to estimate stresses and strains"));
    parser.addOption(QCommandLineOption("working_directory", "Set path where ansys working directory will be stored","working_directory"));
}

void Console::processOptions(const QCommandLineParser &parser, MainWindow &window)
{
    if (parser.isSet("size"))
    {
        QString str = parser.value("size");
        Parameters::size = str.toInt();
        qInfo() << "size:" << Parameters::size;
        window.setNumCubes(Parameters::size);
    }
    else
    {
        qInfo() << "size:" << Parameters::size;
        window.setNumCubes(Parameters::size);
    }
    if (parser.isSet("points"))
    {
        QString str = parser.value("points");
        Parameters::points = str.toInt();
        qInfo() << "points:" << Parameters::points;
        window.setNumColors(Parameters::points);
    }
    if (parser.isSet("concentration"))
    {
        QString str = parser.value("concentration");
        float PercentOfConcentration = str.toFloat()/100;
        Parameters::points = PercentOfConcentration*std::pow(Parameters::size,3);
        qInfo() << "concentration:" << PercentOfConcentration;
        qInfo() << "points:" << Parameters::points;
        window.setNumColors(Parameters::points);
    }
    if (parser.isSet("halfaxis_a"))
    {
        QString str = parser.value("halfaxis_a");
        Parameters::halfaxis_a = str.toFloat();
        qInfo() << "halfaxis_a:" << Parameters::halfaxis_a;
    }

    if (parser.isSet("halfaxis_b"))
    {
        QString str = parser.value("halfaxis_b");
        Parameters::halfaxis_b = str.toFloat();
        qInfo() << "halfaxis_b:" << Parameters::halfaxis_b;
    }

    if (parser.isSet("halfaxis_c"))
    {
        QString str = parser.value("halfaxis_c");
        Parameters::halfaxis_c = str.toFloat();
        qInfo() << "halfaxis_c:" << Parameters::halfaxis_c;
    }

    if (parser.isSet("orientation_angle_a"))
    {
        QString str = parser.value("orientation_angle_a");
        Parameters::orientation_angle_a = str.toFloat();
        qInfo() << "orientation_angle_a:" << Parameters::orientation_angle_a;
    }

    if (parser.isSet("orientation_angle_b"))
    {
        QString str = parser.value("orientation_angle_b");
        Parameters::orientation_angle_b = str.toFloat();
        qInfo() << "orientation_angle_b:" << Parameters::orientation_angle_b;
    }

    if (parser.isSet("orientation_angle_c"))
    {
        QString str = parser.value("orientation_angle_c");
        Parameters::orientation_angle_c = str.toFloat();
        qInfo() << "orientation_angle_c:" << Parameters::orientation_angle_c;
    }

    if (parser.isSet("wave_coefficient"))
    {
        QString str = parser.value("wave_coefficient");
        Parameters::wave_coefficient = str.toFloat();
        qInfo() << "wave_coefficient:" << Parameters::wave_coefficient;
    }
    if (parser.isSet("algorithm"))
    {
        Parameters::algorithm = parser.value("algorithm");
        qInfo() << "algorithm:" << Parameters::algorithm;

        window.setAlgorithms(Parameters::algorithm);
        if (Parameters::algorithm == "prob_circle")
        {
            window.setAlgorithms("Probability Circle");
        }
        if (Parameters::algorithm == "prob_ellipse")
        {
            window.setAlgorithms("Probability Ellipse");
        }
    }
    if (parser.isSet("seed"))
    {
        QString str = parser.value("seed");
        Parameters::seed = str.toInt();
    }
    else
    {
        Parameters::seed = static_cast<unsigned int>(std::time(nullptr));
    }
    qInfo() << "Random seed value is set to:" << Parameters::seed;
    if (parser.isSet("np"))
    {
        QString str = parser.value("np");
        Parameters::num_threads = str.toInt();
    }
    else
    {
        Parameters::num_threads = omp_get_max_threads();
    }
    qInfo() << "Number of threads:" << Parameters::num_threads;
    if (parser.isSet("autostart"))
    {
        qInfo() << "autostart:" << true;
        window.onStartClicked();
    }

    if (parser.isSet("num_rnd_loads"))
    {
        int val = parser.value("num_rnd_loads").toInt();
        val = val >= 0 ? val : 0;
        Parameters::num_rnd_loads = val;
        qInfo() << "num_rnd_loads:" << Parameters::num_rnd_loads;
    }

    if (parser.isSet("output"))
    {
        Parameters::filename = parser.value("output");
        qInfo() << "output:" << Parameters::filename;
        if (Parameters::filename.endsWith(".csv"))
            window.callExportToCSV();
    }

    if (parser.isSet("working_directory"))
    {
        Parameters::working_directory = parser.value("working_directory");
        qInfo() << "working_directory:" << Parameters::working_directory;
    }

    if (parser.isSet("run_stress_calc"))
    {
        qInfo() << "run_stress_calc:" << true;
        StressAnalysis sa;
        sa.estimateStressWithANSYS(Parameters::size, Parameters::points, Parameters::voxels);
    }
    if (parser.isSet("nogui"))
    {
        qInfo() << "nogui:" << true << "Termination application with code 0";
        QApplication::exit(0); // terminating app -- all is done
    }
    else
    {
        window.update();
        window.show();
    }
}

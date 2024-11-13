
#include "console.h"
#include "stressanalysis.h"
#include "QDebug"

Console::Console()
{

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

    if (parser.isSet("wave_coefficient"))
    {
        QString str = parser.value("wave_coefficient");
        Parameters::wave_coefficient = str.toFloat();
        qInfo() << "wave_coefficient:" << Parameters::wave_coefficient;
    }

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
        exit(0); // terminating app -- all is done
    }
    else
    {
        window.update();
        window.show();
    }
}

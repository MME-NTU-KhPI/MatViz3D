
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
        window.setNumCubes(Parameters::size);
    }
    if (parser.isSet("points"))
    {
        QString str = parser.value("points");
        Parameters::points = str.toInt();
        window.setNumColors(Parameters::points);
    }
    if (parser.isSet("concentration"))
    {
        QString str = parser.value("concentration");
        float PercentOfConcentration = str.toFloat()/100;
        Parameters::points = PercentOfConcentration*std::pow(Parameters::size,3);
        window.setNumColors(Parameters::points);
    }
    if (parser.isSet("algorithm"))
    {
        Parameters::algorithm = parser.value("algorithm");
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
        Parameters::seed = static_cast<unsigned int>(std::time(nullptr));
//    if (!parser.isSet("nogui"))
//    {
//        window.show();
//    }
    if (parser.isSet("autostart"))
    {
        window.onStartClicked();
    }

    if (parser.isSet("num_rnd_loads"))
    {
        int val = parser.value("num_rnd_loads").toInt();
        val = val >= 0 ? val : 0;
        Parameters::num_rnd_loads = val;
    }

    if (parser.isSet("output"))
    {
        Parameters::filename = parser.value("output");
        if (Parameters::filename.endsWith(".csv"))
            window.callExportToCSV();
    }

    if (parser.isSet("run_stress_calc"))
    {
        StressAnalysis sa;
        sa.estimateStressWithANSYS(Parameters::size, Parameters::points, Parameters::voxels);
    }
    if (parser.isSet("nogui"))
    {
        exit(0);
    }
    else {
        window.show();
    }
}

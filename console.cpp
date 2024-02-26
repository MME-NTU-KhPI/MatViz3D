
#include "console.h"

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
    if (!parser.isSet("nogui"))
    {
        window.show();
        qDebug() << "Run app with no GUI ";
    }
    if (parser.isSet("autostart"))
    {
        window.onStartClicked();
    }
    if (parser.isSet("output"))
    {
        Parameters::filename = parser.value("output");
        window.callExportToCSV();
    }
}

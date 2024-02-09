#include "mainwindow.h"
#include "parameters.h"
#include <QCommandLineParser>
#include <QApplication>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName("MatViz3D");
    QApplication::setApplicationVersion("2.01");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption sizeOption(QStringList() << "s" << "size", "Set the size", "size");
    QCommandLineOption pointsOption(QStringList() << "p" << "points", "Set the number of points", "points");
    QCommandLineOption algorithmOption(QStringList() << "a" << "algorithm", "Set the algorithm", "algorithm");;
    QCommandLineOption noGUIOption("nogui","Open app with no GUI");
    QCommandLineOption outputOption(QStringList() << "o" << "output", "Specify output file", "filename");
    QCommandLineOption autoStartOption("autostart", "Specify output file");
    parser.addOptions({sizeOption, pointsOption, algorithmOption, noGUIOption, outputOption, autoStartOption});
    parser.process(a);
    MainWindow w;
    w.show();
    if (parser.isSet(sizeOption))
    {
        QString str = parser.value(sizeOption);
        Parameters::size = str.toInt();
        w.setNumCubes(Parameters::size);
    }
    if (parser.isSet(pointsOption))
    {
        QString str = parser.value(pointsOption);
        Parameters::points = str.toInt();
        w.setNumColors(Parameters::points);
    }
    if (parser.isSet(algorithmOption))
    {
        Parameters::algorithm = parser.value(algorithmOption);
        w.setAlgorithms(Parameters::algorithm);
    }
    if (parser.isSet(outputOption))
    {
        Parameters::filename = parser.value(outputOption);

    }
    if (parser.isSet(noGUIOption))
    {
        w.close();
        w.onStartClicked();
    }
    if (parser.isSet(autoStartOption))
    {
        w.onStartClicked();
    }
    return a.exec();

        return 0;
}

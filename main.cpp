#include "mainwindow.h"
#include <QCommandLineParser>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName("MatViz3D");
    QApplication::setApplicationVersion("2.01");
    QCommandLineParser parser;
    short int size = 0;
    short int points = 0;
    unsigned int generations = 1;
    QString algorithm;
    QString filename;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption sizeOption(QStringList() << "s" << "size", "Set the size", "size");
    QCommandLineOption pointsOption(QStringList() << "p" << "points", "Set the number of points", "points");
    QCommandLineOption algorithmOption(QStringList() << "a" << "algorithm", "Set the algorithm", "algorithm");;
    QCommandLineOption generationsOption(QStringList() << "g" << "generations", "Enter the numbers of generation", "generations");
    QCommandLineOption noGUIOption("nogui","Open app with no GUI");
    QCommandLineOption GUIOption("gui","Open app with GUI");
    QCommandLineOption outputOption(QStringList() << "o" << "output", "Specify output file", "filename");
    parser.addOptions({sizeOption, pointsOption, algorithmOption, generationsOption, noGUIOption, GUIOption, outputOption});
    parser.process(a);
    MainWindow w;
    //w.show();
    if (parser.isSet(noGUIOption))
    {
        w.close();
    }
    if (parser.isSet(sizeOption))
    {
        const QString str = parser.value(sizeOption);
        size = str.toShort();
    }
    if (parser.isSet(pointsOption))
    {
        const QString str = parser.value(pointsOption);
        points = str.toShort();
    }
    if (parser.isSet(algorithmOption))
    {
        algorithm = parser.value(algorithmOption);
    }
    if (parser.isSet(outputOption))
    {
        filename = parser.value(outputOption);
    }
    if (parser.isSet(generationsOption))
    {
        const QString str = parser.value(generationsOption);
        generations = str.toUInt();
    }

    return a.exec();

        return 0;
}

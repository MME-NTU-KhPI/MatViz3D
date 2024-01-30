#include "mainwindow.h"
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
    QCommandLineOption helloOption("hello","Say hello!");
    QCommandLineOption sizeOption("size","Enter size of cube");
    QCommandLineOption pointsOption("points","Enter number of points");
    QCommandLineOption algorithmOption("algorithm","Choose algorithm to generate");
    QCommandLineOption generationsOption("generations", "Enter the numbers of generation");
    QCommandLineOption GUIOption("gui","Remove GUI in app");
    QCommandLineOption exportOption("export","Choose export");
    parser.addOptions({helloOption, sizeOption, pointsOption, algorithmOption, generationsOption, GUIOption, exportOption});
    parser.process(a);
    MainWindow w;
    if (parser.isSet(GUIOption))
        w.show();
    if (parser.isSet(generationsOption))
    {

    }
    if (parser.isSet(sizeOption))
    {

    }
    return a.exec();

        return 0;
}

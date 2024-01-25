
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
    parser.process(a);
    MainWindow w;
    w.show();
    return a.exec();

        return 0;
}

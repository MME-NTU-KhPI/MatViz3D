
#ifndef CONSOLE_H
#define CONSOLE_H

#include <QCommandLineParser>
#include "mainwindow.h"
#include "parameters.h"

class Console
{
public:
    Console();
    static void setupParser(QCommandLineParser &parser);
    static void processOptions(const QCommandLineParser &parser, MainWindow &window);
};

#endif // CONSOLE_H

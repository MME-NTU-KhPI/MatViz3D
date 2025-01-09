
#ifndef CONSOLE_H
#define CONSOLE_H

#include <QCommandLineParser>
#include "mainwindow.h"

class Console : public QObject
{
public:
    Console();
    static void processOptions(const QCommandLineParser &parser, MainWindow &window);
};

#endif // CONSOLE_H

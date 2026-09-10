#ifndef CONSOLE_H
#define CONSOLE_H

#include <QCommandLineParser>

class Commandline_Parser
{
public:
    Commandline_Parser();
    static void setupParser(QCommandLineParser &parser);
    static void processOptions(const QCommandLineParser &parser);
    static QString buildApplicationDescription();
    static void printJsonHelp();
};

#endif // CONSOLE_H

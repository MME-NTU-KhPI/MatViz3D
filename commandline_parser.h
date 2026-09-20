#ifndef CONSOLE_H
#define CONSOLE_H

#include <QCommandLineParser>
#include "texturelibrary.h"

class Commandline_Parser
{
public:
    Commandline_Parser();
    static void setupParser(QCommandLineParser &parser);
    static void processOptions(const QCommandLineParser &parser);
    static QString buildApplicationDescription();
    static void printJsonHelp();

    static bool parseProcess(const QString& name, TextureLibrary::Process& out);
    static bool parseLattice(const QString& name, TextureLibrary::Lattice& out);
    static bool isValidCompositeDim(const QString& v);
    static bool isValidCompositePacking(const QString& v);
    static bool isValidSolver(const QString& v);
    static bool isValidStressMode(const QString& v);
};

#endif // CONSOLE_H

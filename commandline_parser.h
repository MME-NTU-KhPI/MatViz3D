#ifndef CONSOLE_H
#define CONSOLE_H

#include <QCommandLineParser>
#include "texturelibrary.h"

class Commandline_Parser
{
public:
    Commandline_Parser();
    static void setupParser(QCommandLineParser &parser);
    static bool processOptions(const QCommandLineParser &parser, QString *error = nullptr);
    static QString buildApplicationDescription();
    static void printJsonHelp();

    static bool parseProcess(const QString& name, TextureLibrary::Process& out);
    static bool parseLattice(const QString& name, TextureLibrary::Lattice& out);
    static bool isValidCompositeDim(const QString& v);
    static bool isValidCompositePacking(const QString& v);
    static bool isValidSolver(const QString& v);
    static bool isValidStressMode(const QString& v);

    static bool applyParameter(const QString& key, const QString& value, QString* error = nullptr);
};

bool applyParameter(const QString& key, const QString& value, QString* error = nullptr);

#endif // CONSOLE_H

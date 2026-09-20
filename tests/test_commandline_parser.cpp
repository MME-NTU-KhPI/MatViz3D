#include "test_commandline_parser.h"
#include <QTest>
#include <QCommandLineParser>
#include <setjmp.h>
#include "commandline_parser.h"
#include "parameters.h"
#include "texturelibrary.h"

namespace {

static jmp_buf s_fatalJmp;
static bool s_expectFatal = false;
static QString s_lastFatalMsg;

void testFatalMessageHandler(QtMsgType type, const QMessageLogContext& /*context*/, const QString& msg)
{
    if (type == QtFatalMsg) {
        s_lastFatalMsg = msg;
        if (s_expectFatal) {
            longjmp(s_fatalJmp, 1);
        }
    }
}

} // namespace

void TestCommandlineParser::init()
{
    Parameters::resetDefaults();
    Parameters::seed = 1001;
    s_expectFatal = false;
    s_lastFatalMsg.clear();
    qInstallMessageHandler(testFatalMessageHandler);
}

void TestCommandlineParser::cleanup()
{
    Parameters::resetDefaults();
    qInstallMessageHandler(nullptr);
}

void TestCommandlineParser::testParseProcessEnumDirect()
{
    TextureLibrary::Process proc;

    // Valid processes
    QVERIFY(Commandline_Parser::parseProcess("random", proc));
    QCOMPARE(proc, TextureLibrary::Process::Random);

    QVERIFY(Commandline_Parser::parseProcess("extrusion", proc));
    QCOMPARE(proc, TextureLibrary::Process::Extrusion);

    QVERIFY(Commandline_Parser::parseProcess("rolling", proc));
    QCOMPARE(proc, TextureLibrary::Process::Rolling);

    QVERIFY(Commandline_Parser::parseProcess("recrystallization", proc));
    QCOMPARE(proc, TextureLibrary::Process::Recrystallization);

    QVERIFY(Commandline_Parser::parseProcess("recryst", proc));
    QCOMPARE(proc, TextureLibrary::Process::Recrystallization);

    QVERIFY(Commandline_Parser::parseProcess("shear", proc));
    QCOMPARE(proc, TextureLibrary::Process::Shear);

    QVERIFY(Commandline_Parser::parseProcess("scattered_cube", proc));
    QCOMPARE(proc, TextureLibrary::Process::ScatteredCube);

    QVERIFY(Commandline_Parser::parseProcess("scatteredcube", proc));
    QCOMPARE(proc, TextureLibrary::Process::ScatteredCube);

    QVERIFY(Commandline_Parser::parseProcess("scattered-cube", proc));
    QCOMPARE(proc, TextureLibrary::Process::ScatteredCube);

    // Invalid processes must be rejected
    QCOMPARE(Commandline_Parser::parseProcess("unknown_process", proc), false);
    QCOMPARE(Commandline_Parser::parseProcess("invalid", proc), false);
    QCOMPARE(Commandline_Parser::parseProcess("", proc), false);
}

void TestCommandlineParser::testParseLatticeEnumDirect()
{
    TextureLibrary::Lattice lat;

    // Valid lattices
    QVERIFY(Commandline_Parser::parseLattice("fcc", lat));
    QCOMPARE(lat, TextureLibrary::Lattice::FCC);

    QVERIFY(Commandline_Parser::parseLattice("bcc", lat));
    QCOMPARE(lat, TextureLibrary::Lattice::BCC);

    QVERIFY(Commandline_Parser::parseLattice("  FCC  ", lat));
    QCOMPARE(lat, TextureLibrary::Lattice::FCC);

    // Invalid lattices must be rejected
    QCOMPARE(Commandline_Parser::parseLattice("hcp", lat), false);
    QCOMPARE(Commandline_Parser::parseLattice("sc", lat), false);
    QCOMPARE(Commandline_Parser::parseLattice("cubic", lat), false);
    QCOMPARE(Commandline_Parser::parseLattice("", lat), false);
}

void TestCommandlineParser::testValidOptionsSetParameters()
{
    Parameters* params = Parameters::instance();

    auto parseArgs = [](const QStringList& args) {
        QCommandLineParser parser;
        Commandline_Parser::setupParser(parser);
        QStringList fullArgs = { "MatViz3D" };
        fullArgs.append(args);
        bool parsed = parser.parse(fullArgs);
        QVERIFY(parsed);
        Commandline_Parser::processOptions(parser);
    };

    // 1. Texture + scatter
    parseArgs({ "--texture", "rolling", "--scatter", "7.5" });
    QCOMPARE(params->getTexturePreset(), QString("Rolling"));
    QCOMPARE(params->getTextureScatter(), 7.5);

    // 2. Solver + stress_mode
    parseArgs({ "--solver", "fft", "--stress_mode", "stiffness" });
    QCOMPARE(params->getStressSolver(), QString("fft"));
    QCOMPARE(params->getStressMode(), QString("stiffness"));

    // 3. Composite dimensions and packing
    parseArgs({ "--composite_dim", "2d", "--composite_packing", "hexagonal" });
    QVERIFY(params->getCompositeDim().contains("2D"));
    QCOMPARE(params->getCompositePacking().toLower(), QString("hexagonal"));

    // 4. Probability matrix mode: surface vs volume
    parseArgs({ "--prob_matrix_mode", "surface" });
    QCOMPARE(params->getProbMatrixMode(), QString("Surface Flux"));

    parseArgs({ "--prob_matrix_mode", "volume" });
    QCOMPARE(params->getProbMatrixMode(), QString("Volume Sampling"));

    // 5. Polycrystall neighborhood
    parseArgs({ "--neighborhood", "von Neumann (6)" });
    QCOMPARE(params->getPolycrystallNeighborhood(), QString("von Neumann (6)"));

    parseArgs({ "--neighborhood", "Radial (18)" });
    QCOMPARE(params->getPolycrystallNeighborhood(), QString("Radial (18)"));
}

void TestCommandlineParser::testInvalidOptionsRejected()
{
    // Invalid composite_dim
    QCOMPARE(Commandline_Parser::isValidCompositeDim("4d"), false);
    QCOMPARE(Commandline_Parser::isValidCompositeDim("xyz"), false);
    QCOMPARE(Commandline_Parser::isValidCompositeDim(""), false);

    // Invalid composite_packing
    QCOMPARE(Commandline_Parser::isValidCompositePacking("triangle"), false);
    QCOMPARE(Commandline_Parser::isValidCompositePacking("circle"), false);
    QCOMPARE(Commandline_Parser::isValidCompositePacking(""), false);

    // Invalid solver
    QCOMPARE(Commandline_Parser::isValidSolver("abaqus"), false);
    QCOMPARE(Commandline_Parser::isValidSolver("fem"), false);
    QCOMPARE(Commandline_Parser::isValidSolver(""), false);

    // Invalid stress_mode
    QCOMPARE(Commandline_Parser::isValidStressMode("full"), false);
    QCOMPARE(Commandline_Parser::isValidStressMode("batch"), false);
    QCOMPARE(Commandline_Parser::isValidStressMode(""), false);

    // Invalid texture
    TextureLibrary::Process proc;
    QCOMPARE(Commandline_Parser::parseProcess("nonexistent", proc), false);
    QCOMPARE(Commandline_Parser::parseProcess("unknown", proc), false);

    // Invalid lattice
    TextureLibrary::Lattice lat;
    QCOMPARE(Commandline_Parser::parseLattice("hcp", lat), false);
    QCOMPARE(Commandline_Parser::parseLattice("sc", lat), false);
}

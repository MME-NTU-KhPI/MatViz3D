#include "test_commandline_parser.h"
#include <QTest>
#include <QCommandLineParser>
#include <QTemporaryFile>
#include <setjmp.h>
#include "commandline_parser.h"
#include "config_source.h"
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

void TestCommandlineParser::testJsonSourceMapsKeysCorrectly()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QByteArray jsonContent = R"({
        "size": 35,
        "algorithm": "Voronoi",
        "periodic": true,
        "eps": [0.002, 0.0, 0.0, 0.0, 0.0, 0.0]
    })";
    tempFile.write(jsonContent);
    tempFile.flush();

    JsonSource source;
    QString err;
    const auto pairs = source.read(tempFile.fileName(), &err);
    QVERIFY(err.isEmpty());

    QMap<QString, QString> map;
    for (const auto& p : pairs) {
        map[p.first] = p.second;
    }

    QCOMPARE(map.value("size"), QString("35"));
    QCOMPARE(map.value("algorithm"), QString("Voronoi"));
    QCOMPARE(map.value("periodic"), QString("true"));
    QCOMPARE(map.value("eps"), QString("0.002,0,0,0,0,0"));
}

void TestCommandlineParser::testJsonSourceNestedFlattening()
{
    QTemporaryFile tempFile(QDir::tempPath() + "/matviz_test_XXXXXX.json");
    QVERIFY(tempFile.open());
    const QByteArray jsonContent = R"({
        "voronoi": {
            "metric": [1.0, 2.0, 3.0],
            "mxx": 1.5
        },
        "composite": {
            "dim": "2d",
            "packing": "hexagonal"
        }
    })";
    tempFile.write(jsonContent);
    tempFile.close();

    JsonSource source;
    QString err;
    const auto pairs = source.read(tempFile.fileName(), &err);
    QVERIFY(err.isEmpty());

    QMap<QString, QString> map;
    for (const auto& p : pairs) {
        map[p.first] = p.second;
    }

    QCOMPARE(map.value("voronoi_metric"), QString("1,2,3"));
    QCOMPARE(map.value("voronoi_mxx"), QString("1.5"));
    QCOMPARE(map.value("composite_dim"), QString("2d"));
    QCOMPARE(map.value("composite_packing"), QString("hexagonal"));

    // Verify applying these pairs into Parameters
    QString applyErr;
    QVERIFY(ConfigDispatcher::loadAndApply(tempFile.fileName(), &applyErr));
    Parameters* params = Parameters::instance();
    QVERIFY(params->getCompositeDim().contains("2D"));
    QCOMPARE(params->getCompositePacking().toLower(), QString("hexagonal"));
    QCOMPARE(params->getVoronoiMxx(), 1.5);
}

void TestCommandlineParser::testApplyParameterValidAndInvalid()
{
    Parameters* params = Parameters::instance();
    QString err;

    // Valid parameters
    QVERIFY(applyParameter("size", "42", &err));
    QCOMPARE(params->getSize(), 42);

    QVERIFY(applyParameter("composite_dim", "3d", &err));
    QVERIFY(params->getCompositeDim().contains("3D"));

    QVERIFY(applyParameter("periodic", "true", &err));
    QCOMPARE(params->getIsPeriodic(), true);

    QVERIFY(applyParameter("fiber_volume_fraction", "0.35", &err));
    QCOMPARE(params->getFiberVolumeFraction(), 0.35);

    // Invalid parameters must return false, set descriptive error, and NOT crash
    QVERIFY(!applyParameter("size", "-5", &err));
    QVERIFY(!err.isEmpty());

    QVERIFY(!applyParameter("size", "not_an_int", &err));
    QVERIFY(!err.isEmpty());

    QVERIFY(!applyParameter("composite_dim", "4d", &err));
    QVERIFY(err.contains("composite_dim", Qt::CaseInsensitive));

    QVERIFY(!applyParameter("composite_packing", "triangle", &err));
    QVERIFY(err.contains("composite_packing", Qt::CaseInsensitive));

    QVERIFY(!applyParameter("fiber_volume_fraction", "1.5", &err));
    QVERIFY(err.contains("fiber_volume_fraction", Qt::CaseInsensitive));

    QVERIFY(!applyParameter("solver", "abaqus", &err));
    QVERIFY(err.contains("solver", Qt::CaseInsensitive));

    QVERIFY(!applyParameter("stress_mode", "bogus", &err));
    QVERIFY(err.contains("stress_mode", Qt::CaseInsensitive));

    QVERIFY(!applyParameter("lattice", "invalid_lattice", &err));
    QVERIFY(err.contains("lattice", Qt::CaseInsensitive));

    QVERIFY(!applyParameter("texture", "invalid_texture", &err));
    QVERIFY(err.contains("texture", Qt::CaseInsensitive));

    QVERIFY(!applyParameter("scatter", "-5.0", &err));
    QVERIFY(err.contains("scatter", Qt::CaseInsensitive));

    QVERIFY(!applyParameter("eps", "1,2,3", &err));
    QVERIFY(err.contains("eps", Qt::CaseInsensitive));
}

void TestCommandlineParser::testUnknownKeyRejected()
{
    QString err;
    QVERIFY(!applyParameter("totally_unknown_key_xyz", "value", &err));
    QVERIFY(err.contains("Unknown parameter key", Qt::CaseInsensitive));
    QVERIFY(err.contains("totally_unknown_key_xyz"));

    // Also test via ConfigDispatcher::loadAndApply
    QTemporaryFile tempFile(QDir::tempPath() + "/matviz_test_XXXXXX.json");
    QVERIFY(tempFile.open());
    tempFile.write(R"({ "invalid_key_123": "val" })");
    tempFile.close();

    QString loadErr;
    QVERIFY(!ConfigDispatcher::loadAndApply(tempFile.fileName(), &loadErr));
    QVERIFY(loadErr.contains("invalid_key_123"));
}

void TestCommandlineParser::testCliOverridesFile()
{
    Parameters* params = Parameters::instance();

    QTemporaryFile tempFile(QDir::tempPath() + "/matviz_test_XXXXXX.json");
    QVERIFY(tempFile.open());
    const QByteArray jsonContent = R"({
        "size": 30,
        "algorithm": "Voronoi",
        "composite_dim": "1d",
        "seed": 7777
    })";
    tempFile.write(jsonContent);
    tempFile.close();

    // CLI overrides size to 50 and algorithm to Composite; composite_dim and seed are not set on CLI
    QCommandLineParser parser;
    Commandline_Parser::setupParser(parser);
    const QStringList args = {
        "MatViz3D",
        "--config", tempFile.fileName(),
        "--size", "50",
        "--algorithm", "Composite"
    };
    QVERIFY(parser.parse(args));
    QVERIFY(Commandline_Parser::processOptions(parser));

    // CLI values override config file
    QCOMPARE(params->getSize(), 50);
    QCOMPARE(params->getAlgorithm(), QString("Composite"));

    // File values are preserved because CLI did not set them (defaults do NOT overwrite)
    QVERIFY(params->getCompositeDim().contains("1D"));
    QCOMPARE(params->getSeed(), 7777u);
}

void TestCommandlineParser::testYamlSourceMapsFlatKeysCorrectly()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QByteArray content =
        "size: 30\n"
        "algorithm: Voronoi\n"
        "periodic: true\n"
        "points: 50\n";
    tempFile.write(content);
    tempFile.flush();

    YamlSource source;
    QString err;
    const auto pairs = source.read(tempFile.fileName(), &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));

    QMap<QString, QString> map;
    for (const auto& p : pairs) {
        map[p.first] = p.second;
    }
    QCOMPARE(map.value("size"), QString("30"));
    QCOMPARE(map.value("algorithm"), QString("Voronoi"));
    QCOMPARE(map.value("periodic"), QString("true"));
    QCOMPARE(map.value("points"), QString("50"));
}

void TestCommandlineParser::testYamlSourceNestedFlattening()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QByteArray content =
        "voronoi:\n"
        "  mxx: 1.5\n"
        "  voronoi_myy: 2.0\n"
        "composite:\n"
        "  dim: 2d\n"
        "  packing: hexagonal\n";
    tempFile.write(content);
    tempFile.flush();

    YamlSource source;
    QString err;
    const auto pairs = source.read(tempFile.fileName(), &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));

    QMap<QString, QString> map;
    for (const auto& p : pairs) {
        map[p.first] = p.second;
    }
    QCOMPARE(map.value("voronoi_mxx"), QString("1.5"));
    QCOMPARE(map.value("voronoi_myy"), QString("2.0")); // Anti-duplicate prefix rule
    QCOMPARE(map.value("composite_dim"), QString("2d"));
    QCOMPARE(map.value("composite_packing"), QString("hexagonal"));
}

void TestCommandlineParser::testYamlSourceCommentsAndQuotes()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QByteArray content =
        "# Leading comment\n"
        "size: 30 # trailing comment\n"
        "algorithm: 'Voronoi'\n"
        "output: \"results # not comment.hdf5\"\n"
        "neighborhood: 'Moore (26)'\n";
    tempFile.write(content);
    tempFile.flush();

    YamlSource source;
    QString err;
    const auto pairs = source.read(tempFile.fileName(), &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));

    QMap<QString, QString> map;
    for (const auto& p : pairs) {
        map[p.first] = p.second;
    }
    QCOMPARE(map.value("size"), QString("30"));
    QCOMPARE(map.value("algorithm"), QString("Voronoi"));
    QCOMPARE(map.value("output"), QString("results # not comment.hdf5"));
    QCOMPARE(map.value("neighborhood"), QString("Moore (26)"));
}

void TestCommandlineParser::testYamlSourceInlineList()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QByteArray content =
        "metric: [1.0, 2.0, 3.0]\n"
        "eps: [0.002, 0.0, 0.0, 0.0, 0.0, 0.0] # comment after list\n"
        "names: ['item 1', \"item 2\"]\n";
    tempFile.write(content);
    tempFile.flush();

    YamlSource source;
    QString err;
    const auto pairs = source.read(tempFile.fileName(), &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));

    QMap<QString, QString> map;
    for (const auto& p : pairs) {
        map[p.first] = p.second;
    }
    QCOMPARE(map.value("metric"), QString("1.0,2.0,3.0"));
    QCOMPARE(map.value("eps"), QString("0.002,0.0,0.0,0.0,0.0,0.0"));
    QCOMPARE(map.value("names"), QString("item 1,item 2"));
}

void TestCommandlineParser::testYamlSourceNoTypeCoercion()
{
    // Guard against the "Norway problem" (algorithm: on, material: NO)
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QByteArray content =
        "algorithm: on\n"
        "material: NO\n"
        "flag_yes: yes\n"
        "flag_off: off\n";
    tempFile.write(content);
    tempFile.flush();

    YamlSource source;
    QString err;
    const auto pairs = source.read(tempFile.fileName(), &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));

    QMap<QString, QString> map;
    for (const auto& p : pairs) {
        map[p.first] = p.second;
    }
    QCOMPARE(map.value("algorithm"), QString("on"));
    QCOMPARE(map.value("material"), QString("NO"));
    QCOMPARE(map.value("flag_yes"), QString("yes"));
    QCOMPARE(map.value("flag_off"), QString("off"));
}

void TestCommandlineParser::testYamlSourceAllowedCharacters()
{
    // Ensure characters like '-', '&', '|' pass when they are not YAML syntax constructs
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QByteArray content =
        "output: results-2024.json\n"
        "material: Steel & Iron\n"
        "fiber_material: \"a|b\"\n";
    tempFile.write(content);
    tempFile.flush();

    YamlSource source;
    QString err;
    const auto pairs = source.read(tempFile.fileName(), &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));

    QMap<QString, QString> map;
    for (const auto& p : pairs) {
        map[p.first] = p.second;
    }
    QCOMPARE(map.value("output"), QString("results-2024.json"));
    QCOMPARE(map.value("material"), QString("Steel & Iron"));
    QCOMPARE(map.value("fiber_material"), QString("a|b"));
}

void TestCommandlineParser::testYamlSourceUnsupportedConstructsRejected()
{
    auto testReject = [](const QByteArray& yaml, const QString& expectedSubstring, int expectedLine) {
        QTemporaryFile file;
        QVERIFY(file.open());
        file.write(yaml);
        file.flush();

        YamlSource source;
        QString err;
        const auto pairs = source.read(file.fileName(), &err);
        QVERIFY(pairs.isEmpty());
        QVERIFY2(!err.isEmpty(), "Expected parse error, got empty error string");
        QVERIFY2(err.contains(expectedSubstring, Qt::CaseInsensitive),
                 qPrintable(QString("Expected substring '%1' not in '%2'").arg(expectedSubstring, err)));
        QVERIFY2(err.contains(QString("line %1").arg(expectedLine), Qt::CaseInsensitive),
                 qPrintable(QString("Expected line %1 not in '%2'").arg(QString::number(expectedLine), err)));
    };

    // 1. Block sequence (- item)
    testReject("- item\n", "block sequence", 1);

    // 2. Tab for indentation
    testReject("\tsize: 30\n", "tabs are not allowed", 1);

    // 3. Anchor (&anchor)
    testReject("key: &anchor 42\n", "anchor/alias", 1);

    // 4. Alias (*alias)
    testReject("key: *alias\n", "anchor/alias", 1);

    // 5. Multi-line scalar (|)
    testReject("key: |\n  multi\n", "multi-line scalar", 1);

    // 6. Multi-line scalar (>)
    testReject("key: >\n  multi\n", "multi-line scalar", 1);

    // 7. Document marker (---)
    testReject("---\nsize: 30\n", "document marker", 1);

    // 8. Nesting deeper than 1 level
    testReject("voronoi:\n  metric:\n    deep: 1\n", "nesting deeper than 1 level", 2);

    // 9. Missing ':' separator
    testReject("just_a_string_without_colon\n", "missing ':'", 1);
}

void TestCommandlineParser::testYamlSourceEndToEnd()
{
    // 1. Valid YAML file applies to Parameters
    {
        QTemporaryFile tempFile(QDir::tempPath() + "/matviz_test_XXXXXX.yaml");
        QVERIFY(tempFile.open());
        const QByteArray content =
            "size: 45\n"
            "periodic: true\n"
            "voronoi:\n"
            "  mxx: 2.5\n"
            "composite:\n"
            "  dim: 2d\n"
            "  packing: hexagonal\n";
        tempFile.write(content);
        tempFile.flush();

        QString applyErr;
        QVERIFY2(ConfigDispatcher::loadAndApply(tempFile.fileName(), &applyErr), qPrintable(applyErr));
        Parameters* params = Parameters::instance();
        QCOMPARE(params->getSize(), 45);
        QCOMPARE(params->getIsPeriodic(), true);
        QCOMPARE(params->getVoronoiMxx(), 2.5);
        QVERIFY(params->getCompositeDim().contains("2D"));
        QCOMPARE(params->getCompositePacking().toLower(), QString("hexagonal"));
    }

    // 2. Invalid parameter value rejected via applyParameter gracefully without crashing
    {
        QTemporaryFile invalidFile(QDir::tempPath() + "/matviz_test_XXXXXX.yaml");
        QVERIFY(invalidFile.open());
        invalidFile.write("size: -10\n");
        invalidFile.flush();

        QString invalidErr;
        QVERIFY(!ConfigDispatcher::loadAndApply(invalidFile.fileName(), &invalidErr));
        QVERIFY(!invalidErr.isEmpty());
        QVERIFY(invalidErr.contains("size", Qt::CaseInsensitive));
    }
}

void TestCommandlineParser::testConfigCallerPathUnknownKey()
{
    QTemporaryFile tempFile(QDir::tempPath() + "/matviz_test_XXXXXX.json");
    QVERIFY(tempFile.open());
    tempFile.write(R"({ "unknown_option_key": "some_value" })");
    tempFile.close();

    // 1. Direct caller path via ConfigDispatcher::loadAndApply
    QString loadErr;
    bool success = ConfigDispatcher::loadAndApply(tempFile.fileName(), &loadErr);
    QVERIFY(!success);
    QVERIFY(!loadErr.isEmpty());
    QVERIFY(loadErr.contains("unknown_option_key"));
    QVERIFY(loadErr.contains("Unknown parameter key", Qt::CaseInsensitive));

    // 2. Caller path via Commandline_Parser::processOptions with --config
    QCommandLineParser parser;
    Commandline_Parser::setupParser(parser);
    const QStringList args = { "MatViz3D", "--config", tempFile.fileName() };
    QVERIFY(parser.parse(args));

    QString processErr;
    bool procSuccess = Commandline_Parser::processOptions(parser, &processErr);
    QVERIFY(!procSuccess);
    QVERIFY(!processErr.isEmpty());
    QVERIFY(processErr.contains("unknown_option_key"));
    QVERIFY(processErr.contains("Unknown parameter key", Qt::CaseInsensitive));
}

void TestCommandlineParser::testConfigCallerPathMissingAndMalformed()
{
    // Missing file
    const QString missingPath = QDir::tempPath() + "/matviz_nonexistent_123456.json";
    QString missingErr;
    QVERIFY(!ConfigDispatcher::loadAndApply(missingPath, &missingErr));
    QVERIFY(!missingErr.isEmpty());

    QCommandLineParser missingParser;
    Commandline_Parser::setupParser(missingParser);
    QVERIFY(missingParser.parse({ "MatViz3D", "--config", missingPath }));
    QString missingProcErr;
    QVERIFY(!Commandline_Parser::processOptions(missingParser, &missingProcErr));
    QVERIFY(!missingProcErr.isEmpty());

    // Malformed JSON
    QTemporaryFile malformedFile(QDir::tempPath() + "/matviz_malformed_XXXXXX.json");
    QVERIFY(malformedFile.open());
    malformedFile.write("{ bad json: 123");
    malformedFile.close();

    QString malformedErr;
    QVERIFY(!ConfigDispatcher::loadAndApply(malformedFile.fileName(), &malformedErr));
    QVERIFY(malformedErr.contains("syntax error", Qt::CaseInsensitive));

    QCommandLineParser malformedParser;
    Commandline_Parser::setupParser(malformedParser);
    QVERIFY(malformedParser.parse({ "MatViz3D", "--config", malformedFile.fileName() }));
    QString malformedProcErr;
    QVERIFY(!Commandline_Parser::processOptions(malformedParser, &malformedProcErr));
    QVERIFY(malformedProcErr.contains("syntax error", Qt::CaseInsensitive));
}

void TestCommandlineParser::testCliCallerPathGracefulErrors()
{
    QCommandLineParser parser;
    Commandline_Parser::setupParser(parser);
    const QStringList args = { "MatViz3D", "--size", "not_a_number" };
    QVERIFY(parser.parse(args));

    QString err;
    QVERIFY(!Commandline_Parser::processOptions(parser, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains("Option --size", Qt::CaseInsensitive));
}

#include <QGuiApplication>
#include <QTest>
#include <iostream>
#include "parameters.h"
#include "test_polycrystall_seeding.h"
#include "test_polycrystall_growth.h"
#include "test_wave_nucleation.h"
#include "test_texturemath.h"
#include "test_hillcriterion.h"
#include "test_svgexporter.h"
#include "test_commandline_parser.h"
#include "test_lattice.h"
#include "test_ffthomog.h"
#include "test_printability.h"
#include "test_datasetbrowser.h"
#include "test_seed_distribution.h"
#include "test_propertysearch.h"
#include "test_celldecoder.h"
#include "test_celldiffusion.h"
#include "test_cellpredictor.h"
#include "test_cellstiffness.h"
#include "test_requestcheck.h"
#include "test_dlca.h"

int main(int argc, char *argv[])
{
    // QGuiApplication is required for GUI math/color primitives (QMatrix4x4, QColor, QRgb)
    // but operates headlessly without opening any windows.
    QGuiApplication app(argc, argv);

    int totalFailures = 0;

    auto runSuite = [&](QObject* testObject, const char* name) {
        std::cout << "\n=======================================================\n";
        std::cout << " Running Suite: " << name << "\n";
        std::cout << "=======================================================\n" << std::flush;
        Parameters::resetDefaults();
        int result = QTest::qExec(testObject, argc, argv);
        Parameters::resetDefaults();
        if (result != 0) {
            totalFailures++;
        }
    };

    TestPolycrystallSeeding testSeeding;
    runSuite(&testSeeding, "TestPolycrystallSeeding");

    TestPolycrystallGrowth testGrowth;
    runSuite(&testGrowth, "TestPolycrystallGrowth");

    TestWaveNucleation testWave;
    runSuite(&testWave, "TestWaveNucleation");

    TestTextureMath testTextureMath;
    runSuite(&testTextureMath, "TestTextureMath");

    TestHillCriterion testHill;
    runSuite(&testHill, "TestHillCriterion");

    TestSvgExporter testSvg;
    runSuite(&testSvg, "TestSvgExporter");

    TestCommandlineParser testCli;
    runSuite(&testCli, "TestCommandlineParser");

    TestLattice testLattice;
    runSuite(&testLattice, "TestLattice");

    TestFftHomog testFft;
    runSuite(&testFft, "TestFftHomog");

    TestPrintability testPrint;
    runSuite(&testPrint, "TestPrintability");

    TestDatasetBrowser testBrowser;
    runSuite(&testBrowser, "TestDatasetBrowser");

    TestSeedDistribution testSeeds;
    runSuite(&testSeeds, "TestSeedDistribution");

    TestPropertySearch testSelector;
    runSuite(&testSelector, "TestPropertySearch");

    TestCellDecoder testDecoder;
    runSuite(&testDecoder, "TestCellDecoder");

    TestCellDiffusion testDiffusion;
    runSuite(&testDiffusion, "TestCellDiffusion");

    TestCellPredictor testPredictor;
    runSuite(&testPredictor, "TestCellPredictor");

    TestCellStiffness testStiffness;
    runSuite(&testStiffness, "TestCellStiffness");

    TestRequestCheck testRequestCheck;
    runSuite(&testRequestCheck, "TestRequestCheck");

    TestDlca testDlca;
    runSuite(&testDlca, "TestDlca");

    std::cout << "\n=======================================================\n";
    if (totalFailures == 0) {
        std::cout << " ALL MATVIZ3D TEST SUITES PASSED!\n";
    } else {
        std::cout << " " << totalFailures << " TEST SUITE(S) FAILED!\n";
    }
    std::cout << "=======================================================\n" << std::flush;

    return totalFailures;
}

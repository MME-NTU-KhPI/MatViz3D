#include "test_wave_nucleation.h"
#include <QTest>
#include <QDebug>
#include "parameters.h"
#include "parent_algorithm.h"
#include "polycrystall.h"
#include "probability_algorithm.h"

namespace {

class TestablePolycrystallWave : public Polycrystall
{
public:
    using Polycrystall::numCubes;
    using Polycrystall::numColors;
    using Polycrystall::voxels;
    using Polycrystall::grains;
    using Polycrystall::seedPoints;
    using Polycrystall::filled_voxels;
    using Polycrystall::flags;
    using Polycrystall::remainingPoints;

    TestablePolycrystallWave(short int nCubes, int nColors, bool thinLayer = false)
        : Polycrystall(nCubes, nColors, Polycrystall::Neighborhood::Moore, thinLayer)
    {
        Allocate_Memory();
    }

    ~TestablePolycrystallWave() override
    {
        if (voxels) {
            Delete3D(voxels);
            voxels = nullptr;
        }
    }
};

} // namespace

void TestWaveNucleation::init()
{
    Parameters::resetDefaults();
    Parameters::seed = 54321;
}

void TestWaveNucleation::cleanup()
{
    Parameters::resetDefaults();
}

void TestWaveNucleation::testWaveOffAllSeedsAtOnce()
{
    const short int size = 4;
    const int colors = 10;
    Parameters::wave_coefficient = 0.0f;
    Parameters::is_wave_generation = false;

    TestablePolycrystallWave algo(size, colors);
    algo.Initialization(false);

    // When wave nucleation is OFF, all seeds must be placed at step 0
    QCOMPARE(static_cast<int>(algo.seedPoints.size()), colors);
    QCOMPARE(algo.getRemainingPoints(), 0);
}

void TestWaveNucleation::testWaveBatchFormulaPolycrystall()
{
    // Check whether ApplyWaveNucleation exists in the merged Polycrystall class.
    // In current Polycrystall (polycrystall.h / polycrystall.cpp), CA growth uses OpenMP
    // atomic CAS front propagation and does not define ApplyWaveNucleation.
    // Per instructions: "First confirm ApplyWaveNucleation exists in the merged Polycrystalline; if not, flag it and skip."
    qWarning() << "[TestWaveNucleation] Checking ApplyWaveNucleation in Polycrystall: METHOD NOT PRESENT in merged Polycrystall.";
    QSKIP("ApplyWaveNucleation is not present in merged Polycrystall; skipping per instructions.");
}

void TestWaveNucleation::testSurfaceModeSkipsWave()
{
    const short int size = 6;
    const int colors = 12;

    // Enable wave generation in Parameters
    Parameters::is_wave_generation = true;
    Parameters::initial_nuclei_count = 3;
    Parameters::wave_coefficient = 0.5f;

    // Create algorithm in surface (thin layer) mode
    TestablePolycrystallWave algo(size, colors, /*isThinLayer=*/true);
    algo.Initialization(true);

    // In surface mode, Parent_Algorithm::Initialization explicitly clears remainingPoints to 0:
    // "numColors = static_cast<int>(seedPoints.size()); remainingPoints = 0;"
    // Thus surface mode skips wave nucleation entirely.
    QCOMPARE(algo.getRemainingPoints(), 0);
    QVERIFY(algo.seedPoints.size() > 0);
}

void TestWaveNucleation::testProbabilityWaveBatchesProgressive()
{
    const short int size = 4;
    const int totalColors = 8;
    const int initialNuclei = 2;

    Parameters::is_wave_generation = true;
    Parameters::initial_nuclei_count = initialNuclei;
    Parameters::wave_peak_fraction = 0.20;
    Parameters::wave_end_fraction = 0.60;

    Probability_Algorithm algo(size, totalColors);
    algo.Allocate_Memory();
    algo.Initialization(true);

    // Initially, only initial_nuclei_count seeds should be placed
    QCOMPARE(algo.seedPoints.size(), static_cast<size_t>(initialNuclei));

    // Run until completion
    int maxIters = 200;
    while (!algo.getDone() && maxIters-- > 0) {
        algo.Next_Iteration();
    }

    // At completion, cumulative seeds placed across all batches must equal totalColors
    QCOMPARE(algo.seedPoints.size(), static_cast<size_t>(totalColors));

    int32_t*** v = algo.getVoxels();
    algo.CleanUp();
    algo.Delete3D(v);
}

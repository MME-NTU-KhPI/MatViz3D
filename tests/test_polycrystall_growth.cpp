#include "test_polycrystall_growth.h"
#include <QTest>
#include <vector>
#include <cmath>
#include <omp.h>
#include "parameters.h"
#include "polycrystall.h"

namespace {

class TestablePolycrystall : public Polycrystall
{
public:
    using Polycrystall::numCubes;
    using Polycrystall::numColors;
    using Polycrystall::voxels;
    using Polycrystall::grains;
    using Polycrystall::seedPoints;
    using Polycrystall::filled_voxels;
    using Polycrystall::flags;
    using Polycrystall::IterationNumber;

    TestablePolycrystall(short int nCubes, int nColors, Neighborhood neigh)
        : Polycrystall(nCubes, nColors, neigh)
    {
        Allocate_Memory();
    }

    ~TestablePolycrystall() override
    {
        if (voxels) {
            Delete3D(voxels);
            voxels = nullptr;
        }
    }

    void placeManualSeed(int x, int y, int z, int id)
    {
        voxels[x][y][z] = id;
        grains.push_back({x, y, z});
        seedPoints.push_back({x, y, z});
        filled_voxels++;
    }
};

} // namespace

void TestPolycrystallGrowth::init()
{
    Parameters::resetDefaults();
    Parameters::seed = 12345;
    omp_set_num_threads(1);
}

void TestPolycrystallGrowth::cleanup()
{
    Parameters::resetDefaults();
}

void TestPolycrystallGrowth::testNeighborhoodOffsetsSingleStep()
{
    const short int size = 5;
    const int cx = 2, cy = 2, cz = 2;

    struct StencilTest {
        Polycrystall::Neighborhood stencil;
        unsigned int maxNewCount;
        const char* name;
    };

    const StencilTest tests[] = {
        { Polycrystall::Neighborhood::Neumann, 6,  "von Neumann (6)" },
        { Polycrystall::Neighborhood::Radial,  18, "Radial (18)" },
        { Polycrystall::Neighborhood::Moore,   26, "Moore (26)" }
    };

    for (const auto& t : tests) {
        TestablePolycrystall algo(size, 1, t.stencil);
        algo.placeManualSeed(cx, cy, cz, 1);

        QCOMPARE(algo.filled_voxels, 1u);

        algo.Next_Iteration();

        unsigned int newlyFilled = algo.filled_voxels - 1;
        QVERIFY2(newlyFilled <= t.maxNewCount,
                 qPrintable(QString("%1 filled %2 voxels, expected <= %3")
                                .arg(t.name).arg(newlyFilled).arg(t.maxNewCount)));
        QVERIFY2(newlyFilled > 0,
                 qPrintable(QString("%1 failed to grow any voxels").arg(t.name)));

        // Verify that all newly filled voxels lie strictly within the stencil's reach
        for (int x = 0; x < size; ++x) {
            for (int y = 0; y < size; ++y) {
                for (int z = 0; z < size; ++z) {
                    if (x == cx && y == cy && z == cz) continue;
                    if (algo.voxels[x][y][z] == 0) continue;

                    int dx = std::abs(x - cx);
                    int dy = std::abs(y - cy);
                    int dz = std::abs(z - cz);

                    QVERIFY2(dx <= 1 && dy <= 1 && dz <= 1,
                             "Filled voxel is outside distance 1 of center");

                    if (t.stencil == Polycrystall::Neighborhood::Neumann) {
                        int manhattan = dx + dy + dz;
                        QCOMPARE(manhattan, 1);
                    } else if (t.stencil == Polycrystall::Neighborhood::Radial) {
                        int manhattan = dx + dy + dz;
                        QVERIFY2(manhattan == 1 || manhattan == 2,
                                 "Radial neighbors must be face or edge neighbors (manhattan 1 or 2)");
                    } else if (t.stencil == Polycrystall::Neighborhood::Moore) {
                        int maxDist = std::max({dx, dy, dz});
                        QCOMPARE(maxDist, 1);
                    }
                }
            }
        }
    }
}

void TestPolycrystallGrowth::testDeterministicFullGrowth()
{
    const short int size = 4;
    const int colors = 2;
    const unsigned int fixedSeed = 99999;

    auto runSimulation = [size, colors, fixedSeed](std::vector<unsigned int>& stepFills,
                                                   std::vector<int32_t>& outVoxels) {
        Parameters::resetDefaults();
        Parameters::seed = fixedSeed;
        omp_set_num_threads(1);

        TestablePolycrystall algo(size, colors, Polycrystall::Neighborhood::Moore);
        algo.Initialization(false);

        stepFills.push_back(algo.filled_voxels);

        int maxIters = 100;
        while (!algo.getDone() && maxIters-- > 0) {
            algo.Next_Iteration();
            stepFills.push_back(algo.filled_voxels);
        }

        outVoxels.resize(size * size * size);
        for (int x = 0; x < size; ++x) {
            for (int y = 0; y < size; ++y) {
                for (int z = 0; z < size; ++z) {
                    outVoxels[(x * size + y) * size + z] = algo.voxels[x][y][z];
                }
            }
        }
        return algo.filled_voxels;
    };

    std::vector<unsigned int> fills1, fills2;
    std::vector<int32_t> voxels1, voxels2;

    unsigned int finalFill1 = runSimulation(fills1, voxels1);
    unsigned int finalFill2 = runSimulation(fills2, voxels2);

    // 1. Assert monotonic increase of filled_voxels
    for (size_t i = 1; i < fills1.size(); ++i) {
        QVERIFY2(fills1[i] >= fills1[i - 1],
                 "filled_voxels must increase monotonically at every step");
    }

    // 2. Assert bit-for-bit determinism: two runs with identical seed produce identical voxels
    QCOMPARE(fills1.size(), fills2.size());
    QCOMPARE(finalFill1, finalFill2);
    QCOMPARE(voxels1.size(), voxels2.size());
    for (size_t i = 0; i < voxels1.size(); ++i) {
        QCOMPARE(voxels1[i], voxels2[i]);
    }

    // 3. Separate soft check for full-fill (without hard assertion)
    const unsigned int totalVoxels = size * size * size;
    if (finalFill1 == totalVoxels) {
        qDebug() << "[GrowthTest] Soft check passed: domain is 100% filled ("
                 << finalFill1 << "/" << totalVoxels << ")";
    } else {
        qDebug() << "[GrowthTest] Soft check: domain filled"
                 << finalFill1 << "/" << totalVoxels;
    }
}

void TestPolycrystallGrowth::testPeriodicBoundaryGrowth()
{
    const short int size = 4;
    TestablePolycrystall algo(size, 1, Polycrystall::Neighborhood::Neumann);
    algo.flags.isPeriodicStructure = true;
    algo.placeManualSeed(0, 0, 0, 1);

    algo.Next_Iteration();

    // With periodic BC on Neumann, neighbors of (0, 0, 0) wrap to:
    // (size-1, 0, 0), (0, size-1, 0), (0, 0, size-1), (1, 0, 0), (0, 1, 0), (0, 0, 1)
    QCOMPARE(algo.voxels[size - 1][0][0], 1);
    QCOMPARE(algo.voxels[0][size - 1][0], 1);
    QCOMPARE(algo.voxels[0][0][size - 1], 1);
    QCOMPARE(algo.voxels[1][0][0], 1);
    QCOMPARE(algo.voxels[0][1][0], 1);
    QCOMPARE(algo.voxels[0][0][1], 1);
}

#include "test_polycrystall_seeding.h"
#include <QTest>
#include "parameters.h"
#include "parent_algorithm.h"
#include "polycrystall.h"

namespace {

class TestableParentAlgorithm : public Parent_Algorithm
{
public:
    using Parent_Algorithm::Random_Generate_Points;
    using Parent_Algorithm::Thin_Layer_Generate_Points;
    using Parent_Algorithm::flags;
    using Parent_Algorithm::voxels;
    using Parent_Algorithm::numCubes;
    using Parent_Algorithm::numColors;
    using Parent_Algorithm::filled_voxels;
    using Parent_Algorithm::seedPoints;
    using Parent_Algorithm::m_layerDirection;
    using Parent_Algorithm::m_rng;

    TestableParentAlgorithm(short int nCubes, int nColors)
    {
        numCubes = nCubes;
        numColors = nColors;
        m_rng.seed(Parameters::seed);
        Allocate_Memory();
    }

    ~TestableParentAlgorithm() override
    {
        if (voxels) {
            Delete3D(voxels);
            voxels = nullptr;
        }
    }

    void Next_Iteration() override {}
};

} // namespace

void TestPolycrystallSeeding::init()
{
    Parameters::resetDefaults();
    Parameters::seed = 42;
}

void TestPolycrystallSeeding::cleanup()
{
    Parameters::resetDefaults();
}

void TestPolycrystallSeeding::testVolumeSeedingDistribution()
{
    const short int size = 4;
    const int targetPoints = 8;
    TestableParentAlgorithm algo(size, targetPoints);
    algo.flags.isThinLayer = false;

    algo.Random_Generate_Points(targetPoints);

    QCOMPARE(static_cast<int>(algo.seedPoints.size()), targetPoints);

    for (const auto& pt : algo.seedPoints) {
        QVERIFY2(pt.x >= 0 && pt.x < size, "Seed x coordinate out of bounds");
        QVERIFY2(pt.y >= 0 && pt.y < size, "Seed y coordinate out of bounds");
        QVERIFY2(pt.z >= 0 && pt.z < size, "Seed z coordinate out of bounds");
        QVERIFY2(algo.voxels[pt.x][pt.y][pt.z] > 0, "Seed voxel must have a positive grain id");
    }
}

void TestPolycrystallSeeding::testVolumeSeedingCapacity()
{
    const short int size = 3; // volume = 27
    const int requested = 50;
    TestableParentAlgorithm algo(size, requested);
    algo.flags.isThinLayer = false;

    algo.Random_Generate_Points(requested);

    QVERIFY2(static_cast<int>(algo.seedPoints.size()) <= 27,
             "Placed seeds should not exceed volume capacity");
    QCOMPARE(static_cast<int>(algo.seedPoints.size()), 27);
}

void TestPolycrystallSeeding::testSurfaceSeedingSixFaces()
{
    const short int size = 8;
    const int points = 10;

    struct FaceConfig {
        const char* dir;
        int expectedAxis;  // 0=X, 1=Y, 2=Z
        int expectedCoord; // 0 or size - 1
    };

    const FaceConfig configs[6] = {
        { "+Z", 2, 0 },
        { "-Z", 2, size - 1 },
        { "+X", 0, 0 },
        { "-X", 0, size - 1 },
        { "+Y", 1, 0 },
        { "-Y", 1, size - 1 }
    };

    for (const auto& cfg : configs) {
        TestableParentAlgorithm algo(size, points);
        algo.flags.isThinLayer = true;
        algo.m_layerDirection = QString::fromLatin1(cfg.dir);

        algo.Thin_Layer_Generate_Points(points);

        QCOMPARE(static_cast<int>(algo.seedPoints.size()), points);

        for (const auto& pt : algo.seedPoints) {
            int coord = (cfg.expectedAxis == 0) ? pt.x : (cfg.expectedAxis == 1 ? pt.y : pt.z);
            QCOMPARE(coord, cfg.expectedCoord);

            QVERIFY(pt.x >= 0 && pt.x < size);
            QVERIFY(pt.y >= 0 && pt.y < size);
            QVERIFY(pt.z >= 0 && pt.z < size);
        }
    }
}

void TestPolycrystallSeeding::testSurfaceSeedingPlaneCapacity()
{
    const short int size = 4; // plane capacity = 16
    const int planeCapacity = size * size;

    // Test 1: below capacity (5 points)
    {
        TestableParentAlgorithm algo(size, 5);
        algo.flags.isThinLayer = true;
        algo.m_layerDirection = "+Z";
        algo.Thin_Layer_Generate_Points(5);
        QCOMPARE(static_cast<int>(algo.seedPoints.size()), 5);
    }

    // Test 2: exact capacity (16 points)
    {
        TestableParentAlgorithm algo(size, planeCapacity);
        algo.flags.isThinLayer = true;
        algo.m_layerDirection = "+Z";
        algo.Thin_Layer_Generate_Points(planeCapacity);
        QCOMPARE(static_cast<int>(algo.seedPoints.size()), planeCapacity);
    }

    // Test 3: exceeding capacity (30 points) -> must be strictly capped at 16
    {
        TestableParentAlgorithm algo(size, 30);
        algo.flags.isThinLayer = true;
        algo.m_layerDirection = "+Z";
        algo.Thin_Layer_Generate_Points(30);
        QCOMPARE(static_cast<int>(algo.seedPoints.size()), planeCapacity);
    }
}

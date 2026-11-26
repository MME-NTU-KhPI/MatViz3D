#include "test_dlca.h"
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <hdf5.h>
#include <memory>
#include <vector>
#include "algorithmfactory.h"
#include "dlca.h"
#include "hdf5wrapper.h"
#include "parameters.h"
#include "periodic_wrap.hpp"

namespace {

using Coord = Parent_Algorithm::Coordinate;
using Voxels = std::vector<Coord>;

const int kN = 8;

// Two single-voxel aggregates; collision checks do not touch the voxel grid.
DLCA twoParticles(const Coord& a, const Coord& b)
{
    DLCA dlca(static_cast<short int>(kN), 2);
    DLCA_Aggregate first(nullptr, kN);
    first.id = 1;
    first.aggr.push_back(a);
    DLCA_Aggregate second(nullptr, kN);
    second.id = 2;
    second.aggr.push_back(b);
    dlca.aggregates.push_back(first);
    dlca.aggregates.push_back(second);
    return dlca;
}

Voxels fiberAlongX(int y, int z)
{
    Voxels v;
    for (int x = 0; x < kN; ++x)
        v.push_back({ x, y, z });
    return v;
}

Voxels fiberAlongY(int x, int z)
{
    Voxels v;
    for (int y = 0; y < kN; ++y)
        v.push_back({ x, y, z });
    return v;
}

Voxels fiberAlongZ(int x, int y)
{
    Voxels v;
    for (int z = 0; z < kN; ++z)
        v.push_back({ x, y, z });
    return v;
}

Voxels joined(std::initializer_list<Voxels> parts)
{
    Voxels v;
    for (const Voxels& part : parts)
        v.insert(v.end(), part.begin(), part.end());
    return v;
}

// A fiber along x stepping up in y mid-cell, so its ends meet across the
// boundary only through an edge: (N-1, 1, 0) and (0, 0, 0).
Voxels fiberClosedThroughEdge()
{
    Voxels v;
    for (int x = 0; x <= kN - 2; ++x)
        v.push_back({ x, 0, 0 });
    v.push_back({ kN - 2, 1, 0 });
    v.push_back({ kN - 1, 1, 0 });
    return v;
}

std::shared_ptr<Parent_Algorithm> initializeDlca(int size, int points, unsigned int seed)
{
    Parameters& params = *Parameters::instance();
    params.setSize(size);
    params.setPoints(points);
    params.setAlgorithm("DLCA");
    Parameters::seed = seed;
    auto algo = AlgorithmFactory::instance().createAlgorithm("DLCA", params);
    if (!algo) return nullptr;
    algo->Allocate_Memory();
    algo->Initialization(false);
    return algo;
}

void release(const std::shared_ptr<Parent_Algorithm>& algo)
{
    int32_t*** voxels = algo->getVoxels();
    algo->CleanUp();
    Parent_Algorithm::Delete3D(voxels);
}

// Saves geometry metadata to a fresh file; the result is the attribute value,
// or -1 when the attribute is absent.
int savedRankAttribute(const QString& path)
{
    {
        HDF5Wrapper hdf5(path.toStdString());
        saveGeometryMetadataToHDF5(hdf5, "/1");
    }
    hid_t file = H5Fopen(path.toUtf8().constData(), H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file < 0) return -2;

    int value = -1;
    if (H5Aexists_by_name(file, "/1/geometry", "percolation_rank6", H5P_DEFAULT) > 0) {
        hid_t attr = H5Aopen_by_name(file, "/1/geometry", "percolation_rank6", H5P_DEFAULT, H5P_DEFAULT);
        H5Aread(attr, H5T_NATIVE_INT, &value);
        H5Aclose(attr);
    }
    H5Fclose(file);
    return value;
}

} // namespace

void TestDlca::testCollision_data()
{
    QTest::addColumn<int>("ax");
    QTest::addColumn<int>("ay");
    QTest::addColumn<int>("az");
    QTest::addColumn<int>("bx");
    QTest::addColumn<int>("by");
    QTest::addColumn<int>("bz");
    QTest::addColumn<bool>("collides");

    const int last = kN - 1;
    QTest::newRow("face inside the cell") << 3 << 3 << 3 << 4 << 3 << 3 << true;
    QTest::newRow("face across x boundary") << 0 << 3 << 3 << last << 3 << 3 << true;
    QTest::newRow("face across y boundary") << 3 << 0 << 3 << 3 << last << 3 << true;
    QTest::newRow("face across z boundary") << 3 << 3 << last << 3 << 3 << 0 << true;
    QTest::newRow("edge inside the cell") << 3 << 3 << 3 << 4 << 4 << 3 << false;
    QTest::newRow("edge across boundary") << 0 << 3 << 3 << last << 4 << 3 << false;
    QTest::newRow("edge across two boundaries") << 0 << 0 << 3 << last << last << 3 << false;
    QTest::newRow("corner across boundaries") << 0 << 0 << 0 << last << last << last << false;
    QTest::newRow("gap of one across boundary") << 0 << 3 << 3 << last - 1 << 3 << 3 << false;
}

void TestDlca::testCollision()
{
    QFETCH(int, ax); QFETCH(int, ay); QFETCH(int, az);
    QFETCH(int, bx); QFETCH(int, by); QFETCH(int, bz);
    QFETCH(bool, collides);

    DLCA dlca = twoParticles({ ax, ay, az }, { bx, by, bz });
    QCOMPARE(dlca.check_collision(0, 1), collides);
    QCOMPARE(dlca.check_collision(1, 0), collides);
}

void TestDlca::testFaceContactAcrossBoundaryJoins()
{
    DLCA dlca = twoParticles({ 0, 3, 3 }, { kN - 1, 3, 3 });
    QVERIFY(dlca.check_collision(0, 1));
    dlca.join_aggregates(0, 1);
    QCOMPARE(dlca.aggregates[0].aggr.size() + dlca.aggregates[1].aggr.size(), size_t(2));
    QVERIFY(dlca.aggregates[0].aggr.empty() || dlca.aggregates[1].aggr.empty());
}

void TestDlca::testWrapRank_data()
{
    QTest::addColumn<Voxels>("voxels");
    QTest::addColumn<int>("rank");

    Voxels blob;
    for (int x = 2; x <= 4; ++x)
        for (int y = 2; y <= 4; ++y)
            for (int z = 2; z <= 4; ++z)
                blob.push_back({ x, y, z });

    // Steps +x then +y; both axes wrap, but along a single direction (1,1,0)
    Voxels staircase;
    for (int i = 0; i < kN; ++i) {
        staircase.push_back({ i, i, 0 });
        staircase.push_back({ (i + 1) % kN, i, 0 });
    }

    Voxels edgeThenFace = fiberClosedThroughEdge();
    edgeThenFace.push_back({ kN - 1, 0, 0 });

    Voxels dense;
    for (int x = 0; x < kN; ++x)
        for (int y = 0; y < kN; ++y)
            for (int z = 0; z < kN; ++z)
                dense.push_back({ x, y, z });

    QTest::newRow("empty") << Voxels{} << 0;
    QTest::newRow("blob") << blob << 0;
    QTest::newRow("fiber along x") << fiberAlongX(3, 3) << 1;
    QTest::newRow("fiber along y") << fiberAlongY(3, 3) << 1;
    QTest::newRow("fiber along z") << fiberAlongZ(3, 3) << 1;
    QTest::newRow("diagonal staircase") << staircase << 1;
    QTest::newRow("two crossing fibers") << joined({ fiberAlongX(3, 3), fiberAlongY(3, 3) }) << 2;
    QTest::newRow("three crossing fibers")
        << joined({ fiberAlongX(3, 3), fiberAlongY(3, 3), fiberAlongZ(3, 3) }) << 3;
    QTest::newRow("fiber closed only through an edge") << fiberClosedThroughEdge() << 0;
    QTest::newRow("same fiber closed through a face") << edgeThenFace << 1;
    QTest::newRow("separate fibers do not combine")
        << joined({ fiberAlongX(1, 1), fiberAlongY(5, 5) }) << 1;
    QTest::newRow("dense grid") << dense << 3;
}

void TestDlca::testWrapRank()
{
    QFETCH(Voxels, voxels);
    QFETCH(int, rank);
    QCOMPARE(periodic::wrapRank6(voxels, kN), rank);
}

void TestDlca::testGeneratedClusterReportsRank()
{
    auto algo = initializeDlca(12, 40, 7);
    QVERIFY(algo);
    QCOMPARE(Parameters::percolation_rank6, -1);

    algo->Generate_To_End();

    auto* dlca = dynamic_cast<DLCA*>(algo.get());
    QVERIFY(dlca);
    QCOMPARE(dlca->aggregates.size(), size_t(1));
    QCOMPARE(Parameters::percolation_rank6, periodic::wrapRank6(dlca->aggregates[0].aggr, 12));
    release(algo);
}

void TestDlca::testSingleParticleWarns()
{
    QTest::ignoreMessage(QtDebugMsg, QRegularExpression("periodic wrap rank \\(6-conn\\) = 0$"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("cluster does not percolate in 3D"));

    auto algo = initializeDlca(8, 1, 1);
    QVERIFY(algo);
    QCOMPARE(Parameters::percolation_rank6, 0);
    release(algo);
}

void TestDlca::testDenseClusterDoesNotWarn()
{
    QTest::failOnWarning(QRegularExpression("cluster does not percolate"));

    auto algo = initializeDlca(4, 64, 1);
    QVERIFY(algo);
    algo->Generate_To_End();
    QCOMPARE(Parameters::percolation_rank6, 3);
    release(algo);
}

void TestDlca::testParticlesKeepDistinctVoxels_data()
{
    QTest::addColumn<int>("concentration");
    QTest::addColumn<unsigned int>("seed");

    for (int concentration : { 5, 20 })
        for (unsigned int seed : { 1u, 2u, 3u })
            QTest::addRow("c%d seed %u", concentration, seed) << concentration << seed;
}

void TestDlca::testParticlesKeepDistinctVoxels()
{
    QFETCH(int, concentration);
    QFETCH(unsigned int, seed);

    const int size = 32;
    const int particles = size * size * size * concentration / 100;

    auto algo = initializeDlca(size, particles, seed);
    QVERIFY(algo);
    algo->Generate_To_End();

    auto* dlca = dynamic_cast<DLCA*>(algo.get());
    QVERIFY(dlca);
    QCOMPARE(dlca->aggregates.size(), size_t(1));
    QCOMPARE(dlca->aggregates[0].aggr.size(), size_t(particles));

    int32_t*** voxels = algo->getVoxels();
    int solid = 0;
    for (int x = 0; x < size; ++x)
        for (int y = 0; y < size; ++y)
            for (int z = 0; z < size; ++z)
                if (voxels[x][y][z] != 0)
                    ++solid;
    QCOMPARE(solid, particles);
    release(algo);
}

void TestDlca::testNoTouchingAggregatesAfterStep_data()
{
    QTest::addColumn<int>("concentration");
    QTest::addColumn<unsigned int>("seed");

    for (int concentration : { 10, 30 })
        for (unsigned int seed : { 1u, 2u, 3u })
            QTest::addRow("c%d seed %u", concentration, seed) << concentration << seed;
}

// After every step no two aggregates may share a face: the grid-based merge must
// find every contact that the pairwise definition check_collision reports.
void TestDlca::testNoTouchingAggregatesAfterStep()
{
    QFETCH(int, concentration);
    QFETCH(unsigned int, seed);

    const int size = 16;
    const int particles = size * size * size * concentration / 100;

    auto algo = initializeDlca(size, particles, seed);
    QVERIFY(algo);
    auto* dlca = dynamic_cast<DLCA*>(algo.get());
    QVERIFY(dlca);

    for (int step = 0; step < 5 && !dlca->getDone(); ++step) {
        dlca->Next_Iteration();

        size_t total = 0;
        int32_t*** voxels = algo->getVoxels();
        for (const DLCA_Aggregate& a : dlca->aggregates) {
            total += a.aggr.size();
            for (const Coord& c : a.aggr)
                QCOMPARE(voxels[c.x][c.y][c.z], a.id);       // grid labels match the owner
        }
        QCOMPARE(total, size_t(particles));                  // no voxel lost or duplicated

        for (size_t i = 0; i < dlca->aggregates.size(); ++i)
            for (size_t j = i + 1; j < dlca->aggregates.size(); ++j)
                QVERIFY2(!dlca->check_collision(i, j), "touching aggregates were not merged");
    }
    release(algo);
}

void TestDlca::testRankAttributeInGeometry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Parameters::instance()->setAlgorithm("DLCA");
    Parameters::percolation_rank6 = 2;
    QCOMPARE(savedRankAttribute(dir.filePath("rank.hdf5")), 2);
}

void TestDlca::testNoRankAttributeWhenNotComputed()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Parameters::instance()->setAlgorithm("DLCA");
    Parameters::percolation_rank6 = -1;
    QCOMPARE(savedRankAttribute(dir.filePath("rank.hdf5")), -1);
}




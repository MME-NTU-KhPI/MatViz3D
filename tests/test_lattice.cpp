#include "test_lattice.h"
#include <QTest>
#include <cmath>
#include <vector>
#include <omp.h>
#include "parameters.h"
#include "lattice.h"
#include "latticefamily.h"
#include "commandline_parser.h"

namespace {

class TestableLattice : public Lattice
{
public:
    using Lattice::voxels;
    using Lattice::numCubes;

    explicit TestableLattice(short int n) : Lattice(n, 1) { Allocate_Memory(); }

    ~TestableLattice() override
    {
        if (voxels) {
            Delete3D(voxels);
            voxels = nullptr;
        }
    }

    long long solidCount(int* maxId = nullptr) const
    {
        long long solid = 0;
        int mx = 0;
        for (int x = 0; x < numCubes; ++x)
            for (int y = 0; y < numCubes; ++y)
                for (int z = 0; z < numCubes; ++z) {
                    const int id = voxels[x][y][z];
                    if (id != 0) ++solid;
                    if (id > mx) mx = id;
                }
        if (maxId) *maxId = mx;
        return solid;
    }

    std::vector<int32_t> flat() const
    {
        std::vector<int32_t> v;
        v.reserve(static_cast<size_t>(numCubes) * numCubes * numCubes);
        for (int x = 0; x < numCubes; ++x)
            for (int y = 0; y < numCubes; ++y)
                for (int z = 0; z < numCubes; ++z) v.push_back(voxels[x][y][z]);
        return v;
    }
};

} // namespace

void TestLattice::init()
{
    Parameters::resetDefaults();
    Parameters::seed = 777;
    omp_set_num_threads(1);
}

void TestLattice::cleanup()
{
    Parameters::resetDefaults();
}

void TestLattice::testFamilyParsing()
{
    LatticeFamily f = LatticeFamily::Spinodoid;

    // Every key and every label round-trips.
    for (const LatticeFamilyInfo& i : latticeFamilies()) {
        QVERIFY2(parseLatticeFamily(QString::fromLatin1(i.key), f), i.key);
        QCOMPARE(static_cast<int>(f), static_cast<int>(i.family));
        QVERIFY2(parseLatticeFamily(QString::fromLatin1(i.label), f), i.label);
        QCOMPARE(static_cast<int>(f), static_cast<int>(i.family));
    }

    // Loose spellings and aliases.
    QVERIFY(parseLatticeFamily("Gyroid-Sheet", f));
    QCOMPARE(static_cast<int>(f), static_cast<int>(LatticeFamily::GyroidSheet));
    QVERIFY(parseLatticeFamily("gyroid", f));
    QCOMPARE(static_cast<int>(f), static_cast<int>(LatticeFamily::GyroidSheet));
    QVERIFY(parseLatticeFamily(" BCC ", f));
    QCOMPARE(static_cast<int>(f), static_cast<int>(LatticeFamily::BCC));
    QVERIFY(parseLatticeFamily("re-entrant", f));
    QCOMPARE(static_cast<int>(f), static_cast<int>(LatticeFamily::Reentrant));
    QVERIFY(parseLatticeFamily("auxetic", f));
    QCOMPARE(static_cast<int>(f), static_cast<int>(LatticeFamily::Reentrant));

    QVERIFY(!parseLatticeFamily("kagome", f));
    QVERIFY(!parseLatticeFamily("", f));
}

void TestLattice::testDensityIsHitForEveryFamily()
{
    Parameters* p = Parameters::instance();
    const int N = 24;
    p->setSize(N);
    p->setCellsPerEdge(2);
    p->setRelativeDensity(0.30);

    for (const LatticeFamilyInfo& i : latticeFamilies()) {
        p->setLatticeFamily(QString::fromLatin1(i.key));

        TestableLattice L(N);
        L.Initialization(false);
        L.Generate_To_End();
        QVERIFY(L.getDone());

        int maxId = 0;
        const long long solid = L.solidCount(&maxId);
        const double rho = static_cast<double>(solid) / (double(N) * N * N);

        // The level is read off the sorted field at the requested quantile, so
        // on a cube no larger than the coarse grid the only slack is ties: a
        // symmetric cell repeats the same field value at many voxels, and all
        // of them land on the same side of the level.
        // Distance fields of axis-aligned struts and walls tie in whole
        // shells on a 12-voxel cell, so those families get more slack.
        const bool shells = (i.family == LatticeFamily::SimpleCubic || i.family == LatticeFamily::BCC ||
                             i.family == LatticeFamily::FCC || i.family == LatticeFamily::Octet ||
                             i.family == LatticeFamily::Honeycomb || i.family == LatticeFamily::Reentrant);
        QVERIFY2(std::abs(rho - 0.30) < (shells ? 0.03 : 0.01),
                 qPrintable(QString("%1: achieved %2").arg(i.key).arg(rho)));
        QCOMPARE(maxId, 1);
        QCOMPARE(L.achievedDensity(), rho);
        QCOMPARE(Parameters::lattice_achieved_density, rho);
    }
}

void TestLattice::testFieldIsPeriodicInTheCube()
{
    Parameters* p = Parameters::instance();
    const int N = 32;
    p->setSize(N);
    p->setCellsPerEdge(2);
    p->setRelativeDensity(0.25);

    const char* families[] = { "gyroid_sheet", "schwarz_d_skeletal", "iwp_sheet",
                               "bcc", "octet", "honeycomb", "reentrant", "spinodoid" };
    for (const char* key : families) {
        p->setLatticeFamily(QString::fromLatin1(key));
        TestableLattice L(N);
        L.Initialization(false);

        const double probes[][3] = { { 3.3, 7.1, 11.9 }, { 15.2, 0.4, 20.8 }, { 27.9, 30.5, 2.2 } };
        for (const double* q : probes) {
            const double f0 = L.fieldAt(q[0], q[1], q[2]);
            QVERIFY2(std::abs(L.fieldAt(q[0] + N, q[1], q[2]) - f0) < 1e-9, key);
            QVERIFY2(std::abs(L.fieldAt(q[0], q[1] + N, q[2]) - f0) < 1e-9, key);
            QVERIFY2(std::abs(L.fieldAt(q[0], q[1], q[2] + N) - f0) < 1e-9, key);
        }
    }
}

void TestLattice::testLayerRevealMatchesOneShot()
{
    Parameters* p = Parameters::instance();
    const int N = 20;
    p->setSize(N);
    p->setCellsPerEdge(2);
    p->setRelativeDensity(0.35);
    p->setLatticeFamily("gyroid_sheet");

    TestableLattice oneShot(N);
    oneShot.Initialization(false);
    oneShot.Generate_To_End();

    TestableLattice stepped(N);
    stepped.Initialization(false);
    int steps = 0;
    while (!stepped.getDone()) {
        stepped.Next_Iteration();
        ++steps;
    }
    QVERIFY(steps > 1);
    QVERIFY(oneShot.flat() == stepped.flat());
    QCOMPARE(stepped.achievedDensity(), oneShot.achievedDensity());
}

void TestLattice::testGradingThickensTowardsTop()
{
    Parameters* p = Parameters::instance();
    const int N = 32;
    p->setSize(N);
    p->setCellsPerEdge(2);
    p->setRelativeDensity(0.30);
    p->setLatticeFamily("gyroid_sheet");
    p->setLatticeGrading(0.5);

    TestableLattice L(N);
    L.Initialization(false);
    L.Generate_To_End();

    auto slabFraction = [&](int z0, int z1) {
        long long solid = 0;
        for (int x = 0; x < N; ++x)
            for (int y = 0; y < N; ++y)
                for (int z = z0; z < z1; ++z)
                    if (L.voxels[x][y][z] != 0) ++solid;
        return static_cast<double>(solid) / (double(N) * N * (z1 - z0));
    };
    const double bottom = slabFraction(0, N / 4);
    const double top    = slabFraction(3 * N / 4, N);
    QVERIFY2(top > bottom * 1.3, qPrintable(QString("bottom %1, top %2").arg(bottom).arg(top)));

    // The overall density is still the requested one.
    QVERIFY(std::abs(L.achievedDensity() - 0.30) < 0.01);
}

void TestLattice::testSeedReproducesSpinodoid()
{
    Parameters* p = Parameters::instance();
    const int N = 16;
    p->setSize(N);
    p->setCellsPerEdge(2);
    p->setRelativeDensity(0.40);
    p->setLatticeFamily("spinodoid");

    Parameters::seed = 4242;
    TestableLattice a(N);
    a.Initialization(false);
    a.Generate_To_End();

    Parameters::seed = 4242;
    TestableLattice b(N);
    b.Initialization(false);
    b.Generate_To_End();
    QVERIFY(a.flat() == b.flat());

    Parameters::seed = 4243;
    TestableLattice c(N);
    c.Initialization(false);
    c.Generate_To_End();
    QVERIFY(a.flat() != c.flat());
}

void TestLattice::testCliOptions()
{
    Parameters* p = Parameters::instance();
    QString err;

    QVERIFY(applyParameter("lattice_family", "octet", &err));
    QCOMPARE(p->getLatticeFamily(), QString("Octet truss"));
    QCOMPARE(Parameters::latticeFamilyKeyStored(), QString("octet"));

    QVERIFY(applyParameter("lattice_family", "Schwarz P (skeletal)", &err));
    QCOMPARE(Parameters::latticeFamilyKeyStored(), QString("schwarz_p_skeletal"));

    QVERIFY(applyParameter("cells_per_edge", "3", &err));
    QCOMPARE(p->getCellsPerEdge(), 3);

    QVERIFY(applyParameter("relative_density", "0.15", &err));
    QCOMPARE(p->getRelativeDensity(), 0.15);

    QVERIFY(applyParameter("lattice_grading", "0.25", &err));
    QCOMPARE(p->getLatticeGrading(), 0.25);

    QVERIFY(applyParameter("lattice_jitter", "0.1", &err));
    QCOMPARE(p->getLatticeJitter(), 0.1);

    QVERIFY(!applyParameter("lattice_family", "kagome", &err));
    QVERIFY(!err.isEmpty());
    QCOMPARE(Parameters::latticeFamilyKeyStored(), QString("schwarz_p_skeletal"));

    QVERIFY(!applyParameter("cells_per_edge", "0", &err));
    QVERIFY(!applyParameter("cells_per_edge", "65", &err));
    QVERIFY(!applyParameter("relative_density", "1.5", &err));
    QVERIFY(!applyParameter("lattice_grading", "0.95", &err));
    QVERIFY(!applyParameter("lattice_jitter", "-0.1", &err));

    QVERIFY(Commandline_Parser::isValidLatticeFamily("gyroid_sheet"));
    QVERIFY(!Commandline_Parser::isValidLatticeFamily("gyroid_cube"));
}

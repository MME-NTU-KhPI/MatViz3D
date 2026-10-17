#include "test_lattice.h"
#include <QTest>
#include <QTemporaryFile>
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
                               "bcc", "octet", "honeycomb", "reentrant", "spinodoid",
                               "noise_sheet", "noise_skeletal" };
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

void TestLattice::testSpinodoidConesMakeLamellae()
{
    // Only the cone about Z open: every admissible wave vector points along
    // +-Z (plus a few that are nearly so), so the field is a stack of layers
    // normal to Z -- nearly invariant along X and Y, varying along Z.
    Parameters* p = Parameters::instance();
    const int N = 32;
    p->setSize(N);
    p->setCellsPerEdge(4);
    p->setRelativeDensity(0.40);
    p->setLatticeFamily("spinodoid");
    p->setSpinodoidThetaX(0.0);
    p->setSpinodoidThetaY(0.0);
    p->setSpinodoidThetaZ(15.0);

    TestableLattice L(N);
    L.Initialization(false);
    L.Generate_To_End();

    long long changesX = 0, changesZ = 0;
    const long long pairs = static_cast<long long>(N) * N * (N - 1);
    for (int x = 0; x < N; ++x)
        for (int y = 0; y < N; ++y)
            for (int z = 0; z < N; ++z) {
                if (x + 1 < N && L.voxels[x][y][z] != L.voxels[x + 1][y][z]) ++changesX;
                if (z + 1 < N && L.voxels[x][y][z] != L.voxels[x][y][z + 1]) ++changesZ;
            }
    const double fx = static_cast<double>(changesX) / pairs;
    const double fz = static_cast<double>(changesZ) / pairs;
    QVERIFY2(fx < 0.05, qPrintable(QString("solid/void changes along X: %1").arg(fx)));
    QVERIFY2(fz > 0.05, qPrintable(QString("solid/void changes along Z: %1").arg(fz)));
    QVERIFY(!Parameters::lattice_wave_terms.empty());
    for (const auto& t : Parameters::lattice_wave_terms)
        QVERIFY2(std::abs(t[2]) >= std::abs(t[0]) && std::abs(t[2]) >= std::abs(t[1]),
                 "a wave of a Z-cone spinodoid must lean along Z");
}

void TestLattice::testFourierFileRoundTrip()
{
    Parameters* p = Parameters::instance();
    const int N = 16;
    p->setSize(N);
    p->setCellsPerEdge(1);
    p->setRelativeDensity(0.35);
    p->setLatticeFamily("fourier_sheet");

    QTemporaryFile file;
    QVERIFY(file.open());
    file.write("# kx ky kz amplitude phase\n");
    file.write("1 0 0 1.0 0.0\n");
    file.write("0 2 0 0.5 1.0\n");
    file.write("\n0 0 0 9.0 0.0\n");     // a constant term is ignored
    file.flush();
    p->setFourierFile(file.fileName());

    TestableLattice L(N);
    L.Initialization(false);
    QCOMPARE(static_cast<int>(Parameters::lattice_wave_terms.size()), 2);

    // fieldAt() takes voxel units; the formula lives in cube fractions.
    const double pi = 3.14159265358979323846;
    const double probes[][3] = { { 3.5, 7.5, 11.5 }, { 0.5, 0.5, 0.5 }, { 12.5, 2.5, 9.5 } };
    for (const double* q : probes) {
        const double X = q[0] / N, Y = q[1] / N;
        const double f = 1.0 * std::cos(2 * pi * X) + 0.5 * std::cos(2 * pi * 2 * Y + 1.0);
        QVERIFY2(std::abs(L.fieldAt(q[0], q[1], q[2]) - std::abs(f)) < 1e-9, "sheet field = |sum of the file's waves|");
    }

    p->setLatticeFamily("fourier_skeletal");
    TestableLattice K(N);
    K.Initialization(false);
    const double X = 3.5 / N, Y = 7.5 / N;
    const double f = std::cos(2 * pi * X) + 0.5 * std::cos(2 * pi * 2 * Y + 1.0);
    QVERIFY(std::abs(K.fieldAt(3.5, 7.5, 11.5) - f) < 1e-9);

    // An unreadable file falls back to a random draw rather than an empty cube.
    p->setFourierFile("/nonexistent/terms.txt");
    TestableLattice R(N);
    R.Initialization(false);
    R.Generate_To_End();
    QVERIFY(R.achievedDensity() > 0.3);
    QVERIFY(static_cast<int>(Parameters::lattice_wave_terms.size()) == p->getFourierTerms());
}

void TestLattice::testFourierRandomIsReproducibleAndPublished()
{
    Parameters* p = Parameters::instance();
    const int N = 16;
    p->setSize(N);
    p->setRelativeDensity(0.30);
    p->setLatticeFamily("fourier_sheet");
    p->setFourierTerms(12);
    p->setFourierKmin(1.0);
    p->setFourierKmax(3.0);
    p->setFourierDecay(1.0);

    Parameters::seed = 99;
    TestableLattice a(N);
    a.Initialization(false);
    a.Generate_To_End();
    const auto termsA = Parameters::lattice_wave_terms;
    QCOMPARE(static_cast<int>(termsA.size()), 12);
    // Unit variance: sum of amp^2 / 2 == 1.
    double var = 0.0;
    for (const auto& t : termsA) var += 0.5 * t[3] * t[3];
    QVERIFY(std::abs(var - 1.0) < 1e-6);
    // Wave numbers stay inside the requested band.
    for (const auto& t : termsA) {
        const double n = std::sqrt(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
        QVERIFY(n >= 1.0 - 1e-9 && n <= 3.0 + 1e-9);
    }

    Parameters::seed = 99;
    TestableLattice b(N);
    b.Initialization(false);
    b.Generate_To_End();
    QVERIFY(a.flat() == b.flat());
    QVERIFY(Parameters::lattice_wave_terms == termsA);

    Parameters::seed = 100;
    TestableLattice c(N);
    c.Initialization(false);
    c.Generate_To_End();
    QVERIFY(a.flat() != c.flat());

    // Other families publish no waves.
    p->setLatticeFamily("bcc");
    TestableLattice d(N);
    d.Initialization(false);
    QVERIFY(Parameters::lattice_wave_terms.empty());
}

void TestLattice::testSmoothedNoiseIsReproducibleAndSmoothingConnects()
{
    Parameters* p = Parameters::instance();
    const int N = 32;
    p->setSize(N);
    p->setRelativeDensity(0.40);
    p->setLatticeFamily("noise_skeletal");
    p->setNoiseSmooth(0.08);

    Parameters::seed = 31;
    TestableLattice a(N);
    a.Initialization(false);
    a.Generate_To_End();
    const int componentsSmooth = Parameters::printability.components;
    QVERIFY(std::abs(a.achievedDensity() - 0.40) < 0.01);

    Parameters::seed = 31;
    TestableLattice b(N);
    b.Initialization(false);
    b.Generate_To_End();
    QVERIFY(a.flat() == b.flat());

    Parameters::seed = 32;
    TestableLattice c(N);
    c.Initialization(false);
    c.Generate_To_End();
    QVERIFY(a.flat() != c.flat());

    // Barely smoothed noise at the same density is dust: many more pieces.
    p->setNoiseSmooth(0.01);
    Parameters::seed = 31;
    TestableLattice d(N);
    d.Initialization(false);
    d.Generate_To_End();
    const int componentsRough = Parameters::printability.components;
    QVERIFY2(componentsRough > 5 * std::max(1, componentsSmooth),
             qPrintable(QString("components: rough %1, smooth %2").arg(componentsRough).arg(componentsSmooth)));

    QString err;
    QVERIFY(applyParameter("noise_smooth", "0.1", &err));
    QCOMPARE(p->getNoiseSmooth(), 0.1);
    QVERIFY(!applyParameter("noise_smooth", "0.9", &err));
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
    QVERIFY(Commandline_Parser::isValidLatticeFamily("fourier_skeletal"));
    QVERIFY(!Commandline_Parser::isValidLatticeFamily("gyroid_cube"));

    QVERIFY(applyParameter("spinodoid_theta_z", "30", &err));
    QCOMPARE(p->getSpinodoidThetaZ(), 30.0);
    QVERIFY(applyParameter("spinodoid_theta_x", "0", &err));
    QCOMPARE(p->getSpinodoidThetaX(), 0.0);
    QVERIFY(!applyParameter("spinodoid_theta_y", "3", &err));
    QVERIFY(!applyParameter("spinodoid_theta_y", "95", &err));

    QVERIFY(applyParameter("fourier_terms", "32", &err));
    QCOMPARE(p->getFourierTerms(), 32);
    QVERIFY(!applyParameter("fourier_terms", "1", &err));
    QVERIFY(applyParameter("fourier_kmax", "6", &err));
    QCOMPARE(p->getFourierKmax(), 6.0);
    QVERIFY(!applyParameter("fourier_kmin", "0.5", &err));
    QVERIFY(applyParameter("fourier_decay", "1.5", &err));
    QCOMPARE(p->getFourierDecay(), 1.5);
    QVERIFY(!applyParameter("fourier_decay", "5", &err));
    QVERIFY(applyParameter("fourier_file", "terms.txt", &err));
    QCOMPARE(p->getFourierFile(), QString("terms.txt"));
    QVERIFY(!applyParameter("fourier_file", "  ", &err));
}

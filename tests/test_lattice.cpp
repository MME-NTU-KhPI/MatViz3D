#include "test_lattice.h"
#include <QTest>
#include <QTemporaryFile>
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
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
        // Plates sit on whole-voxel shells (2, 4, 6 ... voxels thick), so on
        // 12 voxels per cell the reachable densities are 0.42 and 0.70 and no
        // level hits 0.30; spheres and cylinders are quantized the same way.
        // testPlateLatticesSymmetryAndUnion / testSitesCarvingAndSmoothing
        // cover them on a finer cell.
        if (isPlateFamily(i.family) || isSiteFamily(i.family)) continue;
        // Cubic assemblies put members on the mirror planes, whole-voxel shells
        // again; testCubicAssemblyDrawIsConnectedAndRoundTrips covers them.
        if (i.family == LatticeFamily::CubicAssembly) continue;
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
                             i.family == LatticeFamily::Honeycomb || i.family == LatticeFamily::Reentrant ||
                             isPlateFamily(i.family) || isSiteFamily(i.family));
        // Shell families on 12 voxels per cell reach the density in steps of
        // up to 0.1 (the fields are snapped, so a shell is all in or all out
        // and the level lands on the nearer shell): one shell of slack.
        QVERIFY2(std::abs(rho - 0.30) < (shells ? 0.07 : 0.01),
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

void TestLattice::testMaterialOverrideAndTpmsWaves()
{
    Parameters* p = Parameters::instance();
    QString err;

    // Isotropic override: the published phase carries lambda + 2 mu, lambda, mu.
    QVERIFY(applyParameter("material_iso", "2.0,0.38", &err));
    QVERIFY(Parameters::materialOverrideActive());
    const int N = 16;
    p->setSize(N);
    p->setLatticeFamily("gyroid_sheet");
    p->setCellsPerEdge(2);
    p->setRelativeDensity(0.3);
    TestableLattice L(N);
    L.Initialization(false);
    QCOMPARE(static_cast<int>(Parameters::phaseAssignment.materials.size()), 1);
    const double E = 2.0e9, nu = 0.38;
    const double lam = E * nu / ((1 + nu) * (1 - 2 * nu)), mu = E / (2 * (1 + nu));
    const auto& C = Parameters::phaseAssignment.materials[0].C;
    QVERIFY(std::abs(C[0][0] - (lam + 2 * mu)) < 1e-3 * E);
    QVERIFY(std::abs(C[0][1] - lam) < 1e-3 * E);
    QVERIFY(std::abs(C[3][3] - mu) < 1e-3 * E);

    // Gyroid published as six waves that reproduce the level set exactly.
    const auto& terms = Parameters::lattice_wave_terms;
    QCOMPARE(static_cast<int>(terms.size()), 6);
    const double pi = 3.14159265358979323846;
    const double probes[][3] = { { 0.13, 0.71, 0.29 }, { 0.5, 0.05, 0.9 }, { 0.77, 0.33, 0.61 } };
    for (const double* q : probes) {
        double f = 0.0;
        for (const auto& t : terms) f += t[3] * std::cos(2 * pi * (t[0] * q[0] + t[1] * q[1] + t[2] * q[2]) + t[4]);
        const double a = 2 * pi * 2;   // two cells per edge
        const double g = std::sin(a*q[0]) * std::cos(a*q[1]) + std::sin(a*q[1]) * std::cos(a*q[2]) + std::sin(a*q[2]) * std::cos(a*q[0]);
        QVERIFY2(std::abs(f - g) < 1e-5, qPrintable(QString("gyroid waves: %1 vs %2").arg(f).arg(g)));   // terms are stored as float
    }
    p->setLatticeFamily("iwp_skeletal");
    TestableLattice I(N);
    I.Initialization(false);
    QCOMPARE(static_cast<int>(Parameters::lattice_wave_terms.size()), 9);
    {
        const double q[3] = { 0.21, 0.64, 0.37 };
        double f = 0.0;
        for (const auto& t : Parameters::lattice_wave_terms) f += t[3] * std::cos(2 * pi * (t[0] * q[0] + t[1] * q[1] + t[2] * q[2]) + t[4]);
        const double a = 2 * pi * 2, cx = std::cos(a*q[0]), cy = std::cos(a*q[1]), cz = std::cos(a*q[2]);
        const double g = 2 * (cx*cy + cy*cz + cz*cx) - (std::cos(2*a*q[0]) + std::cos(2*a*q[1]) + std::cos(2*a*q[2]));
        QVERIFY2(std::abs(f - g) < 1e-5, qPrintable(QString("IWP waves: %1 vs %2").arg(f).arg(g)));
    }

    // Transversely isotropic override: softer across the layers (axis 3).
    QVERIFY(applyParameter("material_ti", "2.0,0.38,0.7", &err));
    double Ct[6][6];
    QVERIFY(Parameters::overrideStiffnessPa(Ct));
    QVERIFY(Ct[2][2] < Ct[0][0]);
    QVERIFY(std::abs(Ct[0][0] - Ct[1][1]) < 1e-6 * E);
    QVERIFY(Ct[3][3] < Ct[5][5]);                 // G_13 = r * G_12

    // Validation and the fields switch.
    QVERIFY(!applyParameter("material_iso", "2.0", &err));
    QVERIFY(!applyParameter("material_iso", "-1,0.3", &err));
    QVERIFY(!applyParameter("material_ti", "2,0.3,5", &err));
    QVERIFY(p->getHdf5Fields());
    QVERIFY(applyParameter("no_fields", "true", &err));
    QVERIFY(!p->getHdf5Fields());
}

void TestLattice::testOpenedCellKeepsItsMaterial()
{
    // A generated cell publishes its printed law; the same law read back from
    // the file's parameters must publish the identical solid, so a stress run
    // on an opened dataset cell solves what the dataset solved.
    Parameters* p = Parameters::instance();
    QString err;
    QVERIFY(applyParameter("material_ti", "2.193,0.4015,0.827", &err));
    const int N = 16;
    p->setSize(N);
    p->setLatticeFamily("plate_sc");
    p->setCellsPerEdge(1);
    p->setRelativeDensity(0.4);
    TestableLattice L(N);
    L.Initialization(false);
    QCOMPARE(static_cast<int>(Parameters::phaseAssignment.materials.size()), 1);
    const auto generated = Parameters::phaseAssignment.materials[0].C;

    Parameters::phaseAssignment.clear();
    QVariantMap file{ { "mat_e_gpa", 2.193 }, { "mat_nu", 0.4015 }, { "mat_aniso", 0.827 }, { "mat_source", "override" } };
    QVERIFY(Parameters::publishCellMaterial(file, QStringLiteral("Lattice")));
    QVERIFY(Parameters::phaseAssignment.isUsable(1));
    const auto& opened = Parameters::phaseAssignment.materials[0].C;
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            QVERIFY2(std::abs(opened[i][j] - generated[i][j]) <= 1e-9 * std::abs(generated[0][0]),
                     qPrintable(QString("C[%1][%2]: %3 vs %4").arg(i).arg(j).arg(opened[i][j]).arg(generated[i][j])));
    QVERIFY(opened[2][2] < opened[0][0]);         // weaker along the build axis

    // A database row is taken from the database; other algorithms and files
    // without a material publish nothing.
    QVERIFY(Parameters::publishCellMaterial({ { "mat_e_gpa", 2.0 }, { "mat_nu", 0.38 }, { "mat_source", "db:PETG" } }, "Lattice"));
    QCOMPARE(Parameters::phaseAssignment.materials[0].name, QStringLiteral("PETG"));
    QVERIFY(std::abs(Parameters::phaseAssignment.materials[0].C[0][0] - 3.744e9) < 1e3);
    Parameters::phaseAssignment.clear();
    QVERIFY(!Parameters::publishCellMaterial(file, QStringLiteral("Voronoi")));
    QVERIFY(!Parameters::publishCellMaterial({}, QStringLiteral("Lattice")));
    QVERIFY(Parameters::phaseAssignment.materials.empty());
}

void TestLattice::testPlateLatticesSymmetryAndUnion()
{
    Parameters* p = Parameters::instance();
    const int N = 32;
    p->setSize(N);
    p->setCellsPerEdge(1);
    // Plates are whole-voxel shells, symmetric about the mid-plane, so the
    // thickness steps by 2 voxels: 4 voxels on a 32-cube fills exactly
    // 1 - (28/32)^3 = 0.3301 of the cell. Ask for that.
    const double rho4 = 1.0 - std::pow(28.0 / 32.0, 3.0);
    p->setRelativeDensity(rho4);

    // {100} plates, no holes: both sides of every plate tie exactly, so the
    // density is hit on the nose and the plate is 4 voxels thick.
    {
        p->setLatticeFamily("plate_sc");
        TestableLattice L(N);
        L.Initialization(false);
        L.Generate_To_End();
        QVERIFY2(std::abs(L.achievedDensity() - rho4) < 1e-9, qPrintable(QString::number(L.achievedDensity())));
        QVERIFY2(std::abs(L.wallVoxels() - 4.0) < 1e-6, qPrintable(QString::number(L.wallVoxels())));
        QVERIFY(L.fieldAt(0.5, 0.5 * N, 0.5 * N) < 0.02);          // on the x = 0 plate (half a voxel off)
        QVERIFY(std::abs(L.fieldAt(0.5 * N, 0.5 * N, 0.5 * N) - 0.5) < 1e-9);   // cell centre: half a cell from every plate
    }
    // Holes: the face centre becomes void, the rest of the plate stays, and
    // the level is solved with the holes in place (density within one shell).
    {
        p->setLatticeFamily("plate_sc");
        p->setPlateHole(0.5);
        TestableLattice L(N);
        L.Initialization(false);
        L.Generate_To_End();
        QVERIFY2(std::abs(L.achievedDensity() - rho4) < 0.08, qPrintable(QString::number(L.achievedDensity())));
        // Hole through the face centre: the x-plate is gone there, the nearest
        // remaining plates (y = 0, z = 0) are half a cell away, so the point is void.
        QVERIFY(std::abs(L.fieldAt(0.5, 0.5 * N, 0.5 * N) - 0.5) < 1e-6);
        QVERIFY(L.fieldAt(0.5, 0.5 * N, 0.5 * N) > L.threshold());
        QVERIFY(L.fieldAt(0.5, 0.05 * N, 0.05 * N) < 0.02);        // plate corner is solid
        p->setPlateHole(0.0);
    }
    // SC+FCC: at the cell centre the nearest plate is a {111} one, 0.5/sqrt(3)
    // away; the field has the symmetry of the cube.
    {
        p->setLatticeFamily("plate_sc_fcc");
        TestableLattice L(N);
        L.Initialization(false);
        QVERIFY(std::abs(L.fieldAt(0.5 * N, 0.5 * N, 0.5 * N) - 0.5 / std::sqrt(3.0)) < 1e-9);
        const double a = L.fieldAt(0.2 * N, 0.35 * N, 0.6 * N);
        QVERIFY(std::abs(a - L.fieldAt(0.35 * N, 0.2 * N, 0.6 * N)) < 1e-12);
        QVERIFY(std::abs(a - L.fieldAt(0.8 * N, 0.35 * N, 0.6 * N)) < 1e-12);
        QVERIFY(std::abs(a - L.fieldAt(0.6 * N, 0.2 * N, 0.35 * N)) < 1e-12);
    }
    // Cubic symmetry of a random Fourier cell: the field is invariant under
    // axis permutations and reflections, and the published wave list holds
    // the expanded orbits.
    {
        p->setLatticeFamily("fourier_sheet");
        p->setFourierTerms(6);
        p->setFourierSymmetry("cubic");
        Parameters::seed = 11;
        TestableLattice L(N);
        L.Initialization(false);
        const double a = L.fieldAt(3.3, 10.1, 20.7);
        QVERIFY(std::abs(a - L.fieldAt(10.1, 3.3, 20.7)) < 1e-9);
        QVERIFY(std::abs(a - L.fieldAt(N - 3.3, 10.1, 20.7)) < 1e-9);
        QVERIFY(std::abs(a - L.fieldAt(20.7, N - 10.1, 3.3)) < 1e-9);
        L.Generate_To_End();
        QVERIFY(Parameters::lattice_wave_terms.size() > 6);
        QVERIFY(Parameters::lattice_wave_terms.size() <= 48 * 6);
        p->setFourierSymmetry("none");
        p->setFourierTerms(24);
    }
    // Union plates + octet: the field is the smaller of the two distances.
    {
        p->setLatticeFamily("plate_sc");
        p->setLatticeUnion("octet");
        QCOMPARE(Parameters::latticeUnionKeyStored(), QString("octet"));
        TestableLattice L(N);
        L.Initialization(false);
        // Cell centre: 0.5 from the plates, 0.3536 from the nearest octahedron edge.
        QVERIFY(std::abs(L.fieldAt(0.5 * N, 0.5 * N, 0.5 * N) - 0.35355339) < 1e-6);
        QVERIFY(L.fieldAt(0.5, 0.5 * N, 0.2 * N) < 0.02);           // on the plate (half a voxel off), off the struts
        L.Generate_To_End();
        QVERIFY2(std::abs(L.achievedDensity() - rho4) < 0.08, qPrintable(QString::number(L.achievedDensity())));
        p->setLatticeUnion("none");
        QVERIFY(Parameters::latticeUnionKeyStored().isEmpty());
        // Two strut families cannot be united: the second is dropped.
        p->setLatticeFamily("bcc");
        p->setLatticeUnion("octet");
        TestableLattice M(N);
        M.Initialization(false);
        QVERIFY(std::abs(M.fieldAt(0.5 * N, 0.5 * N, 0.5 * N)) < 1e-9);   // bcc strut through the centre
        p->setLatticeUnion("none");
    }
}

void TestLattice::testSitesCarvingAndSmoothing()
{
    Parameters* p = Parameters::instance();
    const int N = 32;
    p->setSize(N);
    p->setCellsPerEdge(1);
    p->setRelativeDensity(0.30);

    // Spheres at the SC sites: distance to the nearest corner; cylinders:
    // distance to the axis lines through the face centres.
    {
        p->setLatticeFamily("spheres_sc");
        TestableLattice L(N);
        L.Initialization(false);
        QVERIFY(std::abs(L.fieldAt(0.5 * N, 0.5 * N, 0.5 * N) - std::sqrt(3.0) / 2.0) < 1e-9);
        QVERIFY(L.fieldAt(0.5, 0.5, 0.5) < 0.03);
        p->setLatticeFamily("spheres_bcc");
        TestableLattice B(N);
        B.Initialization(false);
        QVERIFY(B.fieldAt(0.5 * N, 0.5 * N, 0.5 * N) < 1e-9);          // site at the centre
        p->setLatticeFamily("cylinders_axes");
        TestableLattice C(N);
        C.Initialization(false);
        QVERIFY(C.fieldAt(0.5, 0.5 * N, 0.5 * N) < 1e-9);               // on the x axis line
        QVERIFY(std::abs(C.fieldAt(0.5, 0.5, 0.5) - std::sqrt(0.5)) < 0.03);   // corner: 0.707 from every line
        C.Generate_To_End();
        QVERIFY(std::abs(C.achievedDensity() - 0.30) < 0.03);
    }
    // Carving: plates minus spheres at the BCC sites of radius 0.3. The cell
    // centre and the corners are gone; a plate point away from the spheres
    // stays; the density is solved on what remains.
    {
        p->setLatticeFamily("plate_sc");
        p->setLatticeSubtract("spheres_bcc");
        p->setLatticeSubtractSize(0.3);
        QCOMPARE(Parameters::latticeSubtractKeyStored(), QString("spheres_bcc"));
        TestableLattice L(N);
        L.Initialization(false);
        QVERIFY(L.fieldAt(0.5 * N, 0.5 * N, 0.5 * N) > 1e100);
        QVERIFY(L.fieldAt(0.5, 0.05 * N, 0.05 * N) > 1e100);
        QVERIFY(L.fieldAt(0.5, 0.5 * N, 0.2 * N) < 0.02);
        L.Generate_To_End();
        QVERIFY2(std::abs(L.achievedDensity() - 0.30) < 0.08, qPrintable(QString::number(L.achievedDensity())));
        // A random family cannot be carved out: ignored, the plates are intact.
        p->setLatticeSubtract("spinodoid");
        TestableLattice M(N);
        M.Initialization(false);
        QVERIFY(std::abs(M.fieldAt(0.5 * N, 0.5 * N, 0.5 * N) - 0.5) < 1e-9);
        p->setLatticeSubtract("none");
        QVERIFY(Parameters::latticeSubtractKeyStored().isEmpty());
    }
    // Smoothing: the blurred field is read from the grid, is continuous
    // (no jump larger than a fraction of the plate step) and still gives
    // the requested density because the level is solved on it.
    {
        p->setLatticeFamily("plate_sc");
        p->setLatticeSmooth(0.05);
        TestableLattice L(N);
        L.Initialization(false);
        double maxJump = 0.0;
        for (int i = 0; i < N - 1; ++i)
            maxJump = std::max(maxJump, std::abs(L.fieldAt(i + 1.5, 0.5 * N, 0.2 * N) - L.fieldAt(i + 0.5, 0.5 * N, 0.2 * N)));
        QVERIFY2(maxJump < 0.5 / N * 2.0, qPrintable(QString::number(maxJump)));   // < 2 voxels of distance per voxel step
        QVERIFY(L.fieldAt(0.5, 0.5 * N, 0.5 * N) < L.fieldAt(0.5 * N, 0.5 * N, 0.5 * N));
        L.Generate_To_End();
        QVERIFY2(std::abs(L.achievedDensity() - 0.30) < 0.02, qPrintable(QString::number(L.achievedDensity())));
        p->setLatticeSmooth(0.0);
    }
}

void TestLattice::testHoneycombShape()
{
    Parameters* p = Parameters::instance();
    const int N = 32;
    p->setSize(N);
    p->setCellsPerEdge(2);
    p->setRelativeDensity(0.30);

    // The custom shape -30 deg / rib 2 on the hexagonal family is exactly the
    // re-entrant preset.
    p->setLatticeFamily("reentrant");
    TestableLattice R(N);
    R.Initialization(false);
    p->setLatticeFamily("honeycomb");
    p->setHoneycombCustom(true);
    p->setHoneycombAngle(-30.0);
    p->setHoneycombRib(2.0);
    TestableLattice H(N);
    H.Initialization(false);
    QCOMPARE(H.cellsAlongY(), R.cellsAlongY());
    QVERIFY(std::abs(H.honeycombStretch() - R.honeycombStretch()) < 1e-12);
    for (double x : { 1.3, 7.9, 20.2 })
        for (double y : { 2.1, 15.5, 29.7 })
            QVERIFY(std::abs(H.fieldAt(x, y, 4.5) - R.fieldAt(x, y, 4.5)) < 1e-12);

    // 0 deg with rib 1: cell width 2, period 2 -> as many cells along Y as along X, no stretch.
    p->setHoneycombAngle(0.0);
    p->setHoneycombRib(1.0);
    TestableLattice B(N);
    B.Initialization(false);
    QCOMPARE(B.cellsAlongY(), 2);
    QVERIFY(std::abs(B.honeycombStretch() - 1.0) < 1e-12);
    B.Generate_To_End();
    // Axis-aligned walls sit on whole-voxel shells (like plates), so on 16
    // voxels per cell the density is quantized: within one shell is enough.
    QVERIFY2(std::abs(B.achievedDensity() - 0.30) < 0.08, qPrintable(QString::number(B.achievedDensity())));

    // A fold-over (rib + sin(angle) <= 0.1) falls back to the preset.
    p->setHoneycombAngle(-50.0);
    p->setHoneycombRib(0.8);
    TestableLattice F(N);
    F.Initialization(false);
    p->setHoneycombCustom(false);
    TestableLattice P(N);
    P.Initialization(false);
    QVERIFY(std::abs(F.honeycombStretch() - P.honeycombStretch()) < 1e-12);
}

namespace {
// Brute-force distance from a point (cell units, one cell) to the orbit of a
// member under the 48 point operations of the cube and the lattice translations.
double orbitDistance(const double X[3], const std::array<float,12>& m)
{
    static const int perm[6][3] = { {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0} };
    auto seg = [](const double* p, const double* a, const double* b) {
        double ab[3], ap[3], L2 = 0.0, t = 0.0;
        for (int i = 0; i < 3; ++i) { ab[i] = b[i] - a[i]; ap[i] = p[i] - a[i]; L2 += ab[i] * ab[i]; t += ab[i] * ap[i]; }
        t = L2 > 0.0 ? std::clamp(t / L2, 0.0, 1.0) : 0.0;
        double d2 = 0.0;
        for (int i = 0; i < 3; ++i) { const double e = a[i] + t * ab[i] - p[i]; d2 += e * e; }
        return std::sqrt(d2);
    };
    auto tri = [&](const double* p, const double* a, const double* b, const double* c) {
        // Projection onto the plane when it falls inside, else the nearest edge.
        double u[3], v[3], n[3], w[3];
        for (int i = 0; i < 3; ++i) { u[i] = b[i] - a[i]; v[i] = c[i] - a[i]; w[i] = p[i] - a[i]; }
        n[0] = u[1] * v[2] - u[2] * v[1]; n[1] = u[2] * v[0] - u[0] * v[2]; n[2] = u[0] * v[1] - u[1] * v[0];
        const double nn = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
        const double dn = (w[0] * n[0] + w[1] * n[1] + w[2] * n[2]) / nn;
        double q[3]; for (int i = 0; i < 3; ++i) q[i] = w[i] - dn * n[i];
        const double uu = u[0]*u[0]+u[1]*u[1]+u[2]*u[2], vv = v[0]*v[0]+v[1]*v[1]+v[2]*v[2], uv = u[0]*v[0]+u[1]*v[1]+u[2]*v[2];
        const double qu = q[0]*u[0]+q[1]*u[1]+q[2]*u[2], qv = q[0]*v[0]+q[1]*v[1]+q[2]*v[2];
        const double den = uu * vv - uv * uv, s = (qu * vv - qv * uv) / den, t = (qv * uu - qu * uv) / den;
        if (s >= 0.0 && t >= 0.0 && s + t <= 1.0) return std::abs(dn) * std::sqrt(nn);
        return std::min({ seg(p, a, b), seg(p, b, c), seg(p, c, a) });
    };
    double best = 1e300;
    for (const auto& pm : perm)
        for (int sg = 0; sg < 8; ++sg)
            for (int tx = -1; tx <= 1; ++tx)
                for (int ty = -1; ty <= 1; ++ty)
                    for (int tz = -1; tz <= 1; ++tz) {
                        double P[3][3];
                        for (int k = 0; k < 3; ++k)
                            for (int i = 0; i < 3; ++i)
                                P[k][i] = (((sg >> i) & 1) ? -1.0 : 1.0) * m[static_cast<size_t>(1 + 3 * k + pm[i])]
                                        + (i == 0 ? tx : (i == 1 ? ty : tz));
                        double d = 1e300;
                        const int kind = static_cast<int>(m[0]);
                        if (kind == 0)      d = seg(X, P[0], P[1]);
                        else if (kind == 1) d = tri(X, P[0], P[1], P[2]);
                        else if (kind == 2) { double e = 0.0; for (int i = 0; i < 3; ++i) e += (X[i] - P[0][i]) * (X[i] - P[0][i]);
                                              d = std::abs(std::sqrt(e) - m[10]); }
                        best = std::min(best, d);
                    }
    return best;
}
} // namespace

void TestLattice::testCubicAssemblyIsTheOrbitDistance()
{
    // Members inside the asymmetric unit 0 <= z <= y <= x <= 1/2, read from a
    // file (no windows): the field is the distance to the nearest image of any
    // member over its weight, i.e. the folding into the unit is exact.
    Parameters* p = Parameters::instance();
    const int N = 32;
    p->setSize(N);
    p->setCellsPerEdge(1);
    p->setRelativeDensity(0.25);
    p->setLatticeFamily("cubic_assembly");

    QTemporaryFile file;
    QVERIFY(file.open());
    file.write("# kind x0 y0 z0 x1 y1 z1 x2 y2 z2 radius weight\n");
    file.write("strut 0 0 0   0.5 0.5 0.5   0 0 0   0 1.0\n");          // BCC diagonal
    file.write("plate 0.5 0 0   0.5 0.5 0   0.25 0.25 0.25   0 0.8\n");  // an inclined triangle
    file.write("strut 0.30 0.10 0.05   0.45 0.30 0.20   0 0 0   0 1.2\n");// in general position
    file.write("shell 0.5 0.5 0.5   0 0 0   0 0 0   0.3 0.9\n");         // sphere about the cell centre
    file.flush();
    p->setAssemblyFile(file.fileName());

    TestableLattice L(N);
    L.Initialization(false);
    QCOMPARE(static_cast<int>(Parameters::lattice_assembly_terms.size()), 4);

    std::mt19937 rng(11);
    std::uniform_real_distribution<double> U(0.0, 1.0);
    for (int t = 0; t < 60; ++t) {
        const double X[3] = { U(rng), U(rng), U(rng) };
        double brute = 1e300;
        for (const auto& m : Parameters::lattice_assembly_terms)
            brute = std::min(brute, orbitDistance(X, m) / m[11]);
        const double f = L.fieldAt(X[0] * N, X[1] * N, X[2] * N);
        QVERIFY2(std::abs(f - brute) < 1e-6, qPrintable(QString("at (%1, %2, %3): field %4, orbit distance %5")
                                                          .arg(X[0]).arg(X[1]).arg(X[2]).arg(f).arg(brute)));
    }
}

void TestLattice::testCubicAssemblyDrawIsConnectedAndRoundTrips()
{
    Parameters* p = Parameters::instance();
    const int N = 48;
    p->setSize(N);
    p->setCellsPerEdge(1);
    p->setRelativeDensity(0.40);
    p->setLatticeCellMm(20.0);
    p->setLatticeFamily("cubic_assembly");
    p->setAssemblyStruts(3);
    p->setAssemblyPlates(2);
    p->setAssemblyShells(1);
    p->setAssemblyHoles(2);

    // Every later member stands on an earlier one and windows keep clear of the
    // feet, so a drawn cell is one body; the same seed draws the same cell.
    std::vector<int32_t> first;
    for (int seed : { 3, 8, 21, 34 }) {
        Parameters::seed = seed;
        TestableLattice L(N);
        L.Initialization(false);
        L.Generate_To_End();
        QVERIFY2(Parameters::printability.computed, "printability report");
        // One body; single-voxel specks where a curved surface grazes the level
        // are tolerated, as by the printability check (print_max_island_vox).
        QVERIFY2(Parameters::printability.largest_island_vox <= 8,
                 qPrintable(QString("seed %1: an island of %2 voxels").arg(seed)
                                .arg(static_cast<qulonglong>(Parameters::printability.largest_island_vox))));
        QVERIFY(std::abs(L.achievedDensity() - 0.40) < 0.03);
        if (first.empty()) first = L.flat();
    }
    Parameters::seed = 3;
    {
        TestableLattice A(N);
        A.Initialization(false);
        A.Generate_To_End();
        QVERIFY(A.flat() == first);
    }

    // The published table rebuilds the same cell from a file.
    Parameters::seed = 8;
    TestableLattice B(N);
    B.Initialization(false);
    B.Generate_To_End();
    const std::vector<int32_t> drawn = B.flat();
    const auto terms = Parameters::lattice_assembly_terms;
    QVERIFY(terms.size() >= 6);
    QTemporaryFile file;
    QVERIFY(file.open());
    for (const auto& t : terms) {
        QString line = QString::number(static_cast<int>(t[0]));
        for (int i = 1; i < 12; ++i) line += " " + QString::number(static_cast<double>(t[static_cast<size_t>(i)]), 'g', 9);
        file.write((line + "\n").toUtf8());
    }
    file.flush();
    p->setAssemblyFile(file.fileName());
    Parameters::seed = 999;               // the file wins over the seed
    TestableLattice C(N);
    C.Initialization(false);
    C.Generate_To_End();
    QVERIFY(C.flat() == drawn);
    QCOMPARE(Parameters::lattice_assembly_terms.size(), terms.size());
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

    QVERIFY(applyParameter("lattice_family", "cubic_assembly", &err));
    QCOMPARE(Parameters::latticeFamilyKeyStored(), QString("cubic_assembly"));
    QVERIFY(applyParameter("assembly_struts", "0", &err));
    QCOMPARE(p->getAssemblyStruts(), 0);
    QVERIFY(applyParameter("assembly_plates", "5", &err));
    QCOMPARE(p->getAssemblyPlates(), 5);
    QVERIFY(applyParameter("assembly_shells", "2", &err));
    QVERIFY(applyParameter("assembly_holes", "3", &err));
    QVERIFY(applyParameter("assembly_spread", "0.4", &err));
    QCOMPARE(p->getAssemblySpread(), 0.4);
    QVERIFY(!applyParameter("assembly_struts", "13", &err));
    QVERIFY(!applyParameter("assembly_shells", "5", &err));
    QVERIFY(!applyParameter("assembly_holes", "-1", &err));
    QVERIFY(!applyParameter("assembly_spread", "0.9", &err));
    QVERIFY(!applyParameter("assembly_file", "  ", &err));
    QVERIFY(applyParameter("lattice_family", "Schwarz P (skeletal)", &err));   // the checks below expect it

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

    QVERIFY(applyParameter("fourier_symmetry", "Cubic", &err));
    QCOMPARE(p->getFourierSymmetry(), QString("cubic"));
    QVERIFY(!applyParameter("fourier_symmetry", "hex", &err));
    QVERIFY(applyParameter("plate_hole", "0.4", &err));
    QCOMPARE(p->getPlateHole(), 0.4);
    QVERIFY(!applyParameter("plate_hole", "1.5", &err));
    QVERIFY(applyParameter("lattice_union", "octet", &err));
    QCOMPARE(Parameters::latticeUnionKeyStored(), QString("octet"));
    QCOMPARE(p->getLatticeUnion(), QString("Octet truss"));
    QVERIFY(!applyParameter("lattice_union", "kagome", &err));
    QVERIFY(applyParameter("lattice_union", "none", &err));
    QVERIFY(Parameters::latticeUnionKeyStored().isEmpty());
    QVERIFY(applyParameter("lattice_family", "plate_sc_fcc", &err));
    QCOMPARE(Parameters::latticeFamilyKeyStored(), QString("plate_sc_fcc"));
    QVERIFY(Commandline_Parser::isValidLatticeFamily("plate_bcc"));

    QVERIFY(applyParameter("lattice_subtract", "spheres_fcc", &err));
    QCOMPARE(Parameters::latticeSubtractKeyStored(), QString("spheres_fcc"));
    QVERIFY(!applyParameter("lattice_subtract", "kagome", &err));
    QVERIFY(applyParameter("lattice_subtract", "none", &err));
    QVERIFY(Parameters::latticeSubtractKeyStored().isEmpty());
    QVERIFY(applyParameter("lattice_subtract_size", "0.35", &err));
    QCOMPARE(p->getLatticeSubtractSize(), 0.35);
    QVERIFY(!applyParameter("lattice_subtract_size", "0", &err));
    QVERIFY(!applyParameter("lattice_subtract_size", "1.5", &err));
    QVERIFY(applyParameter("lattice_smooth", "0.05", &err));
    QCOMPARE(p->getLatticeSmooth(), 0.05);
    QVERIFY(!applyParameter("lattice_smooth", "0.5", &err));
    QVERIFY(Commandline_Parser::isValidLatticeFamily("cylinders_axes"));

    QVERIFY(!p->getHoneycombCustom());
    QVERIFY(applyParameter("honeycomb_angle", "15", &err));
    QCOMPARE(p->getHoneycombAngle(), 15.0);
    QVERIFY(p->getHoneycombCustom());                      // setting the angle switches the custom shape on
    QVERIFY(!applyParameter("honeycomb_angle", "60", &err));
    QVERIFY(applyParameter("honeycomb_angle", "-30", &err));   // re-entrant: negative is valid
    QCOMPARE(p->getHoneycombAngle(), -30.0);
    QVERIFY(applyParameter("honeycomb_angle", "0", &err));
    QCOMPARE(p->getHoneycombAngle(), 0.0);
    QVERIFY(applyParameter("honeycomb_rib", "2.5", &err));
    QCOMPARE(p->getHoneycombRib(), 2.5);
    QVERIFY(!applyParameter("honeycomb_rib", "0.1", &err));
    QVERIFY(applyParameter("honeycomb_custom", "false", &err));
    QVERIFY(!p->getHoneycombCustom());
}

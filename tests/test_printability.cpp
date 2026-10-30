#include "test_printability.h"
#include <QTest>
#include <algorithm>
#include <cmath>
#include <functional>
#include "printability.h"
#include "parameters.h"
#include "parent_algorithm.h"
#include "lattice.h"
#include "commandline_parser.h"

namespace {

struct Cube {
    int N;
    int32_t*** v;
    explicit Cube(int n, const std::function<bool(int,int,int)>& solid) : N(n)
    {
        v = Parent_Algorithm::Create3D<int32_t>(n, n, n);
        for (int x = 0; x < n; ++x)
            for (int y = 0; y < n; ++y)
                for (int z = 0; z < n; ++z) v[x][y][z] = solid(x, y, z) ? 1 : 0;
    }
    ~Cube() { Parent_Algorithm::Delete3D(v); }
};

PrintabilityThresholds thresholds(double voxel_mm = 0.25)
{
    PrintabilityThresholds t;
    t.voxel_mm = voxel_mm;
    return t;
}

class TestableLattice : public Lattice
{
public:
    using Lattice::voxels;
    explicit TestableLattice(short int n) : Lattice(n, 1) { Allocate_Memory(); }
    ~TestableLattice() override { if (voxels) { Delete3D(voxels); voxels = nullptr; } }
};

} // namespace

void TestPrintability::init()
{
    Parameters::resetDefaults();
    Parameters::seed = 5;
}

void TestPrintability::testIslandsAreCounted()
{
    // A 10^3 block and a detached 3^3 block.
    Cube c(24, [](int x, int y, int z) {
        const bool big   = x >= 2 && x < 12 && y >= 2 && y < 12 && z >= 2 && z < 12;
        const bool small = x >= 16 && x < 19 && y >= 16 && y < 19 && z >= 16 && z < 19;
        return big || small;
    });
    const PrintabilityReport r = checkPrintability(c.v, c.N, thresholds());
    QVERIFY(r.computed);
    QCOMPARE(r.components, 2);
    QCOMPARE(r.islands, 1);
    QCOMPARE(static_cast<int>(r.largest_island_vox), 27);
    QVERIFY(std::abs(r.island_fraction - 27.0 / 1027.0) < 1e-9);
    QVERIFY(std::abs(r.island_fraction_box - 27.0 / 1027.0) < 1e-9);
    QVERIFY(!r.ok);
    QVERIFY(r.verdict.find("island") != std::string::npos);

    // A single-voxel speck is a voxelization artefact and is tolerated. The
    // block stands on the bed (z = 0) so no overhang rule fires.
    Cube d(24, [](int x, int y, int z) {
        const bool big = x >= 2 && x < 12 && y >= 2 && y < 12 && z < 10;
        return big || (x == 20 && y == 20 && z == 20);
    });
    PrintabilityThresholds speckOnly = thresholds();
    speckOnly.min_directions = 0;          // the block reaches no neighbour; this case is about the speck
    const PrintabilityReport rd = checkPrintability(d.v, d.N, speckOnly);
    QCOMPARE(rd.islands, 1);
    QCOMPARE(static_cast<int>(rd.largest_island_vox), 1);
    QVERIFY(rd.ok);

    // A finite piece that straddles a periodic face is still a finite piece
    // in the tiled lattice: an island. A bar that runs the whole way round
    // is anchored (it meets its own image), even though it never touches the
    // body -- but cut out as a box it hangs loose.
    Cube e(24, [](int x, int y, int z) {
        const bool body  = y >= 10 && y < 14 && z >= 10 && z < 14;              // bar along X, wraps
        const bool piece = (x < 3 || x >= 21) && y >= 20 && y < 23 && z >= 20 && z < 23;
        return body || piece;
    });
    const PrintabilityReport re = checkPrintability(e.v, e.N, thresholds());
    QCOMPARE(re.components, 2);
    QCOMPARE(re.islands, 1);
    QCOMPARE(static_cast<int>(re.largest_island_vox), 54);
    QVERIFY(!re.ok);

    Cube f(24, [](int x, int y, int z) {
        const bool body = y >= 10 && y < 14 && z >= 10 && z < 14;
        const bool bar  = y >= 20 && y < 23 && z >= 20 && z < 23;               // second bar along X, wraps
        (void)x;
        return body || bar;
    });
    const PrintabilityReport rf = checkPrintability(f.v, f.N, thresholds());
    QCOMPARE(rf.components, 2);
    QCOMPARE(rf.islands, 0);
    QVERIFY(rf.island_fraction < 1e-9);
    QVERIFY2(rf.island_fraction_box > 0.3, qPrintable(QString("box-cut island fraction %1").arg(rf.island_fraction_box)));
}

void TestPrintability::testSeparateBodiesFail()
{
    // Three slabs normal to Z: each wraps the periodic cube in X and Y, so none
    // is an island, yet they are three parts, not one.
    Cube c(24, [](int x, int y, int z) { (void)x; (void)y; return (z % 8) < 4; });
    const PrintabilityReport r = checkPrintability(c.v, c.N, thresholds());
    QCOMPARE(r.components, 3);
    QCOMPARE(r.islands, 0);
    QVERIFY(!r.ok);
    QVERIFY(QString::fromStdString(r.verdict).contains("3 separate solid bodies"));
    // With both limits lifted the same cube passes (thick slabs, nothing to support).
    PrintabilityThresholds t = thresholds();
    t.max_components = 0;
    t.min_directions = 0;
    QVERIFY(checkPrintability(c.v, c.N, t).ok);
    // One slab is one body, but only joined in X and Y.
    Cube one(24, [](int x, int y, int z) { (void)x; (void)y; return z < 8; });
    const PrintabilityReport r1 = checkPrintability(one.v, one.N, thresholds());
    QCOMPARE(r1.components, 1);
    QCOMPARE(r1.connected_directions, 2);
    QVERIFY(!r1.ok);
}

void TestPrintability::testBodyMustJoinInThreeDirections()
{
    // A slab normal to Z: one body, wraps in X and Y, no path along Z.
    Cube slab(24, [](int, int, int z) { return z < 8; });
    const PrintabilityReport rs = checkPrintability(slab.v, slab.N, thresholds());
    QCOMPARE(rs.connected_directions, 2);
    QCOMPARE(rs.unjoined_axes, 4);
    QVERIFY(!rs.ok);
    QVERIFY2(QString::fromStdString(rs.verdict).contains("2 of 3 directions (no path along Z)"), rs.verdict.c_str());

    // A rod along X: one direction.
    Cube rod(24, [](int, int y, int z) { return y >= 8 && y < 16 && z >= 8 && z < 16; });
    const PrintabilityReport rr = checkPrintability(rod.v, rod.N, thresholds());
    QCOMPARE(rr.connected_directions, 1);
    QCOMPARE(rr.unjoined_axes, 6);
    QVERIFY(!rr.ok);

    // A diagonal rod in the XZ plane: it moves along X and Z, yet it is still
    // one direction -- no axis is missing, the rank is what counts.
    Cube diag(24, [](int x, int y, int z) {
        const int d = ((x - z) % 24 + 24) % 24;
        return y >= 8 && y < 16 && (d < 5 || d > 19);
    });
    const PrintabilityReport rd = checkPrintability(diag.v, diag.N, thresholds());
    QCOMPARE(rd.components, 1);
    QCOMPARE(rd.connected_directions, 1);
    QCOMPARE(rd.unjoined_axes, 2);

    // Three joined rods (a simple cubic strut frame): a 3-D network.
    Cube frame(24, [](int x, int y, int z) {
        const bool ix = y >= 8 && y < 16, iy = x >= 8 && x < 16, iz = z >= 8 && z < 16;
        return (ix && iz) || (iy && iz) || (ix && iy);
    });
    const PrintabilityReport rf = checkPrintability(frame.v, frame.N, thresholds());
    QCOMPARE(rf.connected_directions, 3);
    QCOMPARE(rf.unjoined_axes, 0);
    QVERIFY2(rf.verdict.find("directions") == std::string::npos, rf.verdict.c_str());   // (its bars overhang; not this test)

    // The check can be switched off, and it does not apply to a finite part.
    PrintabilityThresholds off = thresholds();
    off.min_directions = 0;
    QVERIFY(checkPrintability(slab.v, slab.N, off).ok);
    PrintabilityThresholds box = thresholds();
    box.periodic = false;
    QVERIFY(checkPrintability(slab.v, slab.N, box).verdict.find("directions") == std::string::npos);
}

void TestPrintability::testPeriodicWrapJoinsAcrossFaces()
{
    // Two slabs touching opposite X faces: one body on the torus, two in a box.
    Cube c(16, [](int x, int, int) { return x < 3 || x >= 13; });
    PrintabilityThresholds t = thresholds();
    t.periodic = true;
    QCOMPARE(checkPrintability(c.v, c.N, t).components, 1);
    t.periodic = false;
    QCOMPARE(checkPrintability(c.v, c.N, t).components, 2);
}

void TestPrintability::testThinWallsAreDetected()
{
    // Voxel 0.25 mm, minimum wall 0.8 mm -> opening radius 1.6 voxels.
    // A 2-voxel slab (0.5 mm) is all too thin; an 8-voxel slab (2 mm) is fine.
    Cube thin(16, [](int x, int, int) { return x >= 7 && x < 9; });
    const PrintabilityReport rt = checkPrintability(thin.v, thin.N, thresholds(0.25));
    QVERIFY2(rt.thin_fraction > 0.99, qPrintable(QString("thin fraction %1").arg(rt.thin_fraction)));
    QVERIFY(!rt.ok);
    QVERIFY(rt.verdict.find("thinner") != std::string::npos);

    Cube thick(16, [](int x, int, int) { return x >= 4 && x < 12; });
    PrintabilityThresholds slabOk = thresholds(0.25);
    slabOk.min_directions = 0;             // a slab joins in two directions; this test is about thickness
    const PrintabilityReport rk = checkPrintability(thick.v, thick.N, slabOk);
    QVERIFY2(rk.thin_fraction < 1e-9, qPrintable(QString("thin fraction %1").arg(rk.thin_fraction)));
    QVERIFY(rk.ok);
    QVERIFY(std::abs(rk.opening_radius_vox - 1.6) < 1e-9);
    QVERIFY(rk.thin_check_resolved);
}

void TestPrintability::testThinWallsIgnoreTheVoxelStaircase()
{
    // Voxel 0.25 mm, opening radius 1.6 voxels. A periodic strut along
    // [1 2 3] through the cube centre, 1.5 mm across (6 voxels): no part of it
    // is thinner than 0.8 mm, but its surface is a staircase of voxel
    // corners -- the opening without slack called 3.6 % of it thin.
    const int N = 48;
    const double ax[3] = { 1.0 / std::sqrt(14.0), 2.0 / std::sqrt(14.0), 3.0 / std::sqrt(14.0) };
    auto strutDistance = [&](int x, int y, int z) {
        const double p[3] = { (x + 0.5) / N - 0.5, (y + 0.5) / N - 0.5, (z + 0.5) / N - 0.5 };
        double best = 1e9;
        for (int i = -2; i <= 2; ++i)
            for (int j = -2; j <= 2; ++j)
                for (int k = -2; k <= 2; ++k) {
                    const double d[3] = { p[0] - i, p[1] - j, p[2] - k };
                    const double s = d[0] * ax[0] + d[1] * ax[1] + d[2] * ax[2];
                    double q2 = 0.0;
                    for (int a = 0; a < 3; ++a) { const double q = d[a] - s * ax[a]; q2 += q * q; }
                    best = std::min(best, std::sqrt(q2));
                }
        return best * N * 0.25;                    // mm
    };
    Cube strut(N, [&](int x, int y, int z) { return strutDistance(x, y, z) <= 0.75; });
    PrintabilityThresholds rod = thresholds(0.25);
    rod.min_directions = 0;                        // one rod joins in one direction; this test is about thickness
    const PrintabilityReport rs = checkPrintability(strut.v, strut.N, rod);
    QVERIFY2(rs.thin_fraction < 1e-3, qPrintable(QString("thick inclined strut: thin fraction %1").arg(rs.thin_fraction)));

    // A slab 0.6 mm thick at an angle to the axes is too thin everywhere.
    Cube slab(N, [&](int x, int y, int) {
        const double f = ((x + 0.5) + 2.0 * (y + 0.5)) / N;   // normal (1, 2, 0): periodic in the cube
        const double d = std::abs(f - std::round(f)) / std::sqrt(5.0) * N * 0.25;
        return d <= 0.3;
    });
    const PrintabilityReport rl = checkPrintability(slab.v, slab.N, rod);
    QVERIFY2(rl.thin_fraction > 0.99, qPrintable(QString("thin inclined slab: thin fraction %1").arg(rl.thin_fraction)));
}

void TestPrintability::testOverhangPicksTheBuildDirection()
{
    // A mushroom: a 4x4 column from the bottom and a 14x14 plate on the top
    // two layers. Built along +Z the plate hangs in the air; built along -Z
    // the plate is on the bed and the column stands on it.
    const int N = 20;
    Cube c(N, [N](int x, int y, int z) {
        const bool column = x >= 8 && x < 12 && y >= 8 && y < 12 && z < N - 2;
        const bool plate  = x >= 3 && x < 17 && y >= 3 && y < 17 && z >= N - 2;
        return column || plate;
    });
    PrintabilityThresholds t = thresholds(0.5);
    t.periodic = false;
    const PrintabilityReport r = checkPrintability(c.v, c.N, t);
    // Plate 14x14 on a 4x4 column: the first plate layer is supported only
    // inside the 6x6 cone above the column -> 160 of 680 solid voxels hang.
    QVERIFY2(std::abs(r.unsupported[4] - 160.0 / 680.0) < 1e-9, qPrintable(QString("+Z unsupported %1").arg(r.unsupported[4])));
    QVERIFY2(r.unsupported[5] < 1e-9, qPrintable(QString("-Z unsupported %1").arg(r.unsupported[5])));
    QCOMPARE(r.best_direction, 5);
    QCOMPARE(QString(PrintabilityReport::directionName(r.best_direction)), QString("-Z"));
    QVERIFY(r.ok);
}

void TestPrintability::testClosedPoresAreReported()
{
    // Solid cube with an open void slab at x < 2 and a sealed 4^3 bubble inside.
    const int N = 16;
    Cube c(N, [](int x, int y, int z) {
        const bool slab   = x < 2;
        const bool bubble = x >= 8 && x < 12 && y >= 6 && y < 10 && z >= 6 && z < 10;
        return !(slab || bubble);
    });
    const PrintabilityReport r = checkPrintability(c.v, c.N, thresholds(0.5));
    QCOMPARE(r.void_components, 2);
    QVERIFY(std::abs(r.closed_pore_fraction - 64.0 / (N * N * N)) < 1e-9);
    QCOMPARE(r.components, 1);
}

void TestPrintability::testGyroidPassesAndReportIsPublished()
{
    Parameters* p = Parameters::instance();
    const int N = 32;
    p->setSize(N);
    p->setCellsPerEdge(1);
    p->setRelativeDensity(0.35);
    p->setLatticeFamily("gyroid_sheet");
    p->setLatticeCellMm(10.0);          // voxel 0.3125 mm, wall ~1.1 mm

    TestableLattice L(N);
    L.Initialization(false);
    L.Generate_To_End();

    const PrintabilityReport& r = Parameters::printability;
    QVERIFY(r.computed);
    QCOMPARE(r.components, 1);
    QVERIFY(r.island_fraction < 1e-9);
    QVERIFY(r.closed_pore_fraction < 1e-9);
    QVERIFY2(r.thin_fraction < 0.05, qPrintable(QString("thin %1").arg(r.thin_fraction)));
    QVERIFY2(r.unsupported_best < 0.2, qPrintable(QString("unsupported %1").arg(r.unsupported_best)));
    QVERIFY(std::abs(Parameters::lattice_voxel_mm - 10.0 / 32.0) < 1e-12);
}

void TestPrintability::testUniformWallMakesRandomSheetsPrintable()
{
    // A random Fourier sheet thickened by |f| has walls of very uneven
    // thickness; thickened by |f|/|grad f| the thickness is nearly uniform,
    // so far less of the solid falls below the minimum wall.
    Parameters* p = Parameters::instance();
    const int N = 48;
    p->setSize(N);
    p->setCellsPerEdge(1);
    // At 35 % density a uniform wall is ~1 mm in a 12 mm cell, above the
    // 0.8 mm minimum; at 25 % it would be ~0.7 mm, uniformly too thin.
    p->setRelativeDensity(0.35);
    p->setLatticeFamily("fourier_sheet");
    p->setFourierTerms(16);
    p->setFourierKmin(1.0);
    p->setFourierKmax(3.0);
    p->setLatticeCellMm(12.0);           // voxel 0.25 mm, minimum wall 0.8 mm

    Parameters::seed = 1;
    p->setLatticeUniformWall(false);
    TestableLattice a(N);
    a.Initialization(false);
    a.Generate_To_End();
    const double thinPlain = Parameters::printability.thin_fraction;

    Parameters::seed = 1;
    p->setLatticeUniformWall(true);
    TestableLattice b(N);
    b.Initialization(false);
    b.Generate_To_End();
    const double thinUniform = Parameters::printability.thin_fraction;

    QVERIFY2(thinPlain > 0.1, qPrintable(QString("plain thin fraction %1").arg(thinPlain)));
    QVERIFY2(thinUniform < 0.05, qPrintable(QString("uniform %1 vs plain %2").arg(thinUniform).arg(thinPlain)));
    QVERIFY(Parameters::printability.ok);
    QVERIFY(std::abs(b.achievedDensity() - 0.35) < 0.01);
    QVERIFY(b.wallVoxels() > 0.0);
    QVERIFY(Parameters::printability.mean_thickness_vox > 0.0);

    QString err;
    QVERIFY(applyParameter("lattice_uniform_wall", "true", &err));
    QVERIFY(p->getLatticeUniformWall());
}

void TestPrintability::testCliOptions()
{
    Parameters* p = Parameters::instance();
    QString err;
    QVERIFY(applyParameter("lattice_cell_mm", "8", &err));
    QCOMPARE(p->getLatticeCellMm(), 8.0);
    QVERIFY(applyParameter("print_min_wall_mm", "1.2", &err));
    QCOMPARE(p->getPrintMinWallMm(), 1.2);
    QVERIFY(applyParameter("print_max_overhang_deg", "60", &err));
    QCOMPARE(p->getPrintMaxOverhangDeg(), 60.0);
    QVERIFY(!applyParameter("print_max_overhang_deg", "95", &err));
    QVERIFY(applyParameter("print_max_island_vox", "27", &err));
    QCOMPARE(p->getPrintMaxIslandVox(), 27);
    QVERIFY(!applyParameter("print_max_island_vox", "-1", &err));
    QVERIFY(applyParameter("print_max_components", "2", &err));
    QCOMPARE(p->getPrintMaxComponents(), 2);
    QVERIFY(!applyParameter("print_max_components", "-1", &err));
    QVERIFY(!applyParameter("print_max_thin_fraction", "1.5", &err));
    QVERIFY(applyParameter("require_printable", "true", &err));
    QVERIFY(p->getRequirePrintable());
    const PrintabilityThresholds t = Parameters::printThresholds(0.25);
    QCOMPARE(t.min_wall_mm, 1.2);
    QCOMPARE(t.max_overhang_deg, 60.0);
    QCOMPARE(t.max_island_vox, 27);
    QCOMPARE(t.max_components, 2);
    QCOMPARE(t.voxel_mm, 0.25);
}

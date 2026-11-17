#include "test_cellstiffness.h"
#include <QTest>
#include <array>
#include <atomic>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "../cellstiffness.h"
#include "../cellstiffness_gpu.h"
#include "../fft_solver_session.hpp"
#include "../parameters.h"
#include "../parent_algorithm.h"

using cellstiff::Settings;
using cellstiff::Solution;
using propsearch::Law;
using propsearch::Mat6;

namespace {

size_t at(int n, int x, int y, int z) { return (size_t(z) * size_t(n) + size_t(y)) * size_t(n) + size_t(x); }

// A 12^3 block with a pore of 6 x 4 x 8 voxels off the centre: no two axes alike.
std::vector<uint8_t> poreBlock(int n = 12)
{
    std::vector<uint8_t> solid(size_t(n) * n * n, 1);
    for (int z = 2; z < 10; ++z) for (int y = 5; y < 9; ++y) for (int x = 3; x < 9; ++x) solid[at(n, x, y, z)] = 0;
    return solid;
}

// A bar of 4 x 4 voxels along x through an 8^3 cell: a quarter of the volume, nothing solid across it.
std::vector<uint8_t> barAlongX(int n = 8)
{
    std::vector<uint8_t> solid(size_t(n) * n * n, 0);
    for (int z = 2; z < 6; ++z) for (int y = 2; y < 6; ++y) for (int x = 0; x < n; ++x) solid[at(n, x, y, z)] = 1;
    return solid;
}

double relative(double a, double b) { return std::abs(a - b) / std::max(std::abs(a), std::abs(b)); }

} // namespace

void TestCellStiffness::testSolidCellGivesBackTheLaw()
{
    // No void at all: the cell is the material, so the axes and the shear order of the result can be read off the law.
    const int n = 8;
    const Law law{ 2.0, 0.38, 0.7 };
    const Solution s = cellstiff::solve(std::vector<uint8_t>(size_t(n) * n * n, 1), n, law);
    QVERIFY2(s.ok, s.error.c_str());
    QVERIFY(!s.cancelled);
    QCOMPARE(s.passes, 1);
    QCOMPARE(s.density, 1.0);
    QVERIFY(s.converged);
    QVERIFY(std::abs(s.share - 1.0) < 1e-6);

    const propsearch::Values v = propsearch::moduli(s.C);
    const double E = 2.0e9, G = E / (2.0 * 1.38);
    QVERIFY(relative(v[propsearch::Ex], E) < 1e-6);
    QVERIFY(relative(v[propsearch::Ey], E) < 1e-6);
    QVERIFY(relative(v[propsearch::Ez], 0.7 * E) < 1e-6);      // weaker across the layers
    QVERIFY(relative(v[propsearch::Gxy], G) < 1e-6);
    QVERIFY(relative(v[propsearch::Gyz], 0.7 * G) < 1e-6);
    QVERIFY(relative(v[propsearch::Gxz], 0.7 * G) < 1e-6);
    QVERIFY(std::abs(v[propsearch::NuXy] - 0.38) < 1e-6);
}

void TestCellStiffness::testPorousCellEqualsADirectSolve()
{
    const int n = 12;
    const Law law{ 2.4, 0.35, 0.8 };
    const std::vector<uint8_t> solid = poreBlock(n);
    Settings settings;
    settings.voidEtaFallback = 0.0;   // one solve, whatever the share
    const Solution s = cellstiff::solve(solid, n, law, settings);
    QVERIFY2(s.ok, s.error.c_str());
    QCOMPARE(s.passes, 1);
    QCOMPARE(s.voidEta, 1e-4);
    QVERIFY(s.converged);
    QVERIFY(s.iterations > 6);
    QVERIFY(std::abs(s.density - (1.0 - 6.0 * 4.0 * 8.0 / (12.0 * 12.0 * 12.0))) < 1e-12);

    // The same cell through the session, the way StressAnalysisFFT::computeStiffnessMatrix does it for a
    // dataset cell: six unit loads, the symmetrised matrix, the share from the sensitivity tensor.
    std::vector<int> grain(solid.begin(), solid.end());
    double Cv[6][6];
    QVERIFY(Parameters::lawStiffnessPa(law.eGpa, law.nu, law.aniso, Cv));
    fftsa::Mat6 V{};
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) V[i][j] = Cv[i][j];
    const std::vector<std::array<double, 3>> orient = { { 0, 0, 0 }, { 0, 0, 0 } };
    fftsa::FFTSolverSession session(n, n, n, grain, orient, V, 1e-4);
    session.set_tolerance(1e-5);
    session.set_max_iters(10000);
    session.set_scheme(fftsa::FFTSolverSession::Scheme::Auto);
    session.set_discretization(ffth::FFTHomogenizer::Discretization::Rotated);
    fftsa::Mat6 C{};
    std::vector<std::vector<fftsa::Vec6>> fields(6);
    for (int j = 0; j < 6; ++j) {
        fftsa::Vec6 e{}; e[j] = (j >= 3) ? 0.5 : 1.0;
        const auto step = session.solveLoadCaseFull(e, fields[size_t(j)]);
        for (int i = 0; i < 6; ++i) C[i][j] = step.macro_stress[i];
    }
    fftsa::symmetrize_pipeline_C(C);
    const std::vector<double> T = fftsa::FFTSolverSession::sensitivity_tensor(fields, size_t(n) * n * n);
    const fftsa::Mat6& mandel = session.grain_stiffness_mandel(1);
    static const int perm[6] = { 0, 1, 2, 5, 3, 4 };
    fftsa::Mat6 Cs{};
    for (int a = 0; a < 6; ++a) for (int b = 0; b < 6; ++b) Cs[a][b] = mandel[perm[a]][perm[b]];
    const fftsa::Mat6 predicted = fftsa::FFTSolverSession::predict_stiffness_change(T, Cs);
    double share = 1e300;
    for (int i = 0; i < 6; ++i) share = std::min(share, predicted[i][i] / C[i][i]);

    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            QVERIFY2(std::abs(s.C[i][j] - C[i][j]) <= 1e-9 * C[0][0], qPrintable(QString("C[%1][%2]").arg(i).arg(j)));
    QVERIFY2(std::abs(s.share - share) < 1e-9, qPrintable(QString("%1 against %2").arg(s.share, 0, 'g', 12).arg(share, 0, 'g', 12)));
    QVERIFY(s.share > 0.985 && s.share < 1.0);      // a closed pore: the solid carries the cell

    // With the dataset's settings this cell needs no second solve.
    const Solution rule = cellstiff::solve(solid, n, law);
    QVERIFY(rule.ok);
    QCOMPARE(rule.passes, 1);
    QCOMPARE(rule.share, s.share);
    QCOMPARE(rule.shareFirst, s.share);

    // The three axes of this pore differ, and the result says so.
    const propsearch::Values v = propsearch::moduli(s.C);
    QVERIFY(relative(v[propsearch::Ex], v[propsearch::Ey]) > 0.01);
    QVERIFY(relative(v[propsearch::Ey], v[propsearch::Ez]) > 0.01);
}

void TestCellStiffness::testOccupancyKeepsTheAxes()
{
    const int n = 8;
    int32_t*** voxels = Parent_Algorithm::Create3D<int32_t>(n, n, n);
    for (int x = 0; x < n; ++x) for (int y = 0; y < n; ++y) for (int z = 0; z < n; ++z) voxels[x][y][z] = 0;
    voxels[1][2][3] = 7;                    // any grain number is solid
    std::vector<uint8_t> one = cellstiff::occupancy(voxels, n);
    QCOMPARE(one.size(), size_t(n) * n * n);
    QCOMPARE(int(one[at(n, 1, 2, 3)]), 1);
    QCOMPARE(int(std::count(one.begin(), one.end(), uint8_t(1))), 1);

    // voxels[x][y][z] with a bar along x: the cell must come out stiff along x and nowhere else.
    for (int x = 0; x < n; ++x) for (int y = 0; y < n; ++y) for (int z = 0; z < n; ++z)
        voxels[x][y][z] = (y >= 2 && y < 6 && z >= 2 && z < 6) ? 1 : 0;
    const std::vector<uint8_t> bar = cellstiff::occupancy(voxels, n);
    Parent_Algorithm::Delete3D<int32_t>(voxels);
    QVERIFY(bar == barAlongX(n));

    Settings settings;
    settings.voidEtaFallback = 0.0;
    const Solution s = cellstiff::solve(bar, n, Law{ 2.0, 0.38, 1.0 }, settings);
    QVERIFY2(s.ok, s.error.c_str());
    QCOMPARE(s.density, 0.25);
    const propsearch::Values v = propsearch::moduli(s.C);
    QVERIFY(relative(v[propsearch::Ex], 0.25 * 2.0e9) < 0.01);       // a straight bar: its share of the section
    QVERIFY(v[propsearch::Ey] < 0.05 * v[propsearch::Ex]);
    QVERIFY(v[propsearch::Ez] < 0.05 * v[propsearch::Ex]);
    QVERIFY(relative(v[propsearch::Ey], v[propsearch::Ez]) < 1e-3);
}

void TestCellStiffness::testSofterVoidWhenTheVoidCarriesTheLoad()
{
    // Nothing solid joins the bar to its neighbours across x: in those loads the void holds the cell up, the
    // rule solves again with the softer void, and the share stays below the load-path limit.
    const int n = 8;
    const Law law{ 2.0, 0.38, 1.0 };
    const std::vector<uint8_t> bar = barAlongX(n);
    int firstCalls = 0;
    Solution first;
    int lastPass = -1, lastLoad = -1, calls = 0;
    const Solution s = cellstiff::solve(bar, n, law, Settings(),
        [&](int pass, int load, int iteration, double) {
            QVERIFY(pass >= lastPass);
            if (pass == lastPass) QVERIFY(load >= lastLoad);
            QVERIFY(iteration >= 1);
            lastPass = pass; lastLoad = load; ++calls;
        },
        [&](const Solution& f) { ++firstCalls; first = f; });
    QVERIFY2(s.ok, s.error.c_str());
    QVERIFY(!s.cancelled);
    QCOMPARE(s.passes, 2);
    QCOMPARE(s.voidEta, 1e-6);
    QCOMPARE(firstCalls, 1);
    QCOMPARE(first.passes, 1);
    QCOMPARE(first.voidEta, 1e-4);
    QVERIFY(first.ok);
    QCOMPARE(s.shareFirst, first.share);
    QVERIFY(first.share < 0.985);
    QVERIFY(s.share < cellstiff::kMinLoadPathShare);
    QVERIFY(s.iterations > first.iterations);
    QCOMPARE(lastPass, 1);
    QCOMPARE(lastLoad, 5);
    QVERIFY(calls >= s.iterations);

    // Along the bar nothing changes; across it the stiffness was the void's and falls with it.
    const propsearch::Values a = propsearch::moduli(first.C), b = propsearch::moduli(s.C);
    QVERIFY(relative(a[propsearch::Ex], b[propsearch::Ex]) < 1e-3);
    QVERIFY(b[propsearch::Ey] < 0.1 * a[propsearch::Ey]);

    // A share limit nothing reaches forces the second solve on a healthy cell too; a fallback of 0 switches it off.
    Settings forced;
    forced.minSolidShare = 2.0;
    const Solution twice = cellstiff::solve(std::vector<uint8_t>(bar.size(), 1), n, law, forced);
    QVERIFY(twice.ok);
    QCOMPARE(twice.passes, 1);            // no void: nothing to soften
    Settings never;
    never.voidEtaFallback = 0.0;
    const Solution once = cellstiff::solve(bar, n, law, never);
    QVERIFY(once.ok);
    QCOMPARE(once.passes, 1);
    QCOMPARE(once.share, first.share);
}

void TestCellStiffness::testStopping()
{
    const int n = 8;
    const Law law{ 2.0, 0.38, 1.0 };
    const std::vector<uint8_t> bar = barAlongX(n);

    // Asked to stop before it began.
    std::atomic<bool> stop{ true };
    Solution s = cellstiff::solve(bar, n, law, Settings(), cellstiff::Progress(), cellstiff::FirstPass(), &stop);
    QVERIFY(!s.ok);
    QVERIFY(s.cancelled);
    QVERIFY(s.error.empty());

    // In the middle of a load of the first solve: nothing to show.
    stop = false;
    int seen = 0;
    s = cellstiff::solve(bar, n, law, Settings(), [&](int, int load, int, double) { if (load == 2 && ++seen == 2) stop = true; },
                         cellstiff::FirstPass(), &stop);
    QVERIFY(!s.ok);
    QVERIFY(s.cancelled);
    QCOMPARE(s.passes, 0);

    // During the second solve: the first one's stiffness stays.
    stop = false;
    Solution first;
    s = cellstiff::solve(bar, n, law, Settings(), cellstiff::Progress(), [&](const Solution& f) { first = f; stop = true; }, &stop);
    QVERIFY(s.ok);
    QVERIFY(s.cancelled);
    QCOMPARE(s.passes, 1);
    QCOMPARE(s.voidEta, 1e-4);
    QCOMPARE(s.share, first.share);
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) QCOMPARE(s.C[i][j], first.C[i][j]);

    // What cannot be solved says why.
    QVERIFY(!cellstiff::solve(std::vector<uint8_t>(bar.size(), 0), n, law).error.empty());
    QVERIFY(!cellstiff::solve(std::vector<uint8_t>(10, 1), n, law).error.empty());
    QVERIFY(!cellstiff::solve(bar, n, Law{ 0.0, 0.38, 1.0 }).error.empty());
}

void TestCellStiffness::testCompare()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // The form's units: moduli in MPa. "seed" is not a target.
    const std::map<std::string, double> asked = { { "Ex", 300.0 }, { "nuXy", 0.30 }, { "EzEx", 1.5 }, { "AU", 0.2 }, { "K", 150.0 }, { "seed", 5.0 } };
    propsearch::Values got;
    got.fill(nan);
    got[propsearch::Ex] = 2.7e8;          // Pa
    got[propsearch::NuXy] = 0.25;
    got[propsearch::EzEx] = 1.8;
    got[propsearch::AU] = 0.5;
    got[propsearch::K] = 1.5e8;

    const std::vector<cellstiff::Row> rows = cellstiff::compare(asked, 0.30, &got, 0.33);
    QCOMPARE(rows.size(), size_t(6));
    const char* keys[6] = { "Ex", "nuXy", "EzEx", "AU", "K", "density" };       // the order of propsearch::Target, the density last
    for (int i = 0; i < 6; ++i) QCOMPARE(rows[size_t(i)].key, std::string(keys[i]));

    QVERIFY(rows[0].modulus && rows[0].relative);
    QCOMPARE(rows[0].asked, 300.0);
    QVERIFY(std::abs(rows[0].obtained - 270.0) < 1e-9);          // shown in MPa
    QVERIFY(std::abs(rows[0].deviation + 10.0) < 1e-9);          // percent of what was asked
    QVERIFY(!rows[1].modulus && !rows[1].relative);
    QVERIFY(std::abs(rows[1].deviation + 0.05) < 1e-12);         // Poisson's ratio: the difference
    QVERIFY(!rows[2].modulus && rows[2].relative);
    QVERIFY(std::abs(rows[2].deviation - 20.0) < 1e-9);
    QVERIFY(!rows[3].relative);
    QVERIFY(std::abs(rows[3].deviation - 0.3) < 1e-12);
    QVERIFY(rows[4].modulus);
    QVERIFY(std::abs(rows[4].deviation) < 1e-9);
    QVERIFY(!rows[5].modulus && rows[5].relative);
    QCOMPARE(rows[5].obtained, 0.33);
    QVERIFY(std::abs(rows[5].deviation - 10.0) < 1e-9);

    // Before the solve only the density is known.
    const std::vector<cellstiff::Row> before = cellstiff::compare(asked, 0.30, nullptr, 0.33);
    QCOMPARE(before.size(), size_t(6));
    QVERIFY(std::isnan(before[0].obtained) && std::isnan(before[0].deviation));
    QCOMPARE(before[0].asked, 300.0);
    QVERIFY(std::abs(before[5].deviation - 10.0) < 1e-9);

    // The density was not asked: no row for it. A value the solve could not give has no deviation.
    got[propsearch::K] = nan;
    const std::vector<cellstiff::Row> noDensity = cellstiff::compare(asked, nan, &got, 0.33);
    QCOMPARE(noDensity.size(), size_t(5));
    QVERIFY(std::isnan(noDensity[4].deviation));
}

// The GPU solver (tools/neural/gpu_stiffness.py, a separate process) against the program's own: the same cell, law and
// settings must give the same stiffness. Skipped where no interpreter with PyTorch and a GPU is found.
void TestCellStiffness::testGpuSolveEqualsTheCpuSolve()
{
    const cellstiff::GpuSolver gpu = cellstiff::gpuSolver();
    if (!gpu.available()) QSKIP(qPrintable(QStringLiteral("no GPU solver: ") + gpu.reason));
    const int n = 12;
    const Law law{ 2.4, 0.35, 0.8 };
    const std::vector<uint8_t> solid = poreBlock(n);
    Settings settings;
    settings.voidEtaFallback = 0.0;   // one solve, whatever the share
    const Solution cpu = cellstiff::solve(solid, n, law, settings);
    QVERIFY2(cpu.ok, cpu.error.c_str());
    QVERIFY(cpu.backend.empty());

    int calls = 0, lowest = 0;
    const Solution g = cellstiff::solveOnGpu(gpu, solid, n, law, settings, [&](int, int load, int, double) { ++calls; lowest = std::min(lowest, load); });
    QVERIFY2(g.ok, g.error.c_str());
    QCOMPARE(QString::fromStdString(g.backend), gpu.device);
    QCOMPARE(g.passes, 1);
    QCOMPARE(g.voidEta, 1e-4);
    QVERIFY(g.converged);
    QVERIFY(std::abs(g.density - cpu.density) < 1e-12);
    QVERIFY(std::abs(g.share - cpu.share) < 1e-3);
    if (calls > 0) QVERIFY(lowest < 0);                      // the six loads run together
    double scale = 0.0;
    for (int i = 0; i < 6; ++i) scale = std::max(scale, std::abs(cpu.C[size_t(i)][size_t(i)]));
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            QVERIFY2(std::abs(g.C[size_t(i)][size_t(j)] - cpu.C[size_t(i)][size_t(j)]) < 2e-4 * scale,
                     qPrintable(QString("C[%1][%2]: GPU %3, CPU %4").arg(i).arg(j).arg(g.C[size_t(i)][size_t(j)]).arg(cpu.C[size_t(i)][size_t(j)])));

    // The softer-void rule: a bar along x alone leaves nothing solid across it, so a second solve follows.
    Settings two;
    two.maxIter = 400;
    const std::vector<uint8_t> bar = barAlongX(8);
    int firsts = 0;
    const Solution b = cellstiff::solveOnGpu(gpu, bar, 8, law, two, cellstiff::Progress(), [&](const Solution& first) { ++firsts; QCOMPARE(first.passes, 1); });
    QVERIFY2(b.ok, b.error.c_str());
    QCOMPARE(b.passes, 2);
    QCOMPARE(firsts, 1);
    QCOMPARE(b.voidEta, 1e-6);
    QVERIFY(b.shareFirst < 0.985);

    // A cell of the wrong size is refused without starting anything.
    const Solution wrong = cellstiff::solveOnGpu(gpu, std::vector<uint8_t>(10, 1), 12, law, settings);
    QVERIFY(!wrong.ok && !wrong.error.empty());

    // Stopping: the flag set from the start ends the solve without an answer.
    std::atomic<bool> stop{ true };
    const Solution stopped = cellstiff::solveOnGpu(gpu, solid, n, law, settings, cellstiff::Progress(), cellstiff::FirstPass(), &stop);
    QVERIFY(stopped.cancelled && !stopped.ok);
}


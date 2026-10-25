#include "test_ffthomog.h"
#include <QTest>
#include <cmath>
#include <vector>
#include "fft_homog.hpp"
#include "fft_solver_session.hpp"
#include "parameters.h"
#include "commandline_parser.h"

using ffth::Mat6;
using ffth::Vec6;

namespace {

// Relative Frobenius distance between two Mandel stiffness matrices.
double relDiff(const Mat6& A, const Mat6& B)
{
    double num = 0.0, den = 0.0;
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) {
            num += (A[i][j] - B[i][j]) * (A[i][j] - B[i][j]);
            den += B[i][j] * B[i][j];
        }
    return std::sqrt(num / den);
}

// Phase field: 1 inside a centred sphere of the given radius (in voxels), 0 outside.
std::vector<int> sphereInclusion(int n, double radius)
{
    std::vector<int> ph(static_cast<size_t>(n) * n * n, 0);
    const double c = 0.5 * n - 0.5;
    for (int z = 0; z < n; ++z)
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                const double d2 = (x - c) * (x - c) + (y - c) * (y - c) + (z - c) * (z - c);
                ph[(static_cast<size_t>(z) * n + y) * n + x] = d2 <= radius * radius ? 1 : 0;
            }
    return ph;
}

} // namespace

void TestFftHomog::init()
{
    Parameters::resetDefaults();
}

void TestFftHomog::testHomogeneousCellReturnsItsMaterial()
{
    const int n = 8;
    const Mat6 C = ffth::isotropic_C(2.0e9, 0.38);

    for (ffth::FFTHomogenizer::Scheme sch : { ffth::FFTHomogenizer::Scheme::Basic,
                                              ffth::FFTHomogenizer::Scheme::CG }) {
        ffth::FFTHomogenizer H(n, n, n);
        H.set_materials({ C });
        H.set_phase_field(std::vector<int>(static_cast<size_t>(n) * n * n, 0));
        H.set_tolerance(1e-8);
        H.set_max_iterations(100);
        H.set_scheme(sch);

        int iters = 0;
        const Mat6 Ceff = H.effective_stiffness(&iters);
        QVERIFY2(relDiff(Ceff, C) < 1e-9, "a homogeneous cell must return its own stiffness");
        // Nothing to iterate on: the uniform strain already satisfies equilibrium.
        QVERIFY(iters <= 6);
    }
}

void TestFftHomog::testCgMatchesBasicAtModerateContrast()
{
    // Stiff sphere (E = 10) in a soft matrix (E = 1): both schemes converge
    // to the same fixed point, so the effective stiffnesses must agree.
    const int n = 10;
    ffth::FFTHomogenizer basic(n, n, n), cg(n, n, n);
    const std::vector<Mat6> mats = { ffth::isotropic_C(1.0e9, 0.3), ffth::isotropic_C(10.0e9, 0.25) };
    const std::vector<int> ph = sphereInclusion(n, 3.5);

    for (ffth::FFTHomogenizer* H : { &basic, &cg }) {
        H->set_materials(mats);
        H->set_phase_field(ph);
        H->set_tolerance(1e-7);
        H->set_max_iterations(3000);
    }
    basic.set_scheme(ffth::FFTHomogenizer::Scheme::Basic);
    cg.set_scheme(ffth::FFTHomogenizer::Scheme::CG);

    int itB = 0, itC = 0;
    const Mat6 Cb = basic.effective_stiffness(&itB);
    const Mat6 Cc = cg.effective_stiffness(&itC);

    QVERIFY2(relDiff(Cc, Cb) < 1e-4,
             qPrintable(QString("CG vs basic differ by %1").arg(relDiff(Cc, Cb))));
    // The effective stiffness of a two-phase cell lies between the phases.
    QVERIFY(Cc[0][0] > mats[0][0][0] && Cc[0][0] < mats[1][0][0]);
    QVERIFY(itC > 0 && itB > 0);
}

void TestFftHomog::testCgConvergesOnSolidVoidLaminate()
{
    // Layers normal to X: half solid (E, nu), half "void" at 1e-4 E. Under
    // uniaxial stress along Y the solid layers are in plain uniaxial tension,
    // so E_y,eff = f E exactly; along X the stack is void-dominated.
    const int n = 16;
    const double E = 2.0e9, nu = 0.38, f = 0.5;
    ffth::FFTHomogenizer H(n, n, n);
    H.set_materials({ ffth::isotropic_C(1e-4 * E, 0.3), ffth::isotropic_C(E, nu) });
    std::vector<int> ph(static_cast<size_t>(n) * n * n, 0);
    for (int z = 0; z < n; ++z)
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
                ph[(static_cast<size_t>(z) * n + y) * n + x] = (x < n * f) ? 1 : 0;
    H.set_phase_field(ph);
    H.set_tolerance(1e-7);
    H.set_max_iterations(2000);
    H.set_scheme(ffth::FFTHomogenizer::Scheme::CG);

    int iters = 0;
    const Mat6 C = H.effective_stiffness(&iters);
    Mat6 S{};
    QVERIFY(fftsa::invert6x6(C, S));
    const double Ey = 1.0 / S[1][1];
    const double Ex = 1.0 / S[0][0];

    QVERIFY2(std::abs(Ey - f * E) / (f * E) < 0.01,
             qPrintable(QString("E_y,eff = %1, expected %2").arg(Ey).arg(f * E)));
    QVERIFY2(Ex < 0.01 * E, qPrintable(QString("E_x,eff = %1 should be void-dominated").arg(Ex)));
    // A 1e4 contrast would take the basic scheme thousands of iterations per
    // load case; CG must do all six in a few hundred.
    QVERIFY2(iters < 6 * 200, qPrintable(QString("%1 CG iterations for 6 loads").arg(iters)));
}

void TestFftHomog::testSessionAutoPicksScheme()
{
    const int n = 4;
    const std::vector<std::array<double,3>> orient = { {0,0,0}, {0,0,0} };

    // Dense single grain: auto stays with the basic scheme.
    {
        fftsa::FFTSolverSession dense(n, n, n, std::vector<int>(static_cast<size_t>(n) * n * n, 1), orient);
        QCOMPARE(QString(dense.scheme_name()), QString("basic"));
        dense.set_scheme(fftsa::FFTSolverSession::Scheme::CG);
        QCOMPARE(QString(dense.scheme_name()), QString("cg"));
    }
    // One void voxel: auto switches to CG; an explicit request still wins.
    {
        std::vector<int> g(static_cast<size_t>(n) * n * n, 1);
        g[0] = 0;
        fftsa::FFTSolverSession porous(n, n, n, g, orient);
        QVERIFY(porous.solid_fraction() < 1.0);
        QCOMPARE(QString(porous.scheme_name()), QString("cg"));
        porous.set_scheme(fftsa::FFTSolverSession::Scheme::Basic);
        QCOMPARE(QString(porous.scheme_name()), QString("basic"));
    }
}

void TestFftHomog::testCliOption()
{
    Parameters* p = Parameters::instance();
    QString err;
    QCOMPARE(p->getFftScheme(), QString("auto"));
    QVERIFY(applyParameter("fft_scheme", "CG", &err));
    QCOMPARE(p->getFftScheme(), QString("cg"));
    QVERIFY(applyParameter("fft_scheme", "basic", &err));
    QCOMPARE(p->getFftScheme(), QString("basic"));
    QVERIFY(applyParameter("fft_tol", "1e-4", &err));
    QCOMPARE(p->getFftTol(), 1e-4);
    QVERIFY(applyParameter("fft_max_iter", "250", &err));
    QCOMPARE(p->getFftMaxIter(), 250);
    QVERIFY(!applyParameter("fft_tol", "2", &err));
    QVERIFY(!applyParameter("fft_max_iter", "0", &err));
    QVERIFY(!applyParameter("fft_scheme", "jacobi", &err));
    QVERIFY(!err.isEmpty());
    QCOMPARE(p->getFftScheme(), QString("basic"));
    QVERIFY(Commandline_Parser::isValidFftScheme("auto"));
    QVERIFY(!Commandline_Parser::isValidFftScheme(""));
}


void TestFftHomog::testSensitivityIndex()
{
    // Strain concentration <|eps|^2>_solid / |<eps>_solid|^2 and the solid
    // strain ratio <|eps|^2>_solid / |E|^2. A homogeneous solid deforms
    // uniformly: both are exactly 1 for every unit load. In a solid/void
    // laminate (layers normal to X) each solid layer still deforms uniformly,
    // so the concentration stays 1, while the ratio tells how much of the
    // cell's strain the material carries: above 1 for e_yy (in-plane strain
    // plus free contraction across the layers), near 0 for e_xx (the void
    // layers take it). A block with a cubic hole strains unevenly around the
    // hole, so its concentration index exceeds 1.
    const int n = 8;
    const double E = 2.0e9, nu = 0.38;
    const double mu = E / (2 * (1 + nu)), lam = E * nu / ((1 + nu) * (1 - 2 * nu));
    const std::vector<std::array<double,3>> orient = { {0,0,0}, {0,0,0} };
    using fftsa::FFTSolverSession;

    {
        std::vector<int> grain(static_cast<size_t>(n) * n * n, 1);
        FFTSolverSession s(n, n, n, grain, orient, lam + 2 * mu, lam, mu);
        s.set_tolerance(1e-8);
        for (int j = 0; j < 6; ++j) {
            fftsa::Vec6 e{}; e.fill(0.0); e[j] = 1.0;
            std::vector<fftsa::Vec6> eng;
            s.solveLoadCaseFull(e, eng);
            const double idx = FFTSolverSession::strain_concentration_index(eng);
            const double ratio = FFTSolverSession::solid_strain_ratio(eng, e);
            QVERIFY2(std::abs(idx - 1.0) < 1e-6, qPrintable(QString("solid cube, load %1: index %2").arg(j).arg(idx)));
            QVERIFY2(std::abs(ratio - 1.0) < 1e-6, qPrintable(QString("solid cube, load %1: ratio %2").arg(j).arg(ratio)));
        }
    }
    {
        std::vector<int> grain(static_cast<size_t>(n) * n * n, 0);
        for (int z = 0; z < n; ++z)
            for (int y = 0; y < n; ++y)
                for (int x = 0; x < n; ++x)
                    grain[(static_cast<size_t>(z) * n + y) * n + x] = (x < n / 2) ? 1 : 0;
        FFTSolverSession s(n, n, n, grain, orient, lam + 2 * mu, lam, mu);
        s.set_tolerance(1e-8);
        s.set_max_iters(2000);
        s.set_scheme(FFTSolverSession::Scheme::CG);
        fftsa::Vec6 eyy{}; eyy.fill(0.0); eyy[1] = 1.0;
        fftsa::Vec6 exx{}; exx.fill(0.0); exx[0] = 1.0;
        std::vector<fftsa::Vec6> eng;
        s.solveLoadCaseFull(eyy, eng);
        const double idx_yy = FFTSolverSession::strain_concentration_index(eng);
        const double in_plane = FFTSolverSession::solid_strain_ratio(eng, eyy);
        s.solveLoadCaseFull(exx, eng);
        const double idx_xx = FFTSolverSession::strain_concentration_index(eng);
        const double across = FFTSolverSession::solid_strain_ratio(eng, exx);
        QVERIFY2(std::abs(idx_yy - 1.0) < 1e-3, qPrintable(QString("laminate e_yy concentration %1").arg(idx_yy)));
        QVERIFY2(std::abs(idx_xx - 1.0) < 1e-3, qPrintable(QString("laminate e_xx concentration %1").arg(idx_xx)));
        QVERIFY2(in_plane > 1.0 && in_plane < 2.0, qPrintable(QString("laminate in-plane ratio %1").arg(in_plane)));
        QVERIFY2(across < 0.1, qPrintable(QString("laminate across-layer ratio %1").arg(across)));
    }
    {
        const int m = 16;
        std::vector<int> grain(static_cast<size_t>(m) * m * m, 1);
        for (int z = 4; z < 12; ++z)
            for (int y = 4; y < 12; ++y)
                for (int x = 4; x < 12; ++x)
                    grain[(static_cast<size_t>(z) * m + y) * m + x] = 0;   // cubic hole
        FFTSolverSession s(m, m, m, grain, orient, lam + 2 * mu, lam, mu);
        s.set_tolerance(1e-7);
        s.set_max_iters(2000);
        s.set_scheme(FFTSolverSession::Scheme::CG);
        fftsa::Vec6 exx{}; exx.fill(0.0); exx[0] = 1.0;
        std::vector<fftsa::Vec6> eng;
        s.solveLoadCaseFull(exx, eng);
        const double idx = FFTSolverSession::strain_concentration_index(eng);
        QVERIFY2(idx > 1.05 && idx < 1e3, qPrintable(QString("block with a hole: concentration %1").arg(idx)));
    }
}

// ============================================================================
//  tensormath_selftest.hpp  -  Known-answer tests for tensormath.hpp.
//
//  Not compiled out: it is a normal inline function, run only when the
//  environment variable MATVIZ_TENSOR_SELFTEST=1 is set (see main.cpp).  It is
//  Qt-free and GL-free, so it also runs under --nogui.
//
//      MATVIZ_TENSOR_SELFTEST=1 MatViz3D --nogui --autostart
//
//  The checks that matter most, and why they exist:
//
//   * Jacobi is exercised on DELIBERATELY DEGENERATE matrices (two equal
//     eigenvalues, all three equal, a zero eigenvalue, all-negative).  Those are
//     the cases an analytic eigensolver silently destroys, and they are the
//     cases the glyph and streamline code actually hits.
//
//   * The superquadric ROTATIONAL SYMMETRY check pins down the alpha/beta
//     branch assignment.  A cl == cp crossover-continuity check alone does not:
//     both the correct assignment and the swapped one give alpha == beta at the
//     crossover and pass it.  Rotational symmetry in the degenerate limit is a
//     correctness requirement -- when lambda2 == lambda3 the eigenvectors e2/e3
//     are arbitrary in their plane, so a non-circular cross section would draw
//     an orientation the tensor does not determine.
//
//   * The elastic checks compare the numeric contraction against the CLOSED
//     FORM for a cubic crystal, so they pin the basis conversions to ~1e-9
//     rather than to a remembered literature value.
// ============================================================================
#pragma once

#include "tensormath.hpp"

#include <cstdio>
#include <random>
#include <string>

namespace mvt {

namespace selftest_detail {

struct Ctx {
    int failures = 0;
    int checks   = 0;

    void check(bool ok, const char* what, double got = 0.0, double want = 0.0)
    {
        ++checks;
        if (ok) return;
        ++failures;
        std::fprintf(stderr, "[tensormath selftest] FAIL: %s   (got %.12g, want %.12g)\n",
                     what, got, want);
    }
    void near(double got, double want, double tol, const char* what)
    {
        check(std::abs(got - want) <= tol, what, got, want);
    }
};

inline Sym3 fromMatrix(const double m[3][3])
{
    return { m[0][0], m[1][1], m[2][2], m[0][1], m[1][2], m[0][2] };
}

/// Rebuild the full 3x3 from an eigen-decomposition: sum_k lambda_k e_k (x) e_k.
inline void reconstruct(const Eig3& E, double out[3][3])
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += E.lambda[k] * E.e[k][i] * E.e[k][j];
            out[i][j] = s;
        }
}

inline void checkEigenOf(Ctx& c, const Sym3& A, const char* label)
{
    const Eig3 E = eigenSym3(A);

    const double src[3][3] = { { A.xx, A.xy, A.xz },
                               { A.xy, A.yy, A.yz },
                               { A.xz, A.yz, A.zz } };
    double scale = 0.0;
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) scale = std::max(scale, std::abs(src[i][j]));
    if (scale < 1e-300) scale = 1.0;

    double rec[3][3];
    reconstruct(E, rec);
    double maxErr = 0.0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            maxErr = std::max(maxErr, std::abs(rec[i][j] - src[i][j]));
    c.check(maxErr <= 1e-12 * scale, (std::string("eigen reconstruction: ") + label).c_str(),
            maxErr / scale, 0.0);

    // Orthonormality.
    for (int a = 0; a < 3; ++a) {
        c.near(norm(E.e[a]), 1.0, 1e-12, (std::string("eigenvector unit length: ") + label).c_str());
        for (int b = a + 1; b < 3; ++b)
            c.near(dot(E.e[a], E.e[b]), 0.0, 1e-12,
                   (std::string("eigenvector orthogonality: ") + label).c_str());
    }

    // Right-handed.
    c.near(dot(cross(E.e[0], E.e[1]), E.e[2]), 1.0, 1e-12,
           (std::string("eigenframe right-handed: ") + label).c_str());

    // Algebraically descending.
    c.check(E.lambda[0] >= E.lambda[1] - 1e-12 * scale &&
            E.lambda[1] >= E.lambda[2] - 1e-12 * scale,
            (std::string("eigenvalues descending: ") + label).c_str());
}

inline void testEigen(Ctx& c)
{
    std::mt19937 rng(20260814u);
    std::uniform_real_distribution<double> u(-1.0, 1.0);

    for (int t = 0; t < 10000; ++t) {
        Sym3 A{ u(rng), u(rng), u(rng), u(rng), u(rng), u(rng) };
        checkEigenOf(c, A, "random");
    }

    // Degenerate and pathological cases: exactly what breaks an analytic solver.
    checkEigenOf(c, Sym3{ 0, 0, 0, 0, 0, 0 },             "zero tensor");
    checkEigenOf(c, Sym3{ 5, 5, 5, 0, 0, 0 },             "isotropic (triple root)");
    checkEigenOf(c, Sym3{ 5, 5, 1, 0, 0, 0 },             "double root lambda1==lambda2");
    checkEigenOf(c, Sym3{ 9, 1, 1, 0, 0, 0 },             "double root lambda2==lambda3");
    checkEigenOf(c, Sym3{ 3, 3, 0, 0, 0, 0 },             "zero eigenvalue");
    checkEigenOf(c, Sym3{ -2, -5, -9, 0, 0, 0 },          "all negative");
    checkEigenOf(c, Sym3{ 1, -1, 0, 0, 0, 0 },            "traceless (deviatoric)");
    checkEigenOf(c, Sym3{ 1, 1, 1, 1e-14, 1e-14, 1e-14 }, "near-triple root");
    checkEigenOf(c, Sym3{ 1e8, 1e8, -1e8, 1e-6, 0, 0 },   "wide dynamic range");
    checkEigenOf(c, Sym3{ 0, 0, 0, 1, 1, 1 },             "pure shear");

    // Pure shear has a known spectrum: eigenvalues of [[0,1,1],[1,0,1],[1,1,0]]
    // are {2, -1, -1}.
    {
        const Eig3 E = eigenSym3(Sym3{ 0, 0, 0, 1, 1, 1 });
        c.near(E.lambda[0],  2.0, 1e-12, "pure shear lambda1");
        c.near(E.lambda[1], -1.0, 1e-12, "pure shear lambda2");
        c.near(E.lambda[2], -1.0, 1e-12, "pure shear lambda3");
    }

    // Magnitude ordering must be independent of algebraic ordering.
    {
        const Eig3 E = eigenSym3(Sym3{ 1.0, 0.1, -5.0, 0, 0, 0 });
        double m[3]; int order[3];
        sortedMagnitudes(E, m, order);
        c.near(m[0], 5.0, 1e-12, "sortedMagnitudes m1");
        c.near(m[1], 1.0, 1e-12, "sortedMagnitudes m2");
        c.near(m[2], 0.1, 1e-12, "sortedMagnitudes m3");
        c.near(E.lambda[order[0]], -5.0, 1e-12, "sortedMagnitudes keeps signed pairing");
    }

    // Westin coordinates must sum to 1 and stay finite for a traceless tensor --
    // the case that a trace-denominator formulation divides by zero on.
    {
        const Eig3 E = eigenSym3(Sym3{ 4.0, -1.0, -3.0, 0.5, -0.25, 0.75 });
        double m[3]; int order[3];
        sortedMagnitudes(E, m, order);
        double cl, cp, cs;
        westin(m[0], m[1], m[2], cl, cp, cs);
        c.near(cl + cp + cs, 1.0, 1e-12, "westin partition of unity (deviatoric)");
        c.check(cl >= -1e-12 && cp >= -1e-12 && cs >= -1e-12, "westin non-negative");
    }
}

inline void testSuperquadric(Ctx& c)
{
    const double gamma = 3.0;

    // 1. Isotropic limit -> unit sphere, on both branches.
    {
        const SqShape s = kindlmann(0.0, 0.0, gamma);
        c.near(s.alpha, 1.0, 1e-12, "isotropic alpha == 1");
        c.near(s.beta,  1.0, 1e-12, "isotropic beta == 1");
        for (double th = 0.15; th < 3.0; th += 0.31)
            for (double ph = 0.13; ph < 6.2; ph += 0.47)
                c.near(norm(superquadricPoint(s, th, ph)), 1.0, 1e-12, "isotropic -> unit sphere");
    }

    // Out-of-roundness of the cross section about the given axis, as a fraction
    // of its radius, maximized over a sweep of theta.
    auto outOfRoundness = [](const SqShape& s, int axis) {
        double worst = 0.0;
        for (double th = 0.35; th < 2.9; th += 0.37) {
            double rMin = 1e300, rMax = -1e300;
            for (double ph = 0.0; ph < 6.283; ph += 0.05) {
                const Vec3 p = superquadricPoint(s, th, ph);
                const double r = (axis == 0) ? std::sqrt(p[1]*p[1] + p[2]*p[2])
                                             : std::sqrt(p[0]*p[0] + p[1]*p[1]);
                rMin = std::min(rMin, r); rMax = std::max(rMax, r);
            }
            if (rMax > 1e-12) worst = std::max(worst, (rMax - rMin) / rMax);
        }
        return worst;
    };

    // 2. Linear limit: rotationally symmetric about the x axis.
    //    THE test that pins the alpha/beta assignment -- see the header comment.
    //    Circularity is exact only at cp == 0 (there beta == 1 exactly); nearby
    //    it must merely be close, so the two regimes are checked separately.
    {
        const SqShape s = kindlmann(1.0, 0.0, gamma);
        c.check(s.xAxis, "linear limit uses the x-axis branch");
        c.near(s.beta, 1.0, 1e-12, "linear limit: cross-section exponent beta == 1");
        c.check(outOfRoundness(s, 0) <= 1e-12,
                "linear limit: cross-section perpendicular to e1 is exactly circular",
                outOfRoundness(s, 0), 0.0);

        // Near the limit it should be nearly circular, and converge as cp -> 0.
        const double near1 = outOfRoundness(kindlmann(0.98, 0.01, gamma), 0);
        const double near2 = outOfRoundness(kindlmann(0.998, 0.001, gamma), 0);
        c.check(near1 < 0.02, "near-linear: cross-section within 2% of circular", near1, 0.0);
        c.check(near2 < near1, "cross-section roundness converges as cp -> 0", near2, near1);
    }

    // 3. Planar limit: rotationally symmetric about the z axis.
    {
        const SqShape s = kindlmann(0.0, 1.0, gamma);
        c.check(!s.xAxis, "planar limit uses the z-axis branch");
        c.near(s.beta, 1.0, 1e-12, "planar limit: cross-section exponent beta == 1");
        c.check(outOfRoundness(s, 2) <= 1e-12,
                "planar limit: cross-section perpendicular to e3 is exactly circular",
                outOfRoundness(s, 2), 0.0);

        const double near1 = outOfRoundness(kindlmann(0.01, 0.98, gamma), 2);
        const double near2 = outOfRoundness(kindlmann(0.001, 0.998, gamma), 2);
        c.check(near1 < 0.02, "near-planar: cross-section within 2% of circular", near1, 0.0);
        c.check(near2 < near1, "cross-section roundness converges as cl -> 0", near2, near1);
    }

    // 3b. Discrimination guard: if alpha/beta were swapped, the limit glyph
    //     would have a strongly SQUARE cross section instead.  Confirm the
    //     swapped shape really is far from round, so test 2/3 above have teeth.
    {
        SqShape swapped;
        swapped.alpha = std::pow(1.0 - 0.0, gamma);      // what the swap would give
        swapped.beta  = std::clamp(std::pow(1.0 - 1.0, gamma), kSqExponentMin, 1.0);
        swapped.xAxis = true;
        c.check(outOfRoundness(swapped, 0) > 0.25,
                "swapped exponents would be visibly non-circular (test has teeth)",
                outOfRoundness(swapped, 0), 0.25);
    }

    // 4. Crossover continuity: at cl == cp both branches must trace the same
    //    surface.  With alpha == beta the superquadric is octahedrally
    //    symmetric, so the x-form and z-form differ only by a relabeling.
    {
        const double cl = 0.37, cp = 0.37;
        SqShape sx = kindlmann(cl, cp, gamma);          // takes the cl >= cp branch
        SqShape sz = sx; sz.xAxis = false;              // same exponents, other form
        c.near(sx.alpha, sx.beta, 1e-12, "crossover alpha == beta");

        double maxDiff = 0.0;
        for (double th = 0.05; th < 3.14; th += 0.07)
            for (double ph = 0.03; ph < 6.28; ph += 0.09) {
                const Vec3 a = superquadricPoint(sx, th, ph);
                const Vec3 b = superquadricPoint(sz, th, ph);
                // Both lie on the same implicit surface; compare the implicit
                // residual rather than point-for-point (the parameterizations
                // sweep it differently).
                const double ea = 2.0 / sx.alpha;
                auto residual = [ea](const Vec3& p) {
                    return std::pow(std::abs(p[0]), ea) + std::pow(std::abs(p[1]), ea)
                         + std::pow(std::abs(p[2]), ea);
                };
                maxDiff = std::max(maxDiff, std::abs(residual(a) - residual(b)));
            }
        c.check(maxDiff <= 1e-9, "crossover: both branches trace one surface", maxDiff, 0.0);
    }

    // 5. Analytic normals really are normal to the surface.  Checked against
    //    central differences of the parameterization, away from the poles and
    //    the phi seams where sgnpow has kinks for exponents < 1.
    {
        const SqShape shapes[3] = { kindlmann(0.70, 0.10, gamma),
                                    kindlmann(0.10, 0.70, gamma),
                                    kindlmann(0.30, 0.25, gamma) };
        const double thetas[3] = { 0.7, 1.1, 2.0 };
        const double phis[4]   = { 0.4, 1.9, 3.4, 5.1 };
        const double h = 1e-6;

        for (const SqShape& s : shapes)
            for (double th : thetas)
                for (double ph : phis) {
                    const Vec3 n = superquadricNormal(s, th, ph);
                    const Vec3 pT0 = superquadricPoint(s, th - h, ph);
                    const Vec3 pT1 = superquadricPoint(s, th + h, ph);
                    const Vec3 pP0 = superquadricPoint(s, th, ph - h);
                    const Vec3 pP1 = superquadricPoint(s, th, ph + h);
                    const Vec3 dT = normalized({ pT1[0]-pT0[0], pT1[1]-pT0[1], pT1[2]-pT0[2] });
                    const Vec3 dP = normalized({ pP1[0]-pP0[0], pP1[1]-pP0[1], pP1[2]-pP0[2] });
                    c.near(dot(n, dT), 0.0, 1e-5, "analytic normal perp. to d/dtheta");
                    c.near(dot(n, dP), 0.0, 1e-5, "analytic normal perp. to d/dphi");
                }
    }

    // 6. Exponents stay inside the clamp, so the dual exponents (2-a) never go
    //    negative and pow(0,0) is unreachable.
    for (double cl = 0.0; cl <= 1.0001; cl += 0.05)
        for (double cp = 0.0; cp + cl <= 1.0001; cp += 0.05) {
            const SqShape s = kindlmann(cl, cp, gamma);
            c.check(s.alpha >= kSqExponentMin - 1e-15 && s.alpha <= 1.0 + 1e-15 &&
                    s.beta  >= kSqExponentMin - 1e-15 && s.beta  <= 1.0 + 1e-15,
                    "superquadric exponents within clamp");
        }
}

/// Fill a Voigt 6x6 for a cubic crystal.
inline void cubicVoigt(double c11, double c12, double c44, double out[6][6])
{
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) out[i][j] = 0.0;
    for (int i = 0; i < 3; ++i) {
        out[i][i] = c11;
        out[i + 3][i + 3] = c44;
        for (int j = 0; j < 3; ++j) if (i != j) out[i][j] = c12;
    }
}

inline void testElastic(Ctx& c)
{
    // Cu, from the seeded material_properties row (GPa).
    const double c11 = 168.40, c12 = 121.40, c44 = 75.40;

    double Cv[6][6];
    cubicVoigt(c11, c12, c44, Cv);

    const Mat6 Cm = voigtC_to_mandel(Cv);
    Mat6 Sm{};
    c.check(invertMat6(Cm, Sm), "cubic Mandel stiffness is invertible");

    // Mandel C and S must be true inverses.
    {
        double maxErr = 0.0;
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j) {
                double s = 0.0;
                for (int k = 0; k < 6; ++k) s += Cm[i][k] * Sm[k][j];
                maxErr = std::max(maxErr, std::abs(s - (i == j ? 1.0 : 0.0)));
            }
        c.check(maxErr < 1e-12, "Mandel C * S == I", maxErr, 0.0);
    }

    // detectCubic must recover the Voigt constants from the Mandel matrix.
    {
        double d11 = 0, d12 = 0, d44 = 0;
        c.check(detectCubic(Cm, d11, d12, d44), "detectCubic recognizes a cubic matrix");
        c.near(d11, c11, 1e-9, "detectCubic c11");
        c.near(d12, c12, 1e-9, "detectCubic c12");
        c.near(d44, c44, 1e-9, "detectCubic c44");
        c.near(zener(d11, d12, d44), 2.0 * c44 / (c11 - c12), 1e-12, "Zener ratio");
    }

    const C4 S4 = mandel_to_C4(Sm);
    const C4 C4s = mandel_to_C4(Cm);

    // Closed-form cubic compliance, for an exact cross-check of every basis
    // conversion in the chain (Voigt -> Mandel -> invert -> C4 -> contraction).
    const double den = (c11 - c12) * (c11 + 2.0 * c12);
    const double S11 = (c11 + c12) / den;
    const double S12 = -c12 / den;
    const double S44 = 1.0 / c44;
    const double anisoTerm = S11 - S12 - 0.5 * S44;

    auto Jof = [](const Vec3& n) {
        return n[0]*n[0]*n[1]*n[1] + n[1]*n[1]*n[2]*n[2] + n[2]*n[2]*n[0]*n[0];
    };

    const Vec3 dirs[5] = { {1,0,0}, {0,1,0},
                           normalized({1,1,0}), normalized({1,1,1}), normalized({2,1,3}) };
    for (const Vec3& n : dirs) {
        const double expected = 1.0 / (S11 - 2.0 * anisoTerm * Jof(n));
        c.near(youngsE(S4, n), expected, 1e-9 * std::abs(expected),
               "E(n) matches the closed-form cubic expression");
    }

    // Loose literature sanity, so a self-consistent-but-wrong chain still trips.
    {
        const double e100 = youngsE(S4, { 1, 0, 0 });
        const double e111 = youngsE(S4, normalized({ 1, 1, 1 }));
        c.check(e100 > 60.0 && e100 < 72.0,  "Cu E<100> ~ 67 GPa", e100, 67.0);
        c.check(e111 > 185.0 && e111 < 200.0, "Cu E<111> ~ 192 GPa", e111, 192.0);
        c.check(e111 / e100 > 2.7 && e111 / e100 < 3.0, "Cu E anisotropy ~ 2.9",
                e111 / e100, 2.87);
    }

    // Rotated stiffness component: C'_1111 = C11 - 2H * J(n), H = C11-C12-2C44.
    {
        const double H = c11 - c12 - 2.0 * c44;
        for (const Vec3& n : dirs) {
            const Mat3 g = frameAbout(n, 0.0);
            const double got = rotatedMandelComponent(C4s, g, 0, 0);
            const double expected = c11 - 2.0 * H * Jof(n);
            c.near(got, expected, 1e-9 * std::abs(expected), "rotated C'_1111 closed form");
        }
        // Along <100> it must be exactly C11.
        const Mat3 g = frameAbout({ 1, 0, 0 }, 0.0);
        c.near(rotatedMandelComponent(C4s, g, 0, 0), c11, 1e-9, "C'_1111 along <100> == C11");
        // C'_1111 involves n four times, so it cannot depend on the spin.
        const Vec3 nd = normalized({ 2, 1, 3 });
        const double a = rotatedMandelComponent(C4s, frameAbout(nd, 0.0), 0, 0);
        const double b = rotatedMandelComponent(C4s, frameAbout(nd, 0.9), 0, 0);
        c.near(a, b, 1e-9 * std::abs(a), "C'_1111 is spin-independent");
    }

    // Component SURFACES -- directionalComponent(), the quantity the material
    // database panel plots.  Regression guard for a real bug: the panel used to
    // evaluate C22 with axis 1 (not axis 2) aligned to the plotted direction,
    // so C22 measured stiffness along an arbitrary perpendicular instead. For
    // cubic Cu that put C11 at 237.6 GPa along <111> while C22 read 220.3, and
    // left a hard discontinuity where frameAbout() switches its up vector.
    {
        const double H = c11 - c12 - 2.0 * c44;

        for (const Vec3& n : dirs) {
            const double d11 = directionalComponent(C4s, n, 0.0, 0, 0);
            const double d22 = directionalComponent(C4s, n, 0.0, 1, 1);
            const double d33 = directionalComponent(C4s, n, 0.0, 2, 2);
            const double expected = c11 - 2.0 * H * Jof(n);
            c.near(d11, expected, 1e-9 * std::abs(expected), "C11 surface closed form");
            c.near(d22, d11, 1e-9 * std::abs(d11), "C22 surface == C11 surface");
            c.near(d33, d11, 1e-9 * std::abs(d11), "C33 surface == C11 surface");
        }

        // Voigt convention, so the numbers agree with the material table the
        // panel is drawn next to (Mandel would report 2 * C44 here).
        c.near(directionalComponent(C4s, { 1, 0, 0 }, 0.0, 3, 3), c44, 1e-9 * c44,
               "C44 surface along <100> == C44 in Voigt convention");
        c.near(directionalComponent(C4s, { 1, 0, 0 }, 0.0, 0, 1), c12, 1e-9 * c12,
               "C12 surface along <100> == C12");

        // Every component must reproduce its tabulated value along its own
        // pivot direction -- the property that makes the surface readable, and
        // the one an arbitrary tangent frame destroys for shear components.
        // Checked on an ORTHOTROPIC matrix, where C44, C55 and C66 genuinely
        // differ; on a cubic one the bug is invisible because they are equal.
        {
            double Vo[6][6] = {{0}};
            Vo[0][0] = 200; Vo[1][1] = 150; Vo[2][2] = 120;
            Vo[0][1] = Vo[1][0] = 60; Vo[0][2] = Vo[2][0] = 50; Vo[1][2] = Vo[2][1] = 40;
            Vo[3][3] = 50;  Vo[4][4] = 80;  Vo[5][5] = 30;
            const C4 O = mandel_to_C4(voigtC_to_mandel(Vo));

            const Vec3 ex{ 1, 0, 0 }, ey{ 0, 1, 0 }, ez{ 0, 0, 1 };
            c.near(directionalComponent(O, ex, 0.0, 0, 0), 200.0, 1e-9, "C11 at <100> == table C11");
            c.near(directionalComponent(O, ey, 0.0, 1, 1), 150.0, 1e-9, "C22 at <010> == table C22");
            c.near(directionalComponent(O, ez, 0.0, 2, 2), 120.0, 1e-9, "C33 at <001> == table C33");
            c.near(directionalComponent(O, ex, 0.0, 3, 3),  50.0, 1e-9, "C44 at <100> == table C44");
            c.near(directionalComponent(O, ey, 0.0, 4, 4),  80.0, 1e-9, "C55 at <010> == table C55");
            c.near(directionalComponent(O, ez, 0.0, 5, 5),  30.0, 1e-9, "C66 at <001> == table C66");

            // ... and the three shear surfaces must stay DISTINCT. They
            // collapsed onto one another while the frame came from an arbitrary
            // tangent, which is what made them look wrong.
            double spread = 0.0;
            for (const Vec3& n : dirs) {
                const double s4 = directionalComponent(O, n, 0.0, 3, 3);
                const double s5 = directionalComponent(O, n, 0.0, 4, 4);
                const double s6 = directionalComponent(O, n, 0.0, 5, 5);
                spread = std::max(spread, std::max(std::abs(s4 - s5), std::abs(s4 - s6)));
            }
            c.check(spread > 1.0, "C44/C55/C66 surfaces stay distinct for orthotropic",
                    spread, 27.0);

            // The normal diagonal still collapses, as it must.
            for (const Vec3& n : dirs) {
                const double d1 = directionalComponent(O, n, 0.0, 0, 0);
                c.near(directionalComponent(O, n, 0.0, 1, 1), d1, 1e-6 * std::abs(d1),
                       "orthotropic C22 surface == C11 surface");
                c.near(directionalComponent(O, n, 0.0, 2, 2), d1, 1e-6 * std::abs(d1),
                       "orthotropic C33 surface == C11 surface");
            }
        }

        // Cubic shear surfaces.
        //
        // C44, C55 and C66 are equal in the crystal frame, but their surfaces
        // are NOT pointwise equal: each is pivoted on a different crystal axis,
        // so the twist-free transport carries a different pair of in-plane axes
        // and they sample different in-plane orientations. They agree only
        // where the direction treats the relevant axes symmetrically. That is a
        // property of the frame convention, not of the material -- so the two
        // things worth pinning down are the physical bound, which every
        // convention must respect, and the twist ENVELOPE, which is
        // convention-free and therefore must collapse the three onto one.
        {
            const double lo = std::min(c44, 0.5 * (c11 - c12));
            const double hi = std::max(c44, 0.5 * (c11 - c12));

            auto envelope = [&](const Vec3& n, int idx, bool wantMax) {
                double best = wantMax ? -1e300 : 1e300;
                for (int k = 0; k < 180; ++k) {
                    const double v = directionalComponent(C4s, n, kPi * k / 180.0, idx, idx);
                    best = wantMax ? std::max(best, v) : std::min(best, v);
                }
                return best;
            };

            bool inBound = true;
            for (const Vec3& n : dirs) {
                for (int k = 0; k < 3; ++k) {
                    const double v = directionalComponent(C4s, n, 0.0, 3 + k, 3 + k);
                    if (v < lo - 1e-6 || v > hi + 1e-6) inBound = false;
                }
                // Convention-free: the largest and smallest shear stiffness
                // over all in-plane orientations cannot depend on which axis
                // pair the transport happened to start from.
                // Tolerance is set by the twist grid, not by the invariant:
                // the three sweeps start at different phases, so a 1 degree
                // step leaves an O(h^2) offset near the extremum (~2e-5
                // relative). A genuine failure of the invariant would be tens
                // of GPa, so this is still a sharp test.
                const double mx0 = envelope(n, 3, true),  mn0 = envelope(n, 3, false);
                c.near(envelope(n, 4, true),  mx0, 1e-3 * std::abs(mx0), "cubic C55 twist-max == C44 twist-max");
                c.near(envelope(n, 5, true),  mx0, 1e-3 * std::abs(mx0), "cubic C66 twist-max == C44 twist-max");
                c.near(envelope(n, 4, false), mn0, 1e-3 * std::abs(mn0), "cubic C55 twist-min == C44 twist-min");
                c.near(envelope(n, 5, false), mn0, 1e-3 * std::abs(mn0), "cubic C66 twist-min == C44 twist-min");
            }
            c.check(inBound, "cubic shear surfaces stay within [C', C44]");

            // <001> treats x and y symmetrically, so the surfaces pivoted on
            // them must agree there even at a fixed twist.
            const Vec3 ez{ 0, 0, 1 };
            c.near(directionalComponent(C4s, ez, 0.0, 4, 4),
                   directionalComponent(C4s, ez, 0.0, 3, 3), 1e-9,
                   "cubic C44 == C55 along <001>");
        }

        // Single-axis components must be continuous across the old seam at
        // |n_z| == 0.9, and genuinely inert to the spin control.
        auto atNz = [&](double nz) {
            const double r = std::sqrt(1.0 - nz * nz);
            const Vec3 n{ r * std::cos(0.7), r * std::sin(0.7), nz };
            return directionalComponent(C4s, n, 0.0, 1, 1);
        };
        c.near(atNz(0.9 + 1e-12), atNz(0.9 - 1e-12), 1e-6,
               "C22 surface has no seam where frameAbout() switches up vector");

        const Vec3 nd2 = normalized({ 2, 1, 3 });
        c.near(directionalComponent(C4s, nd2, 0.0,   1, 1),
               directionalComponent(C4s, nd2, 1.234, 1, 1), 1e-9,
               "C22 surface is spin-independent");

        c.check(componentIsSpinFree(0, 0) && componentIsSpinFree(1, 1) &&
                componentIsSpinFree(2, 2), "C11/C22/C33 classified spin-free");
        c.check(!componentIsSpinFree(0, 1) && !componentIsSpinFree(3, 3),
                "C12/C44 classified spin-dependent");

        // ---- CRYSTAL SYMMETRY: the check that catches frame artefacts -------
        //
        // A directional surface of a cubic crystal must be invariant under the
        // 24 rotations of the cube. Nothing else in this file would have caught
        // the defect this guards against: a fixed twist angle gave surfaces that
        // broke cubic symmetry by 100% of the range of the quantity -- a visible
        // funnel where the frame degenerates -- while still passing every
        // pointwise and closed-form check above.
        //
        // Averaging over the twist restores it exactly: the average commutes
        // with the symmetry operations, and a uniform grid integrates the
        // trigonometric polynomial in the twist without error.
        {
            // The 24 proper rotations, as signed axis permutations with det=+1.
            Mat3 group[24];
            int  nGroup = 0;
            const int perms[6][3] = { {0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0} };
            for (const auto& pm : perms) {
                for (int sgn = 0; sgn < 8; ++sgn) {
                    Mat3 m{};
                    const double s[3] = { (sgn&1)?-1.0:1.0, (sgn&2)?-1.0:1.0, (sgn&4)?-1.0:1.0 };
                    for (int i = 0; i < 3; ++i) m[i][pm[i]] = s[i];
                    const double det = m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1])
                                     - m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0])
                                     + m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0]);
                    if (det > 0.5 && nGroup < 24) group[nGroup++] = m;
                }
            }
            c.check(nGroup == 24, "cubic rotation group has 24 elements", nGroup, 24);

            // Mean over the twist, i.e. what the panel plots by default.
            auto meanOverTwist = [&](const Vec3& n, int a, int b) {
                const int N = 72;
                double sum = 0.0;
                for (int k = 0; k < N; ++k)
                    sum += directionalComponent(C4s, n, 2.0 * kPi * k / N, a, b);
                return sum / N;
            };

            const int probe[5][2] = { {0,0}, {0,1}, {3,3}, {4,4}, {5,5} };
            for (const auto& pr : probe) {
                double worst = 0.0;
                for (const Vec3& n : dirs) {
                    const double v0 = meanOverTwist(n, pr[0], pr[1]);
                    for (int g = 0; g < nGroup; ++g) {
                        Vec3 gn{ 0, 0, 0 };
                        for (int i = 0; i < 3; ++i)
                            for (int j = 0; j < 3; ++j) gn[i] += group[g][i][j] * n[j];
                        worst = std::max(worst, std::abs(meanOverTwist(gn, pr[0], pr[1]) - v0));
                    }
                }
                c.check(worst < 1e-6, "twist-mean component surface is cubic-symmetric",
                        worst, 0.0);
            }
        }
    }

    // An isotropic cubic crystal (Zener == 1) must give a perfectly spherical
    // E surface.  W is the seeded near-isotropic case; use exact isotropy here.
    {
        double Vi[6][6];
        const double i11 = 500.0, i12 = 200.0;
        cubicVoigt(i11, i12, 0.5 * (i11 - i12), Vi);      // Zener == 1 exactly
        Mat6 Ci = voigtC_to_mandel(Vi), Si{};
        c.check(invertMat6(Ci, Si), "isotropic stiffness invertible");
        const C4 Si4 = mandel_to_C4(Si);
        const double e0 = youngsE(Si4, { 1, 0, 0 });
        for (const Vec3& n : dirs)
            c.near(youngsE(Si4, n), e0, 1e-9 * e0, "Zener == 1 gives an isotropic E surface");
    }

    // Cu is auxetic: nu goes negative for some (n, m) near <110>.
    {
        double nuMin = 0, nuMax = 0, nuMean = 0;
        poissonNu(S4, normalized({ 1, 1, 0 }), 72, nuMin, nuMax, nuMean);
        c.check(nuMin < 0.0, "Cu nu_min along <110> is negative (auxetic)", nuMin, -0.1);
    }

    // G and nu must be reported as a range, not a single number, for an
    // anisotropic crystal -- i.e. the min/max sweep is actually doing something.
    {
        double gMin = 0, gMax = 0, gMean = 0;
        shearG(S4, normalized({ 1, 1, 0 }), 72, gMin, gMax, gMean);
        c.check(gMax - gMin > 1e-6 * gMax, "G varies with the perpendicular direction");
        c.check(gMean >= gMin && gMean <= gMax, "G mean lies within [min, max]");
    }

    // Pipeline <-> Mandel round trip.  Build a pipeline C by inverting
    // pipelineC_to_mandel's four cases, then convert back.
    {
        const double s2 = std::sqrt(2.0);
        double Cp[6][6] = {{0}};
        for (int a = 0; a < 6; ++a)
            for (int b = 0; b < 6; ++b) {
                const int pa = kMandelToPipeline[a], pb = kMandelToPipeline[b];
                if      (a < 3 && b < 3) Cp[pa][pb] = Cm[a][b];
                else if (a < 3)          Cp[pa][pb] = s2 * Cm[a][b];
                else if (b < 3)          Cp[pa][pb] = Cm[a][b] / s2;
                else                     Cp[pa][pb] = Cm[a][b];
            }

        const Mat6 back = pipelineC_to_mandel(Cp);
        double maxErr = 0.0, scale = 0.0;
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j) {
                maxErr = std::max(maxErr, std::abs(back[i][j] - Cm[i][j]));
                scale  = std::max(scale, std::abs(Cm[i][j]));
            }
        c.check(maxErr <= 1e-9 * scale, "pipeline <-> Mandel round trip", maxErr, 0.0);
    }

    // The factor-2 asymmetry of a pipeline-basis C is real and load-bearing.
    // Build one with non-zero normal<->shear coupling (a monoclinic-ish matrix)
    // and confirm Cp[i][j] == 2 * Cp[j][i] across the coupling block.
    {
        const double s2 = std::sqrt(2.0);
        double Vm[6][6];
        cubicVoigt(c11, c12, c44, Vm);
        Vm[0][5] = Vm[5][0] = 17.0;          // xx <-> xy coupling, Voigt
        Vm[1][5] = Vm[5][1] = -9.0;
        const Mat6 Mm = voigtC_to_mandel(Vm);

        double Cp[6][6] = {{0}};
        for (int a = 0; a < 6; ++a)
            for (int b = 0; b < 6; ++b) {
                const int pa = kMandelToPipeline[a], pb = kMandelToPipeline[b];
                if      (a < 3 && b < 3) Cp[pa][pb] = Mm[a][b];
                else if (a < 3)          Cp[pa][pb] = s2 * Mm[a][b];
                else if (b < 3)          Cp[pa][pb] = Mm[a][b] / s2;
                else                     Cp[pa][pb] = Mm[a][b];
            }

        bool sawCoupling = false;
        for (int i = 0; i < 3; ++i)
            for (int j = 3; j < 6; ++j) {
                if (std::abs(Cp[j][i]) < 1e-9) continue;
                sawCoupling = true;
                c.near(Cp[i][j], 2.0 * Cp[j][i], 1e-9 * std::abs(Cp[i][j]),
                       "pipeline C coupling block satisfies C[i][j] == 2*C[j][i]");
            }
        c.check(sawCoupling, "coupling test actually exercised a non-zero coupling term");
    }
}

} // namespace selftest_detail

/// Run every known-answer test.  Returns true when all pass.
inline bool selfTest()
{
    selftest_detail::Ctx c;
    selftest_detail::testEigen(c);
    selftest_detail::testSuperquadric(c);
    selftest_detail::testElastic(c);

    if (c.failures == 0)
        std::fprintf(stderr, "[tensormath selftest] PASS (%d checks)\n", c.checks);
    else
        std::fprintf(stderr, "[tensormath selftest] %d of %d checks FAILED\n",
                     c.failures, c.checks);
    return c.failures == 0;
}

} // namespace mvt

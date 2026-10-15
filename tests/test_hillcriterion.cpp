#include "test_hillcriterion.h"
#include <QTest>
#include <cmath>
#include <vector>
#include <array>
#include <random>
#include <QRegularExpression>
#include "hillcriterion.h"

void TestHillCriterion::init()
{
}

void TestHillCriterion::cleanup()
{
}

void TestHillCriterion::testDeviatoricConversions6Dto5D()
{
    HillCriterion hill;

    // Pure deviatoric stress (tr(sigma) = s[0] + s[1] + s[2] == 0)
    const double s[6] = { 120.0e6, -50.0e6, -70.0e6, 40.0e6, -30.0e6, 25.0e6 };

    double v[5] = {0};
    hill.sigma6D_to_v5D(s, v);

    double s_rec[6] = {0};
    hill.v5D_to_sigma6D(v, s_rec);

    const double tol = 1e-4; // Pa tolerance relative to 100 MPa
    for (int i = 0; i < 6; ++i) {
        QVERIFY2(std::abs(s[i] - s_rec[i]) < tol,
                 qPrintable(QString("Component %1 mismatch: original %2 vs reconstructed %3")
                                .arg(i).arg(s[i]).arg(s_rec[i])));
    }

    // Isometry check: |v|^2 == s1^2 + s2^2 + s3^2 + 2*(s4^2 + s5^2 + s6^2)
    double norm_v_sq = 0.0;
    for (int i = 0; i < 5; ++i) norm_v_sq += v[i] * v[i];

    double norm_s_sq = s[0]*s[0] + s[1]*s[1] + s[2]*s[2]
                     + 2.0 * (s[3]*s[3] + s[4]*s[4] + s[5]*s[5]);

    double rel_diff = std::abs(norm_v_sq - norm_s_sq) / norm_s_sq;
    QVERIFY2(rel_diff < 1e-12,
             qPrintable(QString("Norm mismatch in 5D projection: rel diff=%1")
                            .arg(rel_diff)));
}

void TestHillCriterion::testResolvedShearStress()
{
    HillCriterion hill;

    // Normal along Y, direction along Z -> shear in Y-Z plane
    const double n[3] = { 0.0, 1.0, 0.0 };
    const double b[3] = { 0.0, 0.0, 1.0 };

    double stress[3][3] = {
        { 0.0,     0.0,     0.0     },
        { 0.0,     0.0,     50.0e6  },
        { 0.0,     50.0e6,  0.0     }
    };

    // tau = b · sigma · n = b[2] * sigma[2][1] * n[1] = 1 * 50e6 * 1 = 50e6 Pa
    double tau = hill.resolvedShearStress(stress, n, b);

    const double eps = 1e-3;
    QVERIFY2(std::abs(tau - 50.0e6) < eps,
             qPrintable(QString("tau mismatch: got %1, expected 50e6").arg(tau)));
}

void TestHillCriterion::testFitEllipsoidKnownInput()
{
    HillCriterion hill;
    const double R = 100.0e6; // 100 MPa sphere in 5D
    const double expectedDiag = 1.0 / (R * R);

    // Generate 30 synthetic points on the 5D sphere of radius R
    // (covers all 5 diagonal and all 10 off-diagonal parameters)
    std::vector<std::array<double, 6>> yield_points;

    // 10 orthogonal axes: +/- along each of the 5 canonical dimensions
    for (int d = 0; d < 5; ++d) {
        for (double sgn : { 1.0, -1.0 }) {
            double v[5] = {0};
            v[d] = sgn * R;
            double s6[6] = {0};
            hill.v5D_to_sigma6D(v, s6);
            yield_points.push_back({ s6[0], s6[1], s6[2], s6[3], s6[4], s6[5] });
        }
    }

    // 20 off-diagonal points: all 10 pairs (i, j) with i < j
    for (int i = 0; i < 5; ++i) {
        for (int j = i + 1; j < 5; ++j) {
            for (double sgn : { 1.0, -1.0 }) {
                double v[5] = {0};
                v[i] = R / std::sqrt(2.0);
                v[j] = sgn * R / std::sqrt(2.0);
                double s6[6] = {0};
                hill.v5D_to_sigma6D(v, s6);
                yield_points.push_back({ s6[0], s6[1], s6[2], s6[3], s6[4], s6[5] });
            }
        }
    }

    bool success = hill.fit(yield_points);
    QVERIFY2(success, "fit() should succeed for well-conditioned 30 points");
    QVERIFY(hill.isValid());

    // Diagonal elements of P_5D should match expectedDiag = 1/R^2 (within Tikhonov reg tolerance 1e-3)
    for (int i = 0; i < 5; ++i) {
        double diagVal = hill.m_P_Hill_5D[i][i];
        double relErr = std::abs(diagVal - expectedDiag) / expectedDiag;
        QVERIFY2(relErr < 2e-3,
                 qPrintable(QString("P_5D[%1][%1] relative error too large: %2")
                                .arg(i).arg(relErr)));
    }

    // Off-diagonal elements of P_5D should be near 0
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            if (i == j) continue;
            double offVal = std::abs(hill.m_P_Hill_5D[i][j]);
            QVERIFY2(offVal / expectedDiag < 2e-3,
                     qPrintable(QString("P_5D[%1][%2] off-diagonal not zero: %3")
                                    .arg(i).arg(j).arg(offVal)));
        }
    }
}

void TestHillCriterion::testFitEllipsoidFewerThan15PointsRejection()
{
    HillCriterion hill;
    std::vector<std::array<double, 6>> points(10, {1e6, -5e5, -5e5, 0, 0, 0});

    // Hill fit requires at least 15 independent points to determine the 15 parameters
    bool ok = hill.fit(points);
    QCOMPARE(ok, false);
    QCOMPARE(hill.isValid(), false);
}

std::vector<std::array<double, 6>> TestHillCriterion::deviatoricSpherePoints(int n, double R, unsigned seed)
{
    HillCriterion hill;
    std::mt19937 gen(seed);
    std::normal_distribution<double> dist(0.0, 1.0);
    std::vector<std::array<double, 6>> pts;
    for (int k = 0; k < n; ++k) {
        double v[5], nn = 0.0;
        for (double& c : v) { c = dist(gen); nn += c * c; }
        nn = std::sqrt(nn);
        for (double& c : v) c *= R / nn;
        double s6[6] = {0};
        hill.v5D_to_sigma6D(v, s6);
        pts.push_back({ s6[0], s6[1], s6[2], s6[3], s6[4], s6[5] });
    }
    return pts;
}

void TestHillCriterion::testVonMisesUniaxialAndShear()
{
    const double sigma = 250.0e6;
    const double uni[6] = { sigma, 0, 0, 0, 0, 0 };
    QVERIFY(std::abs(HillCriterion::vonMisesStress(uni) - sigma) / sigma < 1e-14);

    const double uniz[6] = { 0, 0, -sigma, 0, 0, 0 };
    QVERIFY(std::abs(HillCriterion::vonMisesStress(uniz) - sigma) / sigma < 1e-14);

    // pure shear tau in each shear component -> sqrt(3)*tau
    const double tau = 80.0e6;
    for (int c = 3; c < 6; ++c) {
        double s[6] = {0};
        s[c] = tau;
        QVERIFY(std::abs(HillCriterion::vonMisesStress(s) - std::sqrt(3.0) * tau) / tau < 1e-14);
    }

    // hydrostatic stress has zero von Mises stress
    const double hydro[6] = { sigma, sigma, sigma, 0, 0, 0 };
    QVERIFY(HillCriterion::vonMisesStress(hydro) < 1e-6);
}

void TestHillCriterion::testFitFull6DRecoversKnownEllipsoid()
{
    // Known PD P0 = (A*A^T + 6*I) / R^2, A random: full coupling incl. the hydrostatic axis
    const double R = 100.0e6;
    std::mt19937 gen(42);
    std::normal_distribution<double> dist(0.0, 1.0);
    double A[6][6], P0[6][6];
    for (auto& row : A) for (double& a : row) a = dist(gen);
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) {
            double acc = (i == j) ? 6.0 : 0.0;
            for (int k = 0; k < 6; ++k) acc += A[i][k] * A[j][k];
            P0[i][j] = acc / (R * R);
        }

    // 200 points exactly on sigma^T P0 sigma = 1
    std::vector<std::array<double, 6>> pts;
    for (int n = 0; n < 200; ++n) {
        std::array<double, 6> d;
        for (double& c : d) c = dist(gen);
        double q = 0.0;
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j) q += d[i] * P0[i][j] * d[j];
        for (double& c : d) c /= std::sqrt(q);
        pts.push_back(d);
    }

    HillCriterion hill;
    hill.setYieldSettings("vonmises", "full6d", 120e6);
    QVERIFY(hill.fit(pts));
    QVERIFY(hill.isValid());
    QVERIFY(hill.m_fittedFull6D);

    double maxP0 = 0.0;
    for (auto& row : P0) for (double v : row) maxP0 = std::max(maxP0, std::abs(v));
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) {
            const double rel = std::abs(hill.m_P_Hill[i][j] - P0[i][j]) / maxP0;
            QVERIFY2(rel < 1e-6, qPrintable(QString("P[%1][%2] rel err %3").arg(i).arg(j).arg(rel)));
        }
}

void TestHillCriterion::testFitFull6DFallbackDeviatoric()
{
    // Purely deviatoric points: hydrostatic axis unconstrained -> 5D fallback
    HillCriterion hill;
    const double R = 100.0e6;
    auto pts = deviatoricSpherePoints(60, R, 7);

    hill.setYieldSettings("vonmises", "full6d", 120e6);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("falling back to deviatoric5d"));
    QVERIFY(hill.fit(pts));
    QVERIFY(hill.isValid());
    QVERIFY(!hill.m_fittedFull6D);

    // 5D path result: sphere of radius R
    const double expectedDiag = 1.0 / (R * R);
    for (int i = 0; i < 5; ++i)
        QVERIFY(std::abs(hill.m_P_Hill_5D[i][i] - expectedDiag) / expectedDiag < 2e-3);
}

void TestHillCriterion::testFitFull6DFallbackNearlyPressureInsensitive()
{
    // Deviatoric points plus small random hydrostatic noise: the data carry no
    // hydrostatic stiffness, PD would only be reachable by a forced diagonal
    // shift -> must fall back, not shift. At 1e-3*R the fitted hydrostatic
    // coefficient is round-off that can pass the PD/conditioning checks; the
    // data-support check (r_h vs. p_max) must catch it.
    const double R = 100.0e6;
    for (double level : { 1e-3, 1e-2 }) {
        for (unsigned seed : { 11u, 12u, 13u }) {
            HillCriterion hill;
            auto pts = deviatoricSpherePoints(60, R, seed);
            std::mt19937 gen(123 + seed);
            std::normal_distribution<double> noise(0.0, level * R);
            for (auto& p : pts) {
                const double h = noise(gen);
                p[0] += h; p[1] += h; p[2] += h;
            }

            hill.setYieldSettings("vonmises", "full6d", 120e6);
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression("falling back to deviatoric5d"));
            QVERIFY(hill.fit(pts));
            QVERIFY(hill.isValid());
            QVERIFY2(!hill.m_fittedFull6D,
                     qPrintable(QString("noise %1*R, seed %2: 6D fit accepted").arg(level).arg(seed)));
        }
    }
}

void TestHillCriterion::testGenerateLoadsDefaultEqualsExplicitSchmid5D()
{
    // Default-constructed (historical path) vs explicit schmid + deviatoric5d:
    // identical loads, bit for bit.
    double S[6][6] = {{0}};
    for (int i = 0; i < 6; ++i) S[i][i] = (i < 3) ? 1e-11 : 2e-11;
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) if (i != j) S[i][j] = -3e-12;

    HillCriterion a, b;
    auto pts = deviatoricSpherePoints(40, 100.0e6, 3);
    QVERIFY(a.fit(pts));
    b.setYieldSettings("schmid", "deviatoric5d", 120e6);
    QVERIFY(b.fit(pts));
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            QCOMPARE(a.m_P_Hill[i][j], b.m_P_Hill[i][j]);

    const auto la = a.generateLoads(100, 1e-3, S);
    const auto lb = b.generateLoads(100, 1e-3, S);
    QCOMPARE(la.size(), size_t(100));
    QCOMPARE(la, lb);

    // full6d samples carry a hydrostatic stress part; von Mises uses no Schmid edges
    HillCriterion c;
    c.setYieldSettings("vonmises", "full6d", 120e6);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("falling back to deviatoric5d"));
    QVERIFY(c.fit(pts)); // falls back to 5D, still valid for load generation
    const auto lc = c.generateLoads(100, 1e-3, S);
    QCOMPARE(lc.size(), size_t(100));
    QVERIFY(lc != la);
}

#include "test_hillcriterion.h"
#include <QTest>
#include <cmath>
#include <vector>
#include <array>
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

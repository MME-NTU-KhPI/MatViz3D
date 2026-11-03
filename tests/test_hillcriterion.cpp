#include "test_hillcriterion.h"
#include <QTest>
#include <cmath>
#include <vector>
#include <array>
#include <random>
#include <QRegularExpression>
#include "hillcriterion.h"
#include "ansyswrapper.h"
#include <QFile>
#include <QTextStream>
#include <QTemporaryDir>
#include "stressresult.h"
#include "fft_solver_session.hpp"
#include "tensormath.hpp"
#include "hdf5wrapper.h"
#include "parent_algorithm.h"
#include "parameters.h"
#include "commandline_parser.h"
#include "config_source.h"
#include <QCommandLineParser>
#include <QTemporaryFile>
#include <QDir>
#include <algorithm>

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

namespace {
// Exposes the protected temp dir so the test can drop synthetic result files
// where load_loadstep() reads them, without launching ANSYS.
struct ElasticFitWrapper : public ansysWrapper {
    ElasticFitWrapper() : ansysWrapper(true, false) {}
    QString filePath(const QString& name) const { return tempDir.filePath(name); }
};

// One fixed-width field as load_loadstep() slices it: 16 chars + separator.
QString field(double v) { return QString("%1;").arg(v, 16, 'g', 17); }
}

void TestHillCriterion::testElasticPropertiesEngineeringShearBasis()
{
    // Synthetic anisotropic compliance in the ENGINEERING shear basis
    // (eps_eng = [exx,eyy,ezz,gxy,gyz,gxz] = S_eng * sigma). Symmetric,
    // diagonally dominant (so positive definite), with normal-shear coupling.
    // Dyadic entries + integer stresses keep every value exact in float.
    const double S_eng[6][6] = {
        { 4.0,  -1.0,  -1.5,   0.5,   0.0,   0.0 },
        {-1.0,   4.0,  -1.0,   0.0,   0.25,  0.0 },
        {-1.5,  -1.0,   5.0,   0.0,   0.0,   0.0 },
        { 0.5,   0.0,   0.0,   6.0,   0.0,   0.5 },
        { 0.0,   0.25,  0.0,   0.0,   7.0,   0.0 },
        { 0.0,   0.0,   0.0,   0.5,   0.0,   8.0 },
    };

    ElasticFitWrapper wr;
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(-5, 5);
    const int nSteps = 30;

    for (int k = 0; k < nSteps; ++k) {
        double sigma[6];
        for (double& v : sigma) v = dist(rng);
        double eps_eng[6] = {0};
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j) eps_eng[i] += S_eng[i][j] * sigma[j];

        // The loading is stored the way the pipeline stores it: TENSOR shear.
        std::vector<float> load(6);
        for (int i = 0; i < 6; ++i)
            load[i] = float(i >= 3 ? eps_eng[i] / 2.0 : eps_eng[i]);
        wr.eps_as_loading.push_back(load);

        // Nodal table: ID;X;Y;Z;UX;UY;UZ;SX..SXZ;EpsX..EpsXZ (one node).
        QFile ls(wr.filePath(QString("ls_%1.csv").arg(k + 1)));
        QVERIFY(ls.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&ls);
        out << "header\n" << field(1);
        for (int c = 0; c < 6; ++c) out << field(0);
        for (double v : sigma) out << field(v);
        for (float v : load) out << field(v);
        out << "\n";
        ls.close();

        // Element table: SX..SXZ;EpsX..EpsXZ (the exact volume average).
        QFile lse(wr.filePath(QString("lse_%1.csv").arg(k + 1)));
        QVERIFY(lse.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream oute(&lse);
        oute << "header\n";
        for (double v : sigma) oute << field(v);
        for (float v : load) oute << field(v);
        oute << "\n";
        lse.close();
    }

    const auto res = wr.calculateElasticProperties();
    QVERIFY(res.isValid);

    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) {
            const double scale = std::max(std::abs(S_eng[i][j]), std::abs(S_eng[i][i]));
            const double relErr = std::abs(res.S[i][j] - S_eng[i][j]) / scale;
            if (relErr >= 1e-6)
                QFAIL(qPrintable(QString("S[%1][%2] = %3, expected %4 (rel err %5)")
                                     .arg(i).arg(j).arg(res.S[i][j]).arg(S_eng[i][j]).arg(relErr)));
            QVERIFY(std::abs(res.S[i][j] - res.S[j][i]) <= 1e-12 * scale);
        }
}

namespace {
// Engineering-basis compliance (pipeline order xx,yy,zz,xy,yz,xz) with
// normal<->shear coupling, in 1/Pa. Symmetric and diagonally dominant.
void engineeringComplianceWithCoupling(double S[6][6])
{
    const double base[6][6] = {
        { 4.0,  -1.0,  -1.5,   0.5,   0.0,   0.0 },
        {-1.0,   4.0,  -1.0,   0.0,   0.25,  0.0 },
        {-1.5,  -1.0,   5.0,   0.0,   0.0,   0.3 },
        { 0.5,   0.0,   0.0,   6.0,   0.0,   0.5 },
        { 0.0,   0.25,  0.0,   0.0,   7.0,   0.0 },
        { 0.0,   0.0,   0.3,   0.5,   0.0,   8.0 },
    };
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) S[i][j] = base[i][j] * 1e-12;
}
}

void TestHillCriterion::testSymmetrizeKeepsSymmetricEngineeringC()
{
    double S[6][6], C[6][6];
    engineeringComplianceWithCoupling(S);
    QVERIFY(invert6x6(S, C));
    for (int i = 0; i < 6; ++i)                    // exact symmetry
        for (int j = i + 1; j < 6; ++j) C[j][i] = C[i][j];
    QVERIFY(std::abs(C[0][3]) > 0.0);              // coupling really present

    double Cq[6][6];
    std::copy(&C[0][0], &C[0][0] + 36, &Cq[0][0]);
    symmetrizePipelineC(Cq);

    fftsa::Mat6 Cm{};
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) Cm[i][j] = C[i][j];
    fftsa::symmetrize_pipeline_C(Cm);

    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) {
            QCOMPARE(Cq[i][j], C[i][j]);
            QCOMPARE(Cm[i][j], C[i][j]);
        }
}

void TestHillCriterion::testStressToStrainLoadHalvesShear()
{
    double S[6][6];
    engineeringComplianceWithCoupling(S);
    const double sigma[6] = { 0.3, -0.7, 0.2, 0.5, -0.4, 0.6 };
    const double strain_val = 1e-3;

    double eps_eng[6] = {0};
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) eps_eng[i] += S[i][j] * sigma[j];
    double expected[6], n2 = 0;
    for (int i = 0; i < 6; ++i) {
        expected[i] = (i >= 3) ? eps_eng[i] / 2.0 : eps_eng[i];
        n2 += expected[i] * expected[i];
    }
    for (double& v : expected) v *= strain_val / std::sqrt(n2);

    double eps[6];
    QVERIFY(HillCriterion::stressToStrainLoad(S, sigma, strain_val, eps));
    for (int i = 0; i < 6; ++i)
        QVERIFY2(std::abs(eps[i] - expected[i]) <= 1e-12 * strain_val,
                 qPrintable(QString("eps[%1] = %2, expected %3").arg(i).arg(eps[i]).arg(expected[i])));
    // Shear entries are exactly half the engineering strain, on the same scale
    const double k = eps[0] / eps_eng[0];
    for (int i = 3; i < 6; ++i)
        QVERIFY(std::abs(eps[i] - k * eps_eng[i] / 2.0) <= 1e-12 * strain_val);
}

void TestHillCriterion::testGenerateLoadsUsesEngineeringCompliance()
{
    // von Mises + deviatoric5d: every load comes from a deviatoric stress
    // direction. Undoing the loading (D*eps -> sigma = C_eng*eps_eng) must
    // give back a traceless stress; skipping the shear halving would not,
    // because the compliance couples shear into the normal block.
    double S[6][6], C[6][6];
    engineeringComplianceWithCoupling(S);
    QVERIFY(invert6x6(S, C));

    HillCriterion h;
    h.setYieldSettings("vonmises", "deviatoric5d", 120e6);
    QVERIFY(h.fit(deviatoricSpherePoints(40, 100.0e6, 3)));
    const auto loads = h.generateLoads(50, 1e-3, S);
    QCOMPARE(loads.size(), size_t(50));

    for (const auto& eps : loads) {
        double eps_eng[6];
        for (int i = 0; i < 6; ++i) eps_eng[i] = (i >= 3) ? 2.0 * eps[i] : eps[i];
        double sig[6] = {0}, n2 = 0;
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) sig[i] += C[i][j] * eps_eng[j];
            n2 += sig[i] * sig[i];
        }
        const double tr = sig[0] + sig[1] + sig[2];
        QVERIFY2(std::abs(tr) <= 1e-9 * std::sqrt(n2),
                 qPrintable(QString("recovered stress not deviatoric: tr/|s| = %1")
                                .arg(tr / std::sqrt(n2))));
    }
}

void TestHillCriterion::testPipelineCToMandelIsotropic()
{
    const double E = 200e9, nu = 0.3;
    const double G = E / (2.0 * (1.0 + nu));
    double Cv[6][6];
    isotropicVoigt(E, nu, Cv);                     // engineering: C44 = G
    QVERIFY(std::abs(Cv[3][3] - G) <= 1e-9 * G);

    const mvt::Mat6 M = mvt::pipelineC_to_mandel(Cv);
    for (int a = 0; a < 3; ++a)
        QVERIFY(std::abs(M[a][a] - Cv[0][0]) <= 1e-9 * Cv[0][0]);
    for (int a = 3; a < 6; ++a)
        QVERIFY2(std::abs(M[a][a] - 2.0 * G) <= 1e-9 * G,
                 qPrintable(QString("Mandel[%1][%1] = %2, expected 2G = %3").arg(a).arg(M[a][a]).arg(2.0 * G)));
}

void TestHillCriterion::testHdf5LegacyTensorBasisConverted()
{
    double S_eng[6][6], C_eng[6][6];
    engineeringComplianceWithCoupling(S_eng);
    QVERIFY(invert6x6(S_eng, C_eng));

    // The same material as a legacy file would hold it: tensor shear rows.
    double S_ten[6][6], C_ten[6][6];
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) S_ten[i][j] = (i >= 3) ? S_eng[i][j] / 2.0 : S_eng[i][j];
    QVERIFY(invert6x6(S_ten, C_ten));

    auto toFloat = [](const double M[6][6]) {
        std::vector<std::vector<float>> out(6, std::vector<float>(6));
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j) out[i][j] = float(M[i][j]);
        return out;
    };

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::string newPath = dir.filePath("new.h5").toStdString();
    const std::string oldPath = dir.filePath("legacy.h5").toStdString();
    {
        HDF5Wrapper h(newPath);
        h.write("/1", "S_matrix", toFloat(S_eng));
        h.write("/1", "C_matrix", toFloat(C_eng));
        writeShearBasisAttribute(h, "/1");
    }
    {
        HDF5Wrapper h(oldPath);
        h.write("/1", "S_matrix", toFloat(S_ten));
        h.write("/1", "C_matrix", toFloat(C_ten));
    }

    double Sn[6][6], Cn[6][6], So[6][6], Co[6][6];
    {
        HDF5Wrapper h(newPath);
        QCOMPARE(h.readStringAttribute("/1", "S_matrix", "shear_basis"), QString("engineering"));
        QVERIFY(readElasticMatricesEngineering(h, "/1", Sn, Cn));
    }
    {
        HDF5Wrapper h(oldPath);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("no shear_basis attribute"));
        QVERIFY(readElasticMatricesEngineering(h, "/1", So, Co));
    }

    double scale = 0;
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) scale = std::max(scale, std::abs(C_eng[i][j]));
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) {
            QVERIFY(std::abs(Cn[i][j] - C_eng[i][j]) <= 1e-5 * scale);
            QVERIFY2(std::abs(Co[i][j] - Cn[i][j]) <= 1e-5 * scale,
                     qPrintable(QString("C[%1][%2]: legacy %3 vs new %4").arg(i).arg(j).arg(Co[i][j]).arg(Cn[i][j])));
        }
}

void TestHillCriterion::testFCCSchmidAndCRSSYieldField()
{
    // 1. Verify all 12 FCC slip systems have |n| == 1, |b| == 1, and n . b == 0
    QCOMPARE(int(mvh::kFCCSlipSystems.size()), 12);
    for (int s = 0; s < mvh::kNumFCCSlipSystems; ++s) {
        const auto& sys = mvh::kFCCSlipSystems[s];
        const double nLen = std::sqrt(sys.n[0]*sys.n[0] + sys.n[1]*sys.n[1] + sys.n[2]*sys.n[2]);
        const double bLen = std::sqrt(sys.b[0]*sys.b[0] + sys.b[1]*sys.b[1] + sys.b[2]*sys.b[2]);
        const double dot  = sys.n[0]*sys.b[0] + sys.n[1]*sys.b[1] + sys.n[2]*sys.b[2];
        QVERIFY(std::abs(nLen - 1.0) < 1e-12);
        QVERIFY(std::abs(bLen - 1.0) < 1e-12);
        QVERIFY(std::abs(dot) < 1e-12);
    }

    // 2. Uniaxial tension along [100] in unrotated FCC crystal: m_max = 1/sqrt(6) ≈ 0.40824829
    const double sigma0 = 50.0e6; // 50 MPa
    const double expectedM = 1.0 / std::sqrt(6.0);
    const double expectedTau = sigma0 * expectedM;
    const double s_uni[6] = { sigma0, 0.0, 0.0, 0.0, 0.0, 0.0 };

    mvh::YieldCriterionParams pModeA;
    pModeA.mode = mvh::YieldCriterionMode::ModeA_SchmidThreshold;
    pModeA.schmidThreshold = 0.40;
    pModeA.continuousOutput = false;

    const auto rA1 = mvh::evaluate_fcc_voxel(s_uni, 0.0, 0.0, 0.0, pModeA);
    QVERIFY(std::abs(rA1.m_max - float(expectedM)) < 1e-5f);
    QVERIFY(std::abs(rA1.tau_max - float(expectedTau)) < 1e2f);
    QCOMPARE(rA1.yield_eval, 1.0f); // 0.4082 > 0.40 -> yielding

    pModeA.schmidThreshold = 0.45;
    const auto rA2 = mvh::evaluate_fcc_voxel(s_uni, 0.0, 0.0, 0.0, pModeA);
    QCOMPARE(rA2.yield_eval, 0.0f); // 0.4082 < 0.45 -> elastic

    // 3. Mode B (Physical CRSS): binary and continuous ratio tau_max / tau_cr
    mvh::YieldCriterionParams pModeB;
    pModeB.mode = mvh::YieldCriterionMode::ModeB_PhysicalCRSS;
    pModeB.crss = 10.0e6; // 10 MPa < expectedTau (~20.41 MPa)
    pModeB.continuousOutput = false;
    const auto rB1 = mvh::evaluate_fcc_voxel(s_uni, 0.0, 0.0, 0.0, pModeB);
    QCOMPARE(rB1.yield_eval, 1.0f);

    pModeB.continuousOutput = true;
    const auto rB2 = mvh::evaluate_fcc_voxel(s_uni, 0.0, 0.0, 0.0, pModeB);
    QVERIFY(std::abs(rB2.yield_eval - float(expectedTau / 10.0e6)) < 1e-4f);

    // 4. OpenMP 3D voxel grid + populateFFTFieldSchmidAndYield (including void voxel id == 0)
    const int N = 2;
    int32_t*** vox = Parent_Algorithm::Create3D<int32_t>(N, N, N);
    for (int x = 0; x < N; ++x)
        for (int y = 0; y < N; ++y)
            for (int z = 0; z < N; ++z)
                vox[x][y][z] = 1;
    vox[0][0][0] = 0; // void voxel

    std::vector<std::vector<float>> local_cs = {
        { 0.0f, 0.0f, 0.0f }, // index 0 (void)
        { 0.0f, 0.0f, 0.0f }  // grain 1
    };

    FieldVisualizationData field;
    field.numCubes = N;
    field.componentValid[SX] = true;
    field.perVoxel[SX].assign(N * N * N, float(sigma0));
    for (int c : { SY, SZ, SXY, SYZ, SXZ }) {
        field.componentValid[c] = true;
        field.perVoxel[c].assign(N * N * N, 0.0f);
    }

    pModeB.continuousOutput = false;
    populateFFTFieldSchmidAndYield(field, vox, local_cs, pModeB);

    QVERIFY(field.componentValid[SCHMID_FACTOR]);
    QVERIFY(field.componentValid[YIELD_STATUS]);
    QCOMPARE(field.perVoxel[SCHMID_FACTOR][field.denseIndex(0, 0, 0)], 0.0f); // void skipped
    QCOMPARE(field.perVoxel[YIELD_STATUS][field.denseIndex(0, 0, 0)], 0.0f);
    QVERIFY(std::abs(field.perVoxel[SCHMID_FACTOR][field.denseIndex(1, 0, 0)] - float(expectedM)) < 1e-5f);
    QCOMPARE(field.perVoxel[YIELD_STATUS][field.denseIndex(1, 0, 0)], 1.0f);

    Parent_Algorithm::Delete3D<int32_t>(vox);
}


namespace {
// Reproducible, heterogeneous, nonzero local field (Pa); peak von Mises below 1e9.
std::vector<std::array<double, 6>> syntheticVoxelStress(int n)
{
    std::mt19937 gen(99);
    std::uniform_real_distribution<double> dist(-1.0e8, 1.0e8);
    std::vector<std::array<double, 6>> field(n);
    for (auto& s : field)
        for (double& c : s) c = dist(gen);
    return field;
}

std::array<double, 6> scaled(const std::array<double, 6>& s, double f)
{
    return { s[0]*f, s[1]*f, s[2]*f, s[3]*f, s[4]*f, s[5]*f };
}

// Peak local stress of the scaled field is at least 1e6 times below the old 10 Pa cutoff
const double kBelowOldThreshold = 1e-14;

// 16-char field, short enough for load_loadstep()'s fixed-width slicing at any magnitude
QString fixedField(double v) { return QString("%1;").arg(v, 16, 'e', 8); }

bool writeLoadStep(ElasticFitWrapper& wr, int ls, const std::vector<std::array<double, 6>>& field,
                   const std::array<double, 6>& macro)
{
    QFile f(wr.filePath(QString("ls_%1.csv").arg(ls)));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    out << "header\n";
    for (int e = 0; e < int(field.size()); ++e) {
        out << fixedField(e + 1);
        for (int c = 0; c < 6; ++c) out << fixedField(0);
        for (double v : field[e]) out << fixedField(v);
        for (int c = 0; c < 6; ++c) out << fixedField(0);
        out << "\n";
    }
    f.close();

    QFile fe(wr.filePath(QString("lse_%1.csv").arg(ls)));
    if (!fe.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream oute(&fe);
    oute << "header\n";
    for (double v : macro) oute << fixedField(v);
    for (int c = 0; c < 6; ++c) oute << fixedField(0);
    oute << "\n";
    return true;
}
}

void TestHillCriterion::testYieldPointScaleInvariantBelowOldThreshold()
{
    const auto field = syntheticVoxelStress(200);
    const std::array<double, 6> macro = { 3.0e6, -1.0e6, 2.0e6, 1.0e6, -2.0e6, 0.5e6 };
    const std::vector<int> grains(field.size(), 1);
    const std::vector<std::vector<float>> local_cs = { {0, 0, 0}, {0.3f, 0.7f, 1.1f} };

    std::vector<std::array<double, 6>> tiny;
    for (const auto& s : field) tiny.push_back(scaled(s, kBelowOldThreshold));

    struct Case { const char* criterion; const char* space; };
    for (const Case& c : { Case{"vonmises", "full6d"}, Case{"schmid", "deviatoric5d"} }) {
        HillCriterion h;
        h.setYieldSettings(c.criterion, c.space, 120e6);
        const auto ref = h.computeYieldPointForStep(field, grains, macro, local_cs, 1);
        const auto got = h.computeYieldPointForStep(tiny, grains, scaled(macro, kBelowOldThreshold), local_cs, 1);
        QVERIFY(ref.has_value());
        QVERIFY2(got.has_value(), c.criterion);

        double norm = 0.0;
        for (double v : *ref) norm = std::max(norm, std::abs(v));
        for (int i = 0; i < 6; ++i)
            QVERIFY2(std::abs((*got)[i] - (*ref)[i]) <= 1e-9 * norm,
                     qPrintable(QString("%1 [%2]: %3 vs %4").arg(c.criterion).arg(i).arg((*got)[i]).arg((*ref)[i])));
    }
}

void TestHillCriterion::testYieldPointsWrapperScaleInvariantBelowOldThreshold()
{
    // Same invariance through the ANSYS path, step 2 being the tiny copy of step 1
    const auto field = syntheticVoxelStress(200);
    const std::array<double, 6> macro = { 3.0e6, -1.0e6, 2.0e6, 1.0e6, -2.0e6, 0.5e6 };
    std::vector<std::array<double, 6>> tiny;
    for (const auto& s : field) tiny.push_back(scaled(s, kBelowOldThreshold));

    ElasticFitWrapper wr;
    wr.eps_as_loading.assign(2, std::vector<float>(6, 0.0f));
    QVERIFY(writeLoadStep(wr, 1, field, macro));
    QVERIFY(writeLoadStep(wr, 2, tiny, scaled(macro, kBelowOldThreshold)));

    HillCriterion h;
    h.setYieldSettings("vonmises", "full6d", 120e6);
    const auto points = h.computeYieldPoints(&wr, {}, 1, nullptr);
    QCOMPARE(points.size(), size_t(2));
    for (int i = 0; i < 6; ++i) // results are single precision on this path
        QVERIFY2(std::abs(points[1][i] - points[0][i]) <= 1e-5 * std::abs(points[0][i]),
                 qPrintable(QString("[%1]: %2 vs %3").arg(i).arg(points[1][i]).arg(points[0][i])));
}

void TestHillCriterion::testYieldPointSkipsDegenerateStep()
{
    HillCriterion h;
    h.setYieldSettings("vonmises", "full6d", 120e6);
    const std::vector<std::array<double, 6>> zeros(100, std::array<double, 6>{});
    const std::vector<int> grains(100, 1);
    const std::array<double, 6> macro = { 1.0, 0, 0, 0, 0, 0 };

    // All zeros
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("degenerate"));
    QVERIFY(!h.computeYieldPointForStep(zeros, grains, macro, {}, 7).has_value());

    // Critical element zero, a few others loaded
    auto mostlyZero = zeros;
    mostlyZero[0][0] = 1.0e8;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("degenerate"));
    QVERIFY(!h.computeYieldPointForStep(mostlyZero, grains, macro, {}, 8).has_value());

    // Critical element nonzero but below kDegenerateTauRatio of the step's maximum
    auto noisy = zeros;
    for (auto& s : noisy) s[0] = 1.0e-6;
    noisy[99][0] = 1.0e8;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("degenerate"));
    QVERIFY(!h.computeYieldPointForStep(noisy, grains, macro, {}, 9).has_value());

    // The maximum itself is never degenerate
    h.setYieldSettings("vonmises", "full6d", 120e6, 1.0);
    QVERIFY(h.computeYieldPointForStep(noisy, grains, macro, {}, 10).has_value());
}

void TestHillCriterion::testYieldPercentileOneSelectsMaximum()
{
    // Uniaxial sx = 1..100 MPa in shuffled order: 0.98 picks index 98, 1.0 the maximum
    std::vector<std::array<double, 6>> field;
    for (int i = 1; i <= 100; ++i) field.push_back({ i * 1.0e6, 0, 0, 0, 0, 0 });
    std::shuffle(field.begin(), field.end(), std::mt19937(5));
    const std::vector<int> grains(field.size(), 1);
    const std::array<double, 6> macro = { 1.0e6, 0, 0, 0, 0, 0 };
    const double Y = 120e6;

    HillCriterion h;
    h.setYieldSettings("vonmises", "full6d", Y, 1.0);
    auto p = h.computeYieldPointForStep(field, grains, macro, {}, 1);
    QVERIFY(p.has_value());
    QVERIFY(std::abs((*p)[0] - Y / 100.0) <= 1e-9 * Y);

    h.setYieldSettings("vonmises", "full6d", Y, 0.98);
    p = h.computeYieldPointForStep(field, grains, macro, {}, 1);
    QVERIFY(p.has_value());
    QVERIFY(std::abs((*p)[0] - Y / 99.0) <= 1e-9 * Y);

    // Default-constructed is the historical 98th percentile
    HillCriterion def;
    def.setYieldSettings("vonmises", "full6d", Y);
    const auto q = def.computeYieldPointForStep(field, grains, macro, {}, 1);
    QVERIFY(q.has_value());
    QCOMPARE((*q)[0], (*p)[0]);
}

void TestHillCriterion::testYieldPercentileAutoResolution()
{
    bool isAuto = false;
    QCOMPARE(Parameters::resolveYieldPercentile("Voronoi", 0.0, &isAuto), 0.98);
    QVERIFY(isAuto);
    QCOMPARE(Parameters::resolveYieldPercentile("DLCA", 0.0, &isAuto), 1.0);
    QVERIFY(isAuto);
    for (const char* algo : { "Polycrystall", "Moore", "Probability", "Composite", "" })
        QCOMPARE(Parameters::resolveYieldPercentile(algo, 0.0), 0.98);

    // Explicit wins over the table, in both directions
    QCOMPARE(Parameters::resolveYieldPercentile("DLCA", 0.9, &isAuto), 0.9);
    QVERIFY(!isAuto);
    QCOMPARE(Parameters::resolveYieldPercentile("Voronoi", 1.0, &isAuto), 1.0);
    QVERIFY(!isAuto);
}

void TestHillCriterion::testYieldPercentileParsing()
{
    Parameters::resetDefaults();
    QCOMPARE(Parameters::yield_percentile, 0.0);

    QString err;
    QVERIFY(applyParameter("yield_percentile", "0.95", &err));
    QCOMPARE(Parameters::yield_percentile, 0.95);
    QVERIFY(applyParameter("yield_percentile", "1", &err));
    QCOMPARE(Parameters::yield_percentile, 1.0);
    QVERIFY(applyParameter("yield_percentile", " AUTO ", &err));
    QCOMPARE(Parameters::yield_percentile, 0.0);

    QVERIFY(applyParameter("yield_percentile", "0.9", &err));
    for (const char* bad : { "0", "-0.5", "1.01", "nan", "inf", "abc" }) {
        err.clear();
        QVERIFY2(!applyParameter("yield_percentile", bad, &err), bad);
        QVERIFY(!err.isEmpty());
        QCOMPARE(Parameters::yield_percentile, 0.9);
    }

    // CLI
    Parameters::resetDefaults();
    {
        QCommandLineParser parser;
        Commandline_Parser::setupParser(parser);
        QVERIFY(parser.parse({ "MatViz3D", "--yield_percentile", "0.97" }));
        QVERIFY(Commandline_Parser::processOptions(parser));
        QCOMPARE(Parameters::yield_percentile, 0.97);
    }

    // YAML
    Parameters::resetDefaults();
    QTemporaryFile f(QDir::tempPath() + "/matviz_percentile_XXXXXX.yaml");
    QVERIFY(f.open());
    f.write("yield_percentile: 0.99\n");
    f.close();
    QString cfgErr;
    QVERIFY2(ConfigDispatcher::loadAndApply(f.fileName(), &cfgErr), qPrintable(cfgErr));
    QCOMPARE(Parameters::yield_percentile, 0.99);

    Parameters::resetDefaults();
    QCOMPARE(Parameters::yield_percentile, 0.0);
}

void TestHillCriterion::testYieldPercentileWrittenToHdf5()
{
    HillCriterion h;
    h.setYieldSettings("vonmises", "deviatoric5d", 120e6, 1.0);
    QVERIFY(h.fit(deviatoricSpherePoints(40, 100.0e6, 3)));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::string path = dir.filePath("hill.h5").toStdString();
    {
        HDF5Wrapper out(path);
        h.saveToHDF5(out, "/1");
    }
    HDF5Wrapper in(path);
    QCOMPARE(in.readDoubleAttribute("/1", "P_Hill", "yield_percentile", -1.0), 1.0);
    QCOMPARE(in.readDoubleAttribute("/1", "P_Hill", "no_such_attribute", -1.0), -1.0);
}

void TestHillCriterion::testMacroAboveYieldStressWarns()
{
    const double Y = 120e6;
    const std::vector<std::array<double, 6>> uniform(50, std::array<double, 6>{ 1.0e6, 0, 0, 0, 0, 0 });
    const std::vector<int> grains(uniform.size(), 1);
    const std::array<double, 6> macroAtBound = uniform[0];

    HillCriterion h;
    h.setYieldSettings("vonmises", "full6d", Y);

    // Macro twice the critical element: warned with the step number, point still returned
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Step 42: macro von Mises"));
    QVERIFY(h.computeYieldPointForStep(uniform, grains, scaled(macroAtBound, 2.0), {}, 42).has_value());

    // A homogeneous cell sits exactly on the bound: any warning here fails the test
    QTest::failOnWarning(QRegularExpression("macro von Mises"));
    QVERIFY(h.computeYieldPointForStep(uniform, grains, macroAtBound, {}, 43).has_value());

    // Schmid has no yield_stress to compare against
    const std::vector<std::vector<float>> local_cs = { {0, 0, 0}, {0, 0, 0} };
    HillCriterion s;
    s.setYieldSettings("schmid", "deviatoric5d", Y);
    QVERIFY(s.computeYieldPointForStep(uniform, grains, scaled(macroAtBound, 1000.0), local_cs, 44).has_value());
}

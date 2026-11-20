#include "test_propertysearch.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <vector>
#include "datasetbrowsercontroller.h"
#include "fft_solver_session.hpp"
#include "hdf5wrapper.h"
#include "parameters.h"
#include "propertysearch.h"
#include "propertyselectorcontroller.h"

using propsearch::Cell;
using propsearch::Law;
using propsearch::Mat6;

namespace {

// Engineering-basis stiffness (pipeline order) of an isotropic solid.
Mat6 isotropicC(double E, double nu)
{
    const double lam = E * nu / ((1 + nu) * (1 - 2 * nu)), mu = E / (2 * (1 + nu));
    Mat6 C{};
    for (auto& r : C) r.fill(0.0);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) C[i][j] = (i == j) ? lam + 2 * mu : lam;
        C[i + 3][i + 3] = mu;
    }
    return C;
}

// Engineering-basis stiffness of a cubic solid.
Mat6 cubicC(double C11, double C12, double C44)
{
    Mat6 C{};
    for (auto& r : C) r.fill(0.0);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) C[i][j] = (i == j) ? C11 : C12;
        C[i + 3][i + 3] = C44;
    }
    return C;
}

// The six unit engineering-strain solves of a session: C (pipeline order) and the tensor.
void solveCell(fftsa::FFTSolverSession& s, int nVox, Mat6& C, std::vector<double>& T)
{
    std::vector<std::vector<fftsa::Vec6>> fields(6);
    for (int j = 0; j < 6; ++j) {
        fftsa::Vec6 e{}; e.fill(0.0); e[j] = (j >= 3) ? 0.5 : 1.0;
        std::vector<fftsa::Vec6> eng;
        auto step = s.solveLoadCaseFull(e, eng);
        for (int i = 0; i < 6; ++i) C[i][j] = step.macro_stress[i];
        fields[static_cast<size_t>(j)] = eng;
    }
    for (int i = 0; i < 6; ++i)
        for (int j = i + 1; j < 6; ++j) C[i][j] = C[j][i] = 0.5 * (C[i][j] + C[j][i]);
    T = fftsa::FFTSolverSession::sensitivity_tensor(fields, static_cast<size_t>(nVox));
}

// A 12^3 block with a cubic pore, solved with one law.
void poreBlock(const Law& law, Mat6& C, std::vector<double>& T)
{
    const int n = 12;
    std::vector<int> grain(static_cast<size_t>(n) * n * n, 1);
    for (int z = 3; z < 9; ++z) for (int y = 3; y < 9; ++y) for (int x = 3; x < 9; ++x)
        grain[(static_cast<size_t>(z) * n + y) * n + x] = 0;
    double Cv[6][6];
    QVERIFY(Parameters::lawStiffnessPa(law.eGpa, law.nu, law.aniso, Cv));
    fftsa::Mat6 V{};
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) V[i][j] = Cv[i][j];
    const std::vector<std::array<double, 3>> orient = { { 0, 0, 0 }, { 0, 0, 0 } };
    fftsa::FFTSolverSession s(n, n, n, grain, orient, V, 1e-6);
    s.set_tolerance(1e-8);
    s.set_max_iters(5000);
    s.set_scheme(fftsa::FFTSolverSession::Scheme::CG);
    s.set_discretization(ffth::FFTHomogenizer::Discretization::Rotated);
    solveCell(s, n * n * n, C, T);
}

// One cell file as the dataset writes it; `engineering` = with the shear_basis attribute.
void writeCell(const QString& path, const Mat6& Ceng, const Law& law, double rho, double cellMm, bool engineering,
               const std::vector<double>* T = nullptr)
{
    HDF5Wrapper h(path.toStdString());
    h.write("/", "last_set", 1);
    std::vector<std::vector<float>> C(6, std::vector<float>(6));
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            C[static_cast<size_t>(i)][static_cast<size_t>(j)] = static_cast<float>(Ceng[i][j] * (engineering || j < 3 ? 1.0 : 2.0));
    h.write("/1", "C_matrix", C);
    h.write("/1", "S_matrix", C);   // the selector reads C only
    if (engineering) writeShearBasisAttribute(h, "/1");
    h.write("/1/geometry", "mat_e_gpa", law.eGpa);
    h.write("/1/geometry", "mat_nu", law.nu);
    h.write("/1/geometry", "mat_aniso", law.aniso);
    h.write("/1/geometry", "lattice_achieved_density", rho);
    h.write("/1/geometry", "lattice_cell_mm", cellMm);
    h.write("/1/geometry", "lattice_wall_mm", 1.0);
    if (T) {
        std::vector<std::vector<float>> M(36, std::vector<float>(36));
        for (int row = 0; row < 36; ++row)
            for (int col = 0; col < 36; ++col)
                M[static_cast<size_t>(row)][static_cast<size_t>(col)] =
                    static_cast<float>((*T)[static_cast<size_t>(row * 36 + col)] * (engineering || row % 6 < 3 ? 1.0 : 2.0));
        h.write("/1/geometry", "sensitivity_tensor", M);
    }
}

} // namespace

void TestPropertySearch::initTestCase()
{
    // The selector caches its index under the cache location: keep tests out of the user's.
    QStandardPaths::setTestModeEnabled(true);
}

void TestPropertySearch::testLawStep()
{
    // Reference values from the calibration of the step error (numpy, same definition).
    const Law base{ 2.888, 0.3306, 0.514 };
    QCOMPARE(propsearch::lawStep(base, base), 0.0);
    QVERIFY(std::abs(propsearch::lawStep(base, { 2.888, 0.42, 0.514 }) - 0.380) < 2e-3);
    QVERIFY(std::abs(propsearch::lawStep(base, { 2.888, 0.3306, 1.0 }) - 0.877) < 2e-3);
    QVERIFY(std::abs(propsearch::lawStep(base, { 2.888, 0.3306, 0.65 }) - 0.290) < 2e-3);
    QVERIFY(std::abs(propsearch::lawStep(base, { 2.0, 0.42, 1.0 }) - 1.477) < 2e-3);   // E does not count
}

void TestPropertySearch::testPredictionScalesExactly()
{
    Cell c;
    c.law = { 2.0, 0.38, 0.7 };
    c.C = isotropicC(0.2e9, 0.25);
    c.C[0][1] = c.C[1][0] = 0.31e8;
    std::vector<double> T(1296, 0.37e-1);
    const std::vector<double>* none = nullptr;
    const std::vector<double>* withT = &T;
    for (const std::vector<double>* t : { none, withT }) {
        const Mat6 P = propsearch::predictStiffness(c, { 3.0, 0.38, 0.7 }, t);
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j)
                QVERIFY2(std::abs(P[i][j] - 1.5 * c.C[i][j]) <= 1e-9 * std::abs(c.C[0][0]),
                         qPrintable(QString("C[%1][%2]").arg(i).arg(j)));
    }
    const auto m = propsearch::moduli(isotropicC(100e6, 0.3));
    QVERIFY(std::abs(m[propsearch::Ex] - 100e6) < 1e-3);
    QVERIFY(std::abs(m[propsearch::Gyz] - 100e6 / 2.6) < 1e-3);
    QVERIFY(std::abs(m[propsearch::NuXz] - 0.3) < 1e-12);
}

void TestPropertySearch::testPredictionAgainstASolve()
{
    // The tensor step from one law to another against a direct solve with the
    // other law: within the error the ranking assumes for that step size.
    const Law a{ 2.0, 0.36, 0.8 }, b{ 3.0, 0.40, 1.0 };
    Cell c;
    std::vector<double> T;
    poreBlock(a, c.C, T);
    c.law = a;
    Mat6 Cb{};
    std::vector<double> Tb;
    poreBlock(b, Cb, Tb);
    const auto direct = propsearch::moduli(Cb);
    const auto pred = propsearch::moduli(propsearch::predictStiffness(c, b, &T));
    const auto scaled = propsearch::moduli(propsearch::predictStiffness(c, b, nullptr));
    const double s = propsearch::lawStep(a, b);
    double worst = 0.0, worstScaled = 0.0;
    for (int t = propsearch::Ex; t <= propsearch::Gxz; ++t) {
        worst = std::max(worst, std::abs(pred[t] / direct[t] - 1.0));
        worstScaled = std::max(worstScaled, std::abs(scaled[t] / direct[t] - 1.0));
    }
    QVERIFY2(worst < std::max(0.01, 2.0 * propsearch::tensorStepError(s)),
             qPrintable(QString("tensor step %1: worst modulus error %2").arg(s).arg(worst)));
    QVERIFY2(worst < worstScaled, qPrintable(QString("tensor %1 vs scaling %2").arg(worst).arg(worstScaled)));
}

void TestPropertySearch::testReadCellConvertsTheShearBasis()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Mat6 C = isotropicC(150e6, 0.28);
    C[0][3] = C[3][0] = 4e6;   // a normal-shear coupling, where the two bases differ
    std::vector<double> T(1296);
    for (size_t k = 0; k < T.size(); ++k) T[k] = 1e-3 * static_cast<double>(k % 37);
    const Law law{ 2.5, 0.35, 0.9 };
    const QString legacy = dir.filePath(QStringLiteral("legacy.hdf5")), current = dir.filePath(QStringLiteral("eng.hdf5"));
    writeCell(legacy, C, law, 0.3, 11.0, false, &T);
    writeCell(current, C, law, 0.3, 11.0, true, &T);
    for (const QString& f : { legacy, current }) {
        Cell c;
        QVERIFY(propsearch::readCell(f, c));
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j)
                QVERIFY2(std::abs(c.C[i][j] - C[i][j]) <= 1e-6 * C[0][0], qPrintable(f + QString(" C[%1][%2]").arg(i).arg(j)));
        QCOMPARE(c.law.aniso, 0.9);
        QCOMPARE(c.density, 0.3);
        std::vector<double> R;
        QVERIFY(propsearch::readSensitivity(f, R));
        for (size_t k = 0; k < T.size(); ++k) QVERIFY(std::abs(R[k] - T[k]) < 1e-6);
    }
}

void TestPropertySearch::testSearchRanksAndFilters()
{
    std::vector<Cell> cells;
    auto add = [&](const char* id, const char* fam, double E, double nu, double rho) {
        Cell c;
        c.id = QString::fromLatin1(id);
        c.family = QString::fromLatin1(fam);
        c.law = { 2.0, 0.38, 1.0 };
        c.density = rho;
        c.C = isotropicC(E, nu);
        cells.push_back(c);
    };
    add("a", "gyroid_sheet", 100e6, 0.30, 0.20);
    add("b", "gyroid_sheet", 210e6, 0.30, 0.30);
    add("c", "bcc", 300e6, 0.10, 0.40);
    add("d", "bcc", 190e6, 0.45, 0.35);

    propsearch::Query q;
    q.material = { 2.0, 0.38, 1.0 };
    q.use[propsearch::Ex] = true;
    q.value[propsearch::Ex] = 200e6;
    auto r = propsearch::search(cells, q, nullptr);
    QCOMPARE(r.size(), size_t(4));
    QCOMPARE(cells[static_cast<size_t>(r[0].cell)].id, QStringLiteral("b"));   // 5 % off beats 5.3 %
    QCOMPARE(cells[static_cast<size_t>(r[1].cell)].id, QStringLiteral("d"));

    // A material twice as stiff halves the cells it needs.
    q.material.eGpa = 4.0;
    r = propsearch::search(cells, q, nullptr);
    QCOMPARE(cells[static_cast<size_t>(r[0].cell)].id, QStringLiteral("a"));
    q.material.eGpa = 2.0;

    q.use[propsearch::NuXy] = true;      // a Poisson's ratio target decides between b and d
    q.value[propsearch::NuXy] = 0.45;
    r = propsearch::search(cells, q, nullptr);
    QCOMPARE(cells[static_cast<size_t>(r[0].cell)].id, QStringLiteral("d"));

    q.family = QStringLiteral("gyroid_sheet");
    q.rhoMax = 0.25;
    r = propsearch::search(cells, q, nullptr);
    QCOMPARE(r.size(), size_t(1));
    QCOMPARE(cells[static_cast<size_t>(r[0].cell)].id, QStringLiteral("a"));

    // A cell solved with another law ranks below an equally good one solved with this law.
    std::vector<Cell> two = { cells[1], cells[1] };
    two[0].id = QStringLiteral("other law");
    two[0].law = { 2.0, 0.30, 0.5 };
    two[1].id = QStringLiteral("same law");
    propsearch::Query q2;
    q2.material = { 2.0, 0.38, 1.0 };
    q2.use[propsearch::Ex] = true;
    q2.value[propsearch::Ex] = 210e6;
    r = propsearch::search(two, q2, nullptr);
    QCOMPARE(two[static_cast<size_t>(r[0].cell)].id, QStringLiteral("same law"));
    QVERIFY(r[1].uncertainty > 0.1);
}

void TestPropertySearch::testAnisotropyMeasures()
{
    // Isotropic: every ratio 1, cubic, Zener 1.
    auto v = propsearch::moduli(isotropicC(100e6, 0.3));
    for (int t : { propsearch::EzEx, propsearch::EyEx, propsearch::EmaxEmin, propsearch::Zener })
        QVERIFY2(std::abs(v[t] - 1.0) < 1e-9, qPrintable(propsearch::targetKey(t) + QString(" = %1").arg(v[t])));
    QVERIFY(std::abs(v[propsearch::EMean] - 100e6) < 1e-3);
    QVERIFY(propsearch::cubicDeviation(isotropicC(100e6, 0.3)) < 1e-12);

    // Cubic: Ex = Ey = Ez, the Zener ratio as defined, and E max / E min between
    // <100> and <111> in closed form: 1/E(n) = S11 - 2 (S11 - S12 - S44/2) (n1²n2² + n2²n3² + n3²n1²).
    for (const double A : { 0.4, 2.5 }) {
        const double C11 = 300e6, C12 = 120e6, C44 = 0.5 * A * (C11 - C12);
        const Mat6 C = cubicC(C11, C12, C44);
        v = propsearch::moduli(C);
        QVERIFY(std::abs(v[propsearch::EzEx] - 1.0) < 1e-12 && std::abs(v[propsearch::EyEx] - 1.0) < 1e-12);
        QVERIFY(std::abs(v[propsearch::Zener] - A) < 1e-12);
        QVERIFY(propsearch::cubicDeviation(C) < 1e-12);
        const double S11 = (C11 + C12) / ((C11 - C12) * (C11 + 2 * C12)), S12 = -C12 / ((C11 - C12) * (C11 + 2 * C12));
        const double J = S11 - S12 - 0.5 / C44;
        const double e100 = 1.0 / S11, e111 = 1.0 / (S11 - 2.0 * J / 3.0);
        const double expected = std::max(e100, e111) / std::min(e100, e111);
        QVERIFY2(std::abs(v[propsearch::EmaxEmin] / expected - 1.0) < 1e-6,
                 qPrintable(QString("A %1: E max/E min %2, expected %3").arg(A).arg(v[propsearch::EmaxEmin]).arg(expected)));
    }

    // Weaker along Z: the axis ratio is exact, the cell is no longer cubic, and
    // E max / E min over all directions is at least the axis ratio.
    double Cv[6][6];
    QVERIFY(Parameters::lawStiffnessPa(2.0, 0.35, 0.5, Cv));
    Mat6 ti{};
    constexpr int kPipeToVoigt[6] = { 0, 1, 2, 5, 3, 4 };
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) ti[i][j] = Cv[kPipeToVoigt[i]][kPipeToVoigt[j]];
    v = propsearch::moduli(ti);
    QVERIFY2(std::abs(v[propsearch::EzEx] - 0.5) < 1e-9, qPrintable(QString::number(v[propsearch::EzEx])));
    QVERIFY(v[propsearch::EmaxEmin] >= 2.0 - 1e-9);
    QVERIFY(propsearch::cubicDeviation(ti) > propsearch::kMaxCubicDeviation);
}

void TestPropertySearch::testMoreMeasures()
{
    // Isotropic: every ratio 1, A^U 0, K = E / 3(1 - 2 nu), nu the same in every direction.
    auto v = propsearch::moduli(isotropicC(100e6, 0.3));
    for (int t : { propsearch::EzEy, propsearch::GxzGxy, propsearch::GyzGxy, propsearch::GyzGxz, propsearch::GmaxGmin })
        QVERIFY2(std::abs(v[t] - 1.0) < 1e-9, qPrintable(propsearch::targetKey(t) + QString(" = %1").arg(v[t])));
    QVERIFY2(std::abs(v[propsearch::AU]) < 1e-9, qPrintable(QString::number(v[propsearch::AU])));
    QVERIFY(std::abs(v[propsearch::K] / (100e6 / (3.0 * 0.4)) - 1.0) < 1e-12);
    QVERIFY2(std::abs(v[propsearch::NuMin] - 0.3) < 1e-9, qPrintable(QString::number(v[propsearch::NuMin])));

    // Cubic: G lies between C44 and (C11 - C12) / 2, so G max / G min = max(A, 1/A);
    // A^U = 6/5 (sqrt A - 1/sqrt A)^2; K = (C11 + 2 C12) / 3.
    for (const double A : { 0.4, 2.5 }) {
        const double C11 = 300e6, C12 = 120e6, C44 = 0.5 * A * (C11 - C12);
        const Mat6 C = cubicC(C11, C12, C44);
        v = propsearch::moduli(C);
        QVERIFY2(std::abs(v[propsearch::GmaxGmin] / std::max(A, 1.0 / A) - 1.0) < 1e-6,
                 qPrintable(QString("A %1: G max/G min %2").arg(A).arg(v[propsearch::GmaxGmin])));
        const double aU = 1.2 * std::pow(std::sqrt(A) - 1.0 / std::sqrt(A), 2);
        QVERIFY2(std::abs(v[propsearch::AU] - aU) < 1e-9, qPrintable(QString("A %1: A^U %2, expected %3").arg(A).arg(v[propsearch::AU]).arg(aU)));
        QVERIFY(std::abs(v[propsearch::K] / ((C11 + 2 * C12) / 3.0) - 1.0) < 1e-12);
        for (int t : { propsearch::EzEy, propsearch::GxzGxy, propsearch::GyzGxy, propsearch::GyzGxz })
            QVERIFY(std::abs(v[t] - 1.0) < 1e-12);

        // nu min against a dense search over load directions and directions across them.
        fftsa::Mat6 Cm{}, S{};
        for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) Cm[i][j] = C[i][j];
        QVERIFY(fftsa::invert6x6(Cm, S));
        auto axial = [](const std::array<double, 3>& n) {
            return std::array<double, 6>{ n[0] * n[0], n[1] * n[1], n[2] * n[2], n[0] * n[1], n[1] * n[2], n[0] * n[2] };
        };
        auto q = [&](const std::array<double, 6>& a, const std::array<double, 6>& b) {
            double r = 0.0;
            for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) r += a[i] * S[i][j] * b[j];
            return r;
        };
        double brute = 1e9;
        for (int i = 0; i < 400; ++i) {
            const double th = M_PI * (i + 0.5) / 400;
            for (int j = 0; j < 400; ++j) {
                const double ph = M_PI * j / 400;
                const std::array<double, 3> n{ std::sin(th) * std::cos(ph), std::sin(th) * std::sin(ph), std::cos(th) };
                const std::array<double, 3> t1{ std::cos(th) * std::cos(ph), std::cos(th) * std::sin(ph), -std::sin(th) };
                const std::array<double, 3> t2{ -std::sin(ph), std::cos(ph), 0.0 };
                for (int k = 0; k < 36; ++k) {
                    const double ps = M_PI * k / 36;
                    const std::array<double, 3> m{ std::cos(ps) * t1[0] + std::sin(ps) * t2[0], std::cos(ps) * t1[1] + std::sin(ps) * t2[1],
                                                   std::cos(ps) * t1[2] + std::sin(ps) * t2[2] };
                    brute = std::min(brute, -q(axial(m), axial(n)) / q(axial(n), axial(n)));
                }
            }
        }
        QVERIFY2(v[propsearch::NuMin] <= brute + 1e-6 && v[propsearch::NuMin] > brute - 1e-3,
                 qPrintable(QString("A %1: nu min %2, dense search %3").arg(A).arg(v[propsearch::NuMin]).arg(brute)));
    }

    // Weaker across the layers (transversely isotropic about Z): G xz = G yz = r G xy.
    double Cv[6][6];
    QVERIFY(Parameters::lawStiffnessPa(2.0, 0.35, 0.5, Cv));
    Mat6 ti{};
    constexpr int kPipeToVoigt[6] = { 0, 1, 2, 5, 3, 4 };
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) ti[i][j] = Cv[kPipeToVoigt[i]][kPipeToVoigt[j]];
    v = propsearch::moduli(ti);
    QVERIFY2(std::abs(v[propsearch::GxzGxy] - 0.5) < 1e-9, qPrintable(QString::number(v[propsearch::GxzGxy])));
    QVERIFY(std::abs(v[propsearch::GyzGxz] - 1.0) < 1e-9);
    QVERIFY(std::abs(v[propsearch::EzEy] - 0.5) < 1e-9);
    QVERIFY(v[propsearch::GmaxGmin] >= 2.0 - 1e-9);
    QVERIFY(v[propsearch::AU] > 0.1);
}

void TestPropertySearch::testSearchByAnisotropy()
{
    std::vector<Cell> cells;
    auto add = [&](const char* id, const Mat6& C) {
        Cell c;
        c.id = QString::fromLatin1(id);
        c.family = QStringLiteral("cubic_assembly");
        c.law = { 2.0, 0.38, 1.0 };
        c.density = 0.3;
        c.C = C;
        cells.push_back(c);
    };
    add("isotropic", isotropicC(200e6, 0.3));
    add("diagonals", cubicC(300e6, 120e6, 0.5 * 2.5 * 180e6));   // Zener 2.5
    add("axes", cubicC(300e6, 120e6, 0.5 * 0.4 * 180e6));        // Zener 0.4
    Mat6 tall = isotropicC(200e6, 0.3);
    tall[2][2] *= 3.0;                                             // stiffer along Z, not cubic
    add("tall", tall);

    propsearch::Query q;
    q.material = { 2.0, 0.38, 1.0 };
    auto best = [&]() {
        const auto r = propsearch::search(cells, q, nullptr);
        return r.empty() ? QString() : cells[static_cast<size_t>(r.front().cell)].id;
    };
    q.use[propsearch::Zener] = true;
    q.value[propsearch::Zener] = 2.2;
    QCOMPARE(best(), QStringLiteral("diagonals"));
    q.value[propsearch::Zener] = 0.5;
    QCOMPARE(best(), QStringLiteral("axes"));
    // A Zener target leaves the non-cubic cell out, however close its number.
    for (const auto& m : propsearch::search(cells, q, nullptr))
        QVERIFY(cells[static_cast<size_t>(m.cell)].id != QStringLiteral("tall"));
    q.use[propsearch::Zener] = false;

    q.use[propsearch::EzEx] = true;
    q.value[propsearch::EzEx] = 2.5;
    QCOMPARE(best(), QStringLiteral("tall"));
    q.use[propsearch::EzEx] = false;

    q.use[propsearch::EmaxEmin] = true;
    q.value[propsearch::EmaxEmin] = 1.0;
    QCOMPARE(best(), QStringLiteral("isotropic"));
    q.use[propsearch::EmaxEmin] = false;

    q.use[propsearch::EzEy] = true;
    q.value[propsearch::EzEy] = 2.5;
    QCOMPARE(best(), QStringLiteral("tall"));
    q.use[propsearch::EzEy] = false;

    q.use[propsearch::AU] = true;
    q.value[propsearch::AU] = 0.0;
    QCOMPARE(best(), QStringLiteral("isotropic"));

    // The precomputed moduli of every cell give the same ranking as computing them anew.
    q.use[propsearch::GmaxGmin] = true;
    q.value[propsearch::GmaxGmin] = 2.0;
    q.use[propsearch::Ex] = true;
    q.value[propsearch::Ex] = 150e6;
    q.material = { 3.0, 0.38, 1.0 };
    std::vector<propsearch::Values> own;
    for (const auto& c : cells) own.push_back(propsearch::moduli(c.C));
    const auto fresh = propsearch::search(cells, q, nullptr);
    const auto cached = propsearch::search(cells, q, nullptr, &own);
    QCOMPARE(cached.size(), fresh.size());
    for (size_t i = 0; i < fresh.size(); ++i) {
        QCOMPARE(cached[i].cell, fresh[i].cell);
        QVERIFY(std::abs(cached[i].fit - fresh[i].fit) < 1e-9);
    }
}

void TestPropertySearch::testControllerOnADatasetFolder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir d(dir.path());
    QVERIFY(d.mkpath(QStringLiteral("cells")));
    {
        QFile m(d.filePath(QStringLiteral("manifest.csv")));
        QVERIFY(m.open(QIODevice::WriteOnly | QIODevice::Text));
        m.write("id,group,status,p_lattice_family,p_relative_density,p_lattice_cell_mm,grid\n");
        m.write("000001,tpms,done,gyroid_sheet,0.2,12,64\n");
        m.write("000002,tpms,done,gyroid_sheet,0.3,12,64\n");
        m.write("000003,struts,done,bcc,0.35,10,64\n");
        m.write("000004,struts,unprintable,bcc,0.1,10,\n");
    }
    const Law law{ 2.0, 0.38, 1.0 };
    writeCell(d.filePath(QStringLiteral("cells/000001.hdf5")), isotropicC(100e6, 0.3), law, 0.2, 12.0, false);
    writeCell(d.filePath(QStringLiteral("cells/000002.hdf5")), isotropicC(205e6, 0.3), law, 0.3, 12.0, true);
    writeCell(d.filePath(QStringLiteral("cells/000003.hdf5")), isotropicC(260e6, 0.2), law, 0.35, 10.0, false);

    DatasetBrowserController browser;
    QStringList opened;
    browser.setCellOpener([&opened](const QString& p) { opened << p; return true; });
    PropertySelectorController sel(&browser);
    QVERIFY(browser.openPath(dir.path()));
    QVERIFY(!sel.ready());
    sel.buildIndexNow();
    QVERIFY(sel.ready());
    QCOMPARE(sel.cellCount(), 3);
    QCOMPARE(sel.families().size(), 3);   // "All families" + two
    // Slider ends: the cells span E 100-260 MPa with their own 2 GPa material.
    const QVariantList spanE = sel.ranges(2.0, 4.0).value(QStringLiteral("E")).toList();
    QVERIFY2(std::abs(spanE.at(0).toDouble() - 100.0) < 1e-3 && std::abs(spanE.at(1).toDouble() - 520.0) < 1e-3,
             qPrintable(QString("%1 .. %2").arg(spanE.at(0).toDouble()).arg(spanE.at(1).toDouble())));

    browser.setGroupFilter(QStringLiteral("struts"));   // hides the answer: showing it must relax the filter
    sel.search({ { QStringLiteral("material"), QVariantMap{ { QStringLiteral("e_gpa"), 2.0 }, { QStringLiteral("nu"), 0.38 }, { QStringLiteral("aniso"), 1.0 } } },
                 { QStringLiteral("targets"), QVariantMap{ { QStringLiteral("Ex"), 200.0 }, { QStringLiteral("Ey"), 200.0 }, { QStringLiteral("Ez"), 200.0 } } } });
    QCOMPARE(sel.results().size(), 3);
    QCOMPARE(sel.results().front().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("000002"));
    QCOMPARE(sel.currentResult(), 0);
    QCOMPARE(browser.currentId(), QStringLiteral("000002"));
    QVERIFY(opened.last().endsWith(QStringLiteral("cells/000002.hdf5")));

    sel.show(2);
    QCOMPARE(browser.currentId(), sel.results().at(2).toMap().value(QStringLiteral("id")).toString());

    // Unreachable at this density: above the Hashin-Shtrikman bound of the material.
    sel.search({ { QStringLiteral("targets"), QVariantMap{ { QStringLiteral("Ex"), 1500.0 } } },
                 { QStringLiteral("rho_max"), 0.4 } });
    QVERIFY2(sel.message().contains(QStringLiteral("Hashin")), qPrintable(sel.message()));

    // No target ticked: a message, no results.
    sel.search({});
    QVERIFY(sel.results().isEmpty());
    QVERIFY(!sel.message().isEmpty());

    // A second index of the same folder comes from the cache and finds the same cells,
    // in batches through the event loop; asking again once it is built changes nothing.
    PropertySelectorController again(&browser);
    again.buildIndex();
    QTRY_VERIFY(again.ready());
    QCOMPARE(again.cellCount(), 3);
    again.buildIndex();
    QVERIFY(again.ready() && !again.indexing());
}

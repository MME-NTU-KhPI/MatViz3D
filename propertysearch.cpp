#include "propertysearch.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "fft_solver_session.hpp"
#include "hdf5wrapper.h"
#include "parameters.h"

namespace propsearch {

namespace {

// Pipeline index (xx, yy, zz, xy, yz, xz) -> Voigt index (11, 22, 33, 23, 13, 12).
constexpr int kPipeToVoigt[6] = { 0, 1, 2, 5, 3, 4 };

// 1 / E along the unit vector n: n n : S : n n with engineering shear in S.
double inverseModulus(const fftsa::Mat6& S, const std::array<double, 3>& n)
{
    const double s[6] = { n[0] * n[0], n[1] * n[1], n[2] * n[2], n[0] * n[1], n[1] * n[2], n[0] * n[2] };
    double r = 0.0;
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) r += s[i] * S[i][j] * s[j];
    return r;
}

using Vec3 = std::array<double, 3>;

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
}
double dot3(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 combine(double p, const Vec3& a, double q, const Vec3& b)
{
    return { p * a[0] + q * b[0], p * a[1] + q * b[1], p * a[2] + q * b[2] };
}
Vec3 normalized(Vec3 v)
{
    const double l = std::sqrt(dot3(v, v));
    for (double& x : v) x /= l;
    return v;
}

// The 13 directions where a cubic or orthotropic cell has its extremes: the axes,
// the face diagonals and the body diagonals (one of each +/- pair).
const std::vector<Vec3>& symmetryDirections()
{
    static const std::vector<Vec3> dirs = [] {
        const double a = 1.0 / std::sqrt(2.0), b = 1.0 / std::sqrt(3.0);
        return std::vector<Vec3>{ { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { a, a, 0 }, { a, -a, 0 }, { a, 0, a }, { a, 0, -a },
                                  { 0, a, a }, { 0, a, -a }, { b, b, b }, { b, b, -b }, { b, -b, b }, { -b, b, b } };
    }();
    return dirs;
}

// A Fibonacci half sphere of `n` points (every quantity here is the same along n and -n)
// plus the symmetry directions.
std::vector<Vec3> halfSphere(int n)
{
    std::vector<Vec3> d;
    const double golden = M_PI * (3.0 - std::sqrt(5.0));
    for (int k = 0; k < n; ++k) {
        const double z = (k + 0.5) / n, r = std::sqrt(1.0 - z * z), phi = golden * k;
        d.push_back({ r * std::cos(phi), r * std::sin(phi), z });
    }
    for (const Vec3& v : symmetryDirections()) d.push_back(v);
    return d;
}

// Start points of the search for E over the sphere.
const std::vector<Vec3>& sphereDirections()
{
    static const std::vector<Vec3> dirs = halfSphere(600);
    return dirs;
}

// A unit vector across n: n x (the axis n is least along).
Vec3 across(const Vec3& n)
{
    const int k = (std::abs(n[0]) <= std::abs(n[1]) && std::abs(n[0]) <= std::abs(n[2])) ? 0
                : (std::abs(n[1]) <= std::abs(n[2]) ? 1 : 2);
    Vec3 e{ 0, 0, 0 };
    e[static_cast<size_t>(k)] = 1.0;
    return normalized(cross(n, e));
}

// Engineering stress of a unit uniaxial stress along n (also: what a strain vector
// is dotted with to give the normal strain along n).
std::array<double, 6> axial(const Vec3& n)
{
    return { n[0] * n[0], n[1] * n[1], n[2] * n[2], n[0] * n[1], n[1] * n[2], n[0] * n[2] };
}

// Engineering stress of a unit shear stress on the plane with normal n, along m
// (m across n); dotted with the strain vector it gives the shear strain gamma(n, m).
std::array<double, 6> shear(const Vec3& n, const Vec3& m)
{
    return { 2 * n[0] * m[0], 2 * n[1] * m[1], 2 * n[2] * m[2],
             n[0] * m[1] + n[1] * m[0], n[1] * m[2] + n[2] * m[1], n[0] * m[2] + n[2] * m[0] };
}

double quadratic(const fftsa::Mat6& S, const std::array<double, 6>& a, const std::array<double, 6>& b)
{
    double r = 0.0;
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) r += a[static_cast<size_t>(i)] * S[i][j] * b[static_cast<size_t>(j)];
    return r;
}

// Pattern search over orthonormal pairs (n, m) towards the largest f(n, m): small
// turns of the pair about n, about m and about n x m, the step halved when none helps.
template <class F>
double refinePair(const F& f, Vec3 n, Vec3 m)
{
    double best = f(n, m);
    double h = 0.1;
    for (int it = 0; it < 300 && h > 1e-5; ++it) {
        const Vec3 l = cross(n, m);
        const double c = std::cos(h);
        bool moved = false;
        for (int k = 0; k < 6 && !moved; ++k) {
            const double s = (k % 2 ? -1.0 : 1.0) * std::sin(h);
            Vec3 n2 = n, m2 = m;
            if (k / 2 == 0) m2 = combine(c, m, s, l);                                   // about n
            else if (k / 2 == 1) n2 = combine(c, n, -s, l);                             // about m
            else { n2 = combine(c, n, s, m); m2 = combine(c, m, -s, n); }               // about n x m
            const double v = f(n2, m2);
            if (v > best) { best = v; n = normalized(n2); m = normalized(m2); moved = true; }
        }
        if (!moved) h *= 0.5;
    }
    return best;
}

// Pattern search in the tangent plane from n towards the largest sign * 1/E.
double refineExtreme(const fftsa::Mat6& S, std::array<double, 3> n, double sign)
{
    double best = sign * inverseModulus(S, n);
    double h = 0.05;
    for (int it = 0; it < 400 && h > 1e-5; ++it) {
        // Tangent basis: n x (the axis n is least along), then n x t1.
        const int k = (std::abs(n[0]) <= std::abs(n[1]) && std::abs(n[0]) <= std::abs(n[2])) ? 0
                    : (std::abs(n[1]) <= std::abs(n[2]) ? 1 : 2);
        std::array<double, 3> e{ 0, 0, 0 };
        e[static_cast<size_t>(k)] = 1.0;
        std::array<double, 3> t1{ n[1] * e[2] - n[2] * e[1], n[2] * e[0] - n[0] * e[2], n[0] * e[1] - n[1] * e[0] };
        const double l1 = std::sqrt(t1[0] * t1[0] + t1[1] * t1[1] + t1[2] * t1[2]);
        for (double& x : t1) x /= l1;
        const std::array<double, 3> t2{ n[1] * t1[2] - n[2] * t1[1], n[2] * t1[0] - n[0] * t1[2], n[0] * t1[1] - n[1] * t1[0] };
        bool moved = false;
        for (int q = 0; q < 8 && !moved; ++q) {
            const double c = std::cos(q * M_PI / 4.0), s = std::sin(q * M_PI / 4.0);
            std::array<double, 3> m{ n[0] + h * (c * t1[0] + s * t2[0]), n[1] + h * (c * t1[1] + s * t2[1]),
                                     n[2] + h * (c * t1[2] + s * t2[2]) };
            const double l = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
            for (double& x : m) x /= l;
            const double v = sign * inverseModulus(S, m);
            if (v > best) { best = v; n = m; moved = true; }
        }
        if (!moved) h *= 0.5;
    }
    return sign * best;
}

std::string cellGroup(HDF5Wrapper& h)
{
    int set = h.readInt("/", "last_set");
    if (set <= 0) set = 1;
    return "/" + std::to_string(set);
}

double scalar(HDF5Wrapper& h, const std::string& group, const char* name, double fallback)
{
    return h.datasetExists(group, name) ? h.readDouble(group, name) : fallback;
}

} // namespace

QString targetKey(int t)
{
    static const char* keys[TargetCount] = { "Ex", "Ey", "Ez", "Gxy", "Gyz", "Gxz", "nuXy", "nuXz", "nuYz",
                                             "Emean", "Gmean", "EzEx", "EyEx", "EmaxEmin", "zener",
                                             "EzEy", "GxzGxy", "GyzGxy", "GyzGxz", "GmaxGmin", "AU", "K", "nuMin" };
    return (t >= 0 && t < TargetCount) ? QString::fromLatin1(keys[t]) : QString();
}

bool isModulus(int t) { return (t >= Ex && t <= Gxz) || t == EMean || t == GMean || t == K; }
bool isRatio(int t)
{
    switch (t) {
    case EzEx: case EyEx: case EzEy: case GxzGxy: case GyzGxy: case GyzGxz:
    case EmaxEmin: case GmaxGmin: case Zener:
        return true;
    default:
        return false;
    }
}
bool isPoisson(int t) { return (t >= NuXy && t <= NuYz) || t == NuMin; }
bool isAllDirections(int t) { return t == EmaxEmin || t == GmaxGmin || t == NuMin; }

bool readCell(const QString& file, Cell& cell, QString* error)
{
    auto fail = [&](const QString& why) { if (error) *error = why; return false; };
    HDF5Wrapper h(file.toStdString(), /*readOnly=*/true);
    const std::string pre = cellGroup(h);
    const std::string geo = pre + "/geometry";
    if (!h.datasetExists(pre, "C_matrix")) return fail(QStringLiteral("no C_matrix"));
    const auto m = h.readVectorVectorFloat(pre, "C_matrix");
    if (m.size() != 6) return fail(QStringLiteral("C_matrix is not 6x6"));
    for (int i = 0; i < 6; ++i) {
        if (m[static_cast<size_t>(i)].size() != 6) return fail(QStringLiteral("C_matrix is not 6x6"));
        for (int j = 0; j < 6; ++j) cell.C[i][j] = m[static_cast<size_t>(i)][static_cast<size_t>(j)];
    }
    // Tensor basis (no attribute): the shear columns are twice the engineering ones.
    if (h.readStringAttribute(pre, "C_matrix", "shear_basis") != QLatin1String("engineering"))
        for (int i = 0; i < 6; ++i)
            for (int j = 3; j < 6; ++j) cell.C[i][j] *= 0.5;

    if (!h.datasetExists(pre, "geometry")) return fail(QStringLiteral("no geometry group"));
    cell.law.eGpa  = scalar(h, geo, "mat_e_gpa", 0.0);
    cell.law.nu    = scalar(h, geo, "mat_nu", 0.38);
    cell.law.aniso = scalar(h, geo, "mat_aniso", 1.0);
    if (!(cell.law.eGpa > 0.0)) return fail(QStringLiteral("no material law (mat_e_gpa)"));
    cell.density = scalar(h, geo, "lattice_achieved_density", 0.0);
    cell.cellMm  = scalar(h, geo, "lattice_cell_mm", cell.cellMm);
    cell.wallMm  = scalar(h, geo, "lattice_wall_mm", 0.0);
    if (cell.family.isEmpty() && h.datasetExists(geo, "lattice_family_key"))
        cell.family = h.readQString(geo, "lattice_family_key");
    cell.file = file;
    return true;
}

bool readSensitivity(const QString& file, std::vector<double>& T)
{
    HDF5Wrapper h(file.toStdString(), /*readOnly=*/true);
    const std::string pre = cellGroup(h);
    const std::string geo = pre + "/geometry";
    if (!h.datasetExists(geo, "sensitivity_tensor")) return false;
    const auto m = h.readVectorVectorFloat(geo, "sensitivity_tensor");
    if (m.size() != 36) return false;
    T.assign(1296, 0.0);
    // 36 x 36: row = i*6 + j (stiffness entry), column = a*6 + b (Mandel law entry).
    for (int row = 0; row < 36; ++row) {
        if (m[static_cast<size_t>(row)].size() != 36) return false;
        for (int col = 0; col < 36; ++col)
            T[static_cast<size_t>(row * 36 + col)] = m[static_cast<size_t>(row)][static_cast<size_t>(col)];
    }
    // Tensor-basis files: like C, the entries of a shear column j are twice the engineering ones.
    if (h.readStringAttribute(pre, "C_matrix", "shear_basis") != QLatin1String("engineering"))
        for (int row = 0; row < 36; ++row)
            if (row % 6 >= 3)
                for (int col = 0; col < 36; ++col) T[static_cast<size_t>(row * 36 + col)] *= 0.5;
    return true;
}

Values moduli(const Mat6& C, bool allDirections)
{
    Values v;
    v.fill(std::numeric_limits<double>::quiet_NaN());
    fftsa::Mat6 S{};
    fftsa::Mat6 Cm{};
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) Cm[i][j] = C[i][j];
    if (!fftsa::invert6x6(Cm, S)) return v;
    for (int i = 0; i < 6; ++i) v[static_cast<size_t>(i)] = 1.0 / S[i][i];   // E x/y/z, then G xy/yz/xz
    v[NuXy] = -S[1][0] / S[0][0];
    v[NuXz] = -S[2][0] / S[0][0];
    v[NuYz] = -S[2][1] / S[1][1];
    if (v[Ex] > 0.0 && v[Ey] > 0.0 && v[Ez] > 0.0) {
        v[EMean] = std::cbrt(v[Ex] * v[Ey] * v[Ez]);
        v[EzEx] = v[Ez] / v[Ex];
        v[EyEx] = v[Ey] / v[Ex];
        v[EzEy] = v[Ez] / v[Ey];
    }
    if (v[Gxy] > 0.0 && v[Gyz] > 0.0 && v[Gxz] > 0.0) {
        v[GMean] = std::cbrt(v[Gxy] * v[Gyz] * v[Gxz]);
        v[GxzGxy] = v[Gxz] / v[Gxy];
        v[GyzGxy] = v[Gyz] / v[Gxy];
        v[GyzGxz] = v[Gyz] / v[Gxz];
    }
    const double c11 = (C[0][0] + C[1][1] + C[2][2]) / 3.0;
    const double c12 = (C[0][1] + C[1][2] + C[0][2] + C[1][0] + C[2][1] + C[2][0]) / 6.0;
    const double c44 = (C[3][3] + C[4][4] + C[5][5]) / 3.0;
    if (c11 > c12) v[Zener] = 2.0 * c44 / (c11 - c12);
    // Voigt (uniform strain) and Reuss (uniform stress) averages: the bulk modulus
    // is the Reuss one, uniform pressure over the volume change it causes.
    const double s11 = S[0][0] + S[1][1] + S[2][2];
    const double s12 = 0.5 * (S[0][1] + S[1][2] + S[0][2] + S[1][0] + S[2][1] + S[2][0]);
    const double s44 = S[3][3] + S[4][4] + S[5][5];
    const double sK = s11 + 2.0 * s12, sG = 4.0 * s11 - 4.0 * s12 + 3.0 * s44;
    if (sK > 0.0) v[K] = 1.0 / sK;
    if (sK > 0.0 && sG > 0.0) {
        const double KV = (3.0 * c11 + 6.0 * c12) / 9.0, GV = (3.0 * c11 - 3.0 * c12 + 9.0 * c44) / 15.0;
        v[AU] = std::max(0.0, 5.0 * GV * sG / 15.0 + KV * sK - 6.0);   // 5 G_V / G_R + K_V / K_R - 6
    }
    if (allDirections) {
        double lo = 0.0, hi = 0.0;
        directionalModulus(C, lo, hi);
        if (lo > 0.0) v[EmaxEmin] = hi / lo;
        double gLo = 0.0, gHi = 0.0, nuLo = 0.0;
        directionalShear(C, gLo, gHi, nuLo);
        if (gLo > 0.0) v[GmaxGmin] = gHi / gLo;
        v[NuMin] = nuLo;
    }
    return v;
}

double cubicDeviation(const Mat6& C)
{
    const double c11 = (C[0][0] + C[1][1] + C[2][2]) / 3.0;
    const double c12 = (C[0][1] + C[1][2] + C[0][2] + C[1][0] + C[2][1] + C[2][0]) / 6.0;
    const double c44 = (C[3][3] + C[4][4] + C[5][5]) / 3.0;
    double diff = 0.0, norm = 0.0;
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) {
            const double k = (i < 3 && j < 3) ? (i == j ? c11 : c12) : (i == j ? c44 : 0.0);
            diff += (C[i][j] - k) * (C[i][j] - k);
            norm += C[i][j] * C[i][j];
        }
    return norm > 0.0 ? std::sqrt(diff / norm) : std::numeric_limits<double>::quiet_NaN();
}

void directionalModulus(const Mat6& C, double& eMin, double& eMax)
{
    eMin = eMax = std::numeric_limits<double>::quiet_NaN();
    fftsa::Mat6 S{};
    fftsa::Mat6 Cm{};
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) Cm[i][j] = C[i][j];
    if (!fftsa::invert6x6(Cm, S)) return;
    // The best start points on the sphere, then a local search from each.
    double lo = std::numeric_limits<double>::infinity(), hi = -lo;
    std::array<double, 3> nLo{}, nHi{};
    for (const auto& n : sphereDirections()) {
        const double r = inverseModulus(S, n);
        if (!(r > 0.0)) return;   // not positive definite
        if (r < lo) { lo = r; nLo = n; }
        if (r > hi) { hi = r; nHi = n; }
    }
    lo = refineExtreme(S, nLo, -1.0);
    hi = refineExtreme(S, nHi, +1.0);
    if (!(lo > 0.0)) return;
    eMax = 1.0 / lo;
    eMin = 1.0 / hi;
}

void directionalShear(const Mat6& C, double& gMin, double& gMax, double& nuMin)
{
    gMin = gMax = nuMin = std::numeric_limits<double>::quiet_NaN();
    fftsa::Mat6 S{};
    fftsa::Mat6 Cm{};
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) Cm[i][j] = C[i][j];
    if (!fftsa::invert6x6(Cm, S)) return;
    auto invG = [&](const Vec3& n, const Vec3& m) { const auto b = shear(n, m); return quadratic(S, b, b); };
    auto nu = [&](const Vec3& n, const Vec3& m) {
        const auto a = axial(n);
        return -quadratic(S, axial(m), a) / quadratic(S, a, a);
    };

    // Coarse pass: 150 load directions, 12 directions across each (every 15°), and
    // the symmetry directions that lie across a symmetry direction.
    static const std::vector<Vec3> dirs = halfSphere(150);
    struct Best { double v; Vec3 n, m; };
    Best gLo{ std::numeric_limits<double>::infinity(), {}, {} }, gHi{ -gLo.v, {}, {} }, nLo = gLo;
    auto visit = [&](const Vec3& n, const Vec3& m, const std::array<double, 6>& Sa, double den) {
        const auto b = shear(n, m);
        const double g = quadratic(S, b, b);
        const auto am = axial(m);
        double num = 0.0;
        for (int i = 0; i < 6; ++i) num += am[static_cast<size_t>(i)] * Sa[static_cast<size_t>(i)];
        const double p = -num / den;
        if (g < gLo.v) gLo = { g, n, m };
        if (g > gHi.v) gHi = { g, n, m };
        if (p < nLo.v) nLo = { p, n, m };
        return g > 0.0;
    };
    for (const Vec3& n : dirs) {
        const auto a = axial(n);
        std::array<double, 6> Sa{};
        for (int i = 0; i < 6; ++i) {
            double r = 0.0;
            for (int j = 0; j < 6; ++j) r += S[i][j] * a[static_cast<size_t>(j)];
            Sa[static_cast<size_t>(i)] = r;
        }
        double den = 0.0;
        for (int i = 0; i < 6; ++i) den += a[static_cast<size_t>(i)] * Sa[static_cast<size_t>(i)];
        if (!(den > 0.0)) return;   // not positive definite
        const Vec3 t1 = across(n), t2 = cross(n, t1);
        for (int k = 0; k < 12; ++k) {
            const double psi = k * M_PI / 12.0;
            if (!visit(n, combine(std::cos(psi), t1, std::sin(psi), t2), Sa, den)) return;
        }
        for (const Vec3& d : symmetryDirections())
            if (std::abs(dot3(d, n)) < 1e-9 && !visit(n, d, Sa, den)) return;
    }
    const double invLo = -refinePair([&](const Vec3& n, const Vec3& m) { return -invG(n, m); }, gLo.n, gLo.m);
    const double invHi = refinePair(invG, gHi.n, gHi.m);
    if (!(invLo > 0.0)) return;
    gMax = 1.0 / invLo;
    gMin = 1.0 / invHi;
    nuMin = -refinePair([&](const Vec3& n, const Vec3& m) { return -nu(n, m); }, nLo.n, nLo.m);
}

bool lawMandel(const Law& law, Mat6& M)
{
    double Cv[6][6];
    if (!Parameters::lawStiffnessPa(law.eGpa, law.nu, law.aniso, Cv)) return false;
    const double r2 = std::sqrt(2.0);
    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < 6; ++b)
            M[a][b] = Cv[kPipeToVoigt[a]][kPipeToVoigt[b]] * (a < 3 ? 1.0 : r2) * (b < 3 ? 1.0 : r2);
    return true;
}

Mat6 predictStiffness(const Cell& cell, const Law& law, const std::vector<double>* T)
{
    Mat6 C = cell.C;
    if (T && T->size() == 1296) {
        // The step in nu and anisotropy at the cell's own E, then the exact E scaling.
        const Law moved{ cell.law.eGpa, law.nu, law.aniso };
        Mat6 M0{}, M1{};
        if (lawMandel(cell.law, M0) && lawMandel(moved, M1)) {
            for (int i = 0; i < 6; ++i)
                for (int j = 0; j < 6; ++j) {
                    double d = 0.0;
                    for (int a = 0; a < 6; ++a)
                        for (int b = 0; b < 6; ++b)
                            d += (*T)[static_cast<size_t>(((i * 6 + j) * 6 + a) * 6 + b)] * (M1[a][b] - M0[a][b]);
                    C[i][j] += d;
                }
        }
    }
    const double s = law.eGpa / cell.law.eGpa;
    for (auto& row : C) for (double& c : row) c *= s;
    return C;
}

namespace {

// Eigen-decomposition of a symmetric 6x6 by cyclic Jacobi rotations: A = V diag(w) V^T.
void jacobi6(Mat6 A, std::array<double, 6>& w, Mat6& V)
{
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) V[i][j] = (i == j) ? 1.0 : 0.0;
    for (int sweep = 0; sweep < 50; ++sweep) {
        double off = 0.0;
        for (int p = 0; p < 6; ++p) for (int q = p + 1; q < 6; ++q) off += A[p][q] * A[p][q];
        if (off < 1e-30) break;
        for (int p = 0; p < 6; ++p)
            for (int q = p + 1; q < 6; ++q) {
                if (std::abs(A[p][q]) < 1e-300) continue;
                const double th = 0.5 * (A[q][q] - A[p][p]) / A[p][q];
                const double t = (th >= 0 ? 1.0 : -1.0) / (std::abs(th) + std::sqrt(th * th + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0), s = t * c;
                for (int k = 0; k < 6; ++k) {
                    const double akp = A[k][p], akq = A[k][q];
                    A[k][p] = c * akp - s * akq;
                    A[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < 6; ++k) {
                    const double apk = A[p][k], aqk = A[q][k];
                    A[p][k] = c * apk - s * aqk;
                    A[q][k] = s * apk + c * aqk;
                }
                for (int k = 0; k < 6; ++k) {
                    const double vkp = V[k][p], vkq = V[k][q];
                    V[k][p] = c * vkp - s * vkq;
                    V[k][q] = s * vkp + c * vkq;
                }
            }
    }
    for (int i = 0; i < 6; ++i) w[static_cast<size_t>(i)] = A[i][i];
}

} // namespace

double lawStep(const Law& from, const Law& to)
{
    Mat6 M0{}, M1{};
    if (!lawMandel(from, M0) || !lawMandel({ from.eGpa, to.nu, to.aniso }, M1))
        return std::numeric_limits<double>::infinity();
    std::array<double, 6> w{};
    Mat6 V{};
    jacobi6(M0, w, V);
    Mat6 R{};   // M0^-1/2
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) {
            double r = 0.0;
            for (int k = 0; k < 6; ++k)
                if (w[static_cast<size_t>(k)] > 0.0) r += V[i][k] * V[j][k] / std::sqrt(w[static_cast<size_t>(k)]);
            R[i][j] = r;
        }
    Mat6 A{};   // R M1 R
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) {
            double a = 0.0;
            for (int k = 0; k < 6; ++k)
                for (int l = 0; l < 6; ++l) a += R[i][k] * M1[k][l] * R[l][j];
            A[i][j] = a;
        }
    jacobi6(A, w, V);
    double s = 0.0;
    for (double e : w) s = std::max(s, e > 0.0 ? std::abs(std::log(e)) : std::numeric_limits<double>::infinity());
    return s;
}

double fitError(const Values& p, const Query& q)
{
    double sum = 0.0;
    int n = 0;
    for (int t = 0; t < TargetCount; ++t) {
        if (!q.use[static_cast<size_t>(t)]) continue;
        const double y = q.value[static_cast<size_t>(t)], x = p[static_cast<size_t>(t)];
        double e;
        if (isModulus(t) || isRatio(t)) {
            if (!(x > 0.0) || !(y > 0.0)) return std::numeric_limits<double>::infinity();
            e = std::log(x / y);
        } else if (t == AU) {
            // 0 for an isotropic cell, so on the scale of 1 + A^U.
            if (!(x >= 0.0) || !(y >= 0.0)) return std::numeric_limits<double>::infinity();
            e = std::log((1.0 + x) / (1.0 + y));
        } else {
            // A Poisson's ratio off by 0.05 weighs like a modulus off by ~5 %.
            if (!std::isfinite(x)) return std::numeric_limits<double>::infinity();
            e = x - y;
        }
        sum += e * e;
        ++n;
    }
    return n > 0 ? std::sqrt(sum / n) : 0.0;
}

std::vector<Match> search(const std::vector<Cell>& cells, const Query& q,
                          const std::function<bool(const Cell&, std::vector<double>&)>& tensor,
                          const std::vector<Values>* own)
{
    const bool cubicOnly = q.use[Zener];
    bool allDirections = false;
    for (int t = 0; t < TargetCount; ++t) allDirections = allDirections || (q.use[static_cast<size_t>(t)] && isAllDirections(t));
    std::vector<Match> all;
    all.reserve(cells.size());
    for (int i = 0; i < static_cast<int>(cells.size()); ++i) {
        const Cell& c = cells[static_cast<size_t>(i)];
        if (c.density < q.rhoMin || c.density > q.rhoMax) continue;
        if (!q.family.isEmpty() && c.family != q.family) continue;
        Match m;
        m.cell = i;
        const Mat6 P = predictStiffness(c, q.material, nullptr);
        if (own && static_cast<size_t>(i) < own->size()) {
            // P is the cell's own C scaled: the moduli scale with it, the rest stays.
            const double s = q.material.eGpa / c.law.eGpa;
            m.predicted = (*own)[static_cast<size_t>(i)];
            for (int t = 0; t < TargetCount; ++t)
                if (isModulus(t)) m.predicted[static_cast<size_t>(t)] *= s;
        } else {
            m.predicted = moduli(P, allDirections);
        }
        m.fit = fitError(m.predicted, q);
        if (!std::isfinite(m.fit)) continue;
        // The scaling-only estimate ignores the step in nu / anisotropy altogether.
        m.uncertainty = scalingStepError(lawStep(c.law, q.material));
        m.score = std::hypot(m.fit, m.uncertainty);
        if (cubicOnly) {
            // A cubic cell printed weaker across the layers is not cubic as solved;
            // the tensor step below can make it cubic again for the user's material,
            // so here the distance from cubic only ranks, with a wide cut.
            m.cubic = cubicDeviation(P);
            if (!(m.cubic <= 3.0 * kMaxCubicDeviation)) continue;
            m.score = std::hypot(m.score, m.cubic);
        }
        all.push_back(m);
    }
    auto byScore = [](const Match& a, const Match& b) { return a.score < b.score; };
    const size_t shortlist = std::min(all.size(), static_cast<size_t>(std::max(200, 20 * q.count)));
    std::partial_sort(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(shortlist), all.end(), byScore);
    all.resize(shortlist);

    std::vector<double> T;
    for (Match& m : all) {
        const Cell& c = cells[static_cast<size_t>(m.cell)];
        m.corrected = tensor && tensor(c, T);
        const Mat6 P = predictStiffness(c, q.material, m.corrected ? &T : nullptr);
        m.predicted = moduli(P);
        m.fit = fitError(m.predicted, q);
        m.cubic = cubicDeviation(P);
        m.uncertainty = m.corrected ? tensorStepError(lawStep(c.law, q.material)) : scalingStepError(lawStep(c.law, q.material));
        m.score = std::hypot(m.fit, m.uncertainty);
    }
    all.erase(std::remove_if(all.begin(), all.end(), [&](const Match& m) {
                  return !std::isfinite(m.fit) || (cubicOnly && !(m.cubic <= kMaxCubicDeviation));
              }), all.end());
    std::sort(all.begin(), all.end(), byScore);
    if (static_cast<int>(all.size()) > q.count) all.resize(static_cast<size_t>(std::max(0, q.count)));
    return all;
}

double hsUpperE(double Es, double nus, double porosity)
{
    if (!(porosity >= 0.0 && porosity < 1.0) || Es <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    const double K = Es / (3.0 * (1.0 - 2.0 * nus)), G = Es / (2.0 * (1.0 + nus)), f = porosity;
    const double Khs = K + f / (-1.0 / K + 3.0 * (1.0 - f) / (3.0 * K + 4.0 * G));
    const double Ghs = G + f / (-1.0 / G + 6.0 * (1.0 - f) * (K + 2.0 * G) / (5.0 * G * (3.0 * K + 4.0 * G)));
    return 9.0 * Khs * Ghs / (3.0 * Khs + Ghs);
}

} // namespace propsearch

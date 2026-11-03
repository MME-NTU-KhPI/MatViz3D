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
    static const char* keys[TargetCount] = { "Ex", "Ey", "Ez", "Gxy", "Gyz", "Gxz", "nuXy", "nuXz", "nuYz" };
    return (t >= 0 && t < TargetCount) ? QString::fromLatin1(keys[t]) : QString();
}

bool isModulus(int t) { return t >= Ex && t <= Gxz; }

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

Values moduli(const Mat6& C)
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
    return v;
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
        if (isModulus(t)) {
            if (!(x > 0.0) || !(y > 0.0)) return std::numeric_limits<double>::infinity();
            e = std::log(x / y);
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
                          const std::function<bool(const Cell&, std::vector<double>&)>& tensor)
{
    std::vector<Match> all;
    all.reserve(cells.size());
    for (int i = 0; i < static_cast<int>(cells.size()); ++i) {
        const Cell& c = cells[static_cast<size_t>(i)];
        if (c.density < q.rhoMin || c.density > q.rhoMax) continue;
        if (!q.family.isEmpty() && c.family != q.family) continue;
        Match m;
        m.cell = i;
        m.predicted = moduli(predictStiffness(c, q.material, nullptr));
        m.fit = fitError(m.predicted, q);
        if (!std::isfinite(m.fit)) continue;
        // The scaling-only estimate ignores the step in nu / anisotropy altogether.
        m.uncertainty = scalingStepError(lawStep(c.law, q.material));
        m.score = std::hypot(m.fit, m.uncertainty);
        all.push_back(m);
    }
    auto byScore = [](const Match& a, const Match& b) { return a.score < b.score; };
    const size_t shortlist = std::min(all.size(), static_cast<size_t>(std::max(200, 20 * q.count)));
    std::partial_sort(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(shortlist), all.end(), byScore);
    all.resize(shortlist);

    std::vector<double> T;
    for (Match& m : all) {
        const Cell& c = cells[static_cast<size_t>(m.cell)];
        if (!tensor || !tensor(c, T)) continue;
        m.predicted = moduli(predictStiffness(c, q.material, &T));
        m.fit = fitError(m.predicted, q);
        m.uncertainty = tensorStepError(lawStep(c.law, q.material));
        m.score = std::hypot(m.fit, m.uncertainty);
        m.corrected = true;
    }
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

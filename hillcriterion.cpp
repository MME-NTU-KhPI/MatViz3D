#include "hillcriterion.h"
#include "ansyswrapper.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <QDebug>

// =============================================================================
// 12 FCC slip systems {111}<110> (for copper, aluminum, austenite)
// =============================================================================
const double HillCriterion::SLIP_NORMALS[12][3] = {
    // (111) plane
    { 1, 1, 1}, { 1, 1, 1}, { 1, 1, 1},
    // (-111) plane
    {-1, 1, 1}, {-1, 1, 1}, {-1, 1, 1},
    // (1-11) plane
    { 1,-1, 1}, { 1,-1, 1}, { 1,-1, 1},
    // (11-1) plane
    { 1, 1,-1}, { 1, 1,-1}, { 1, 1,-1}
};

const double HillCriterion::SLIP_DIRECTIONS[12][3] = {
    // <110> slip directions in the (111) plane
    { 1,-1, 0}, { 0, 1,-1}, {-1, 0, 1},
    // <110> slip directions in the (-111) plane
    { 1, 1, 0}, { 0, 1,-1}, { 1, 0, 1},
    // <110> slip directions in the (1-11) plane
    { 1, 1, 0}, { 0, 1, 1}, {-1, 0, 1},
    // <110> slip directions in the (11-1) plane
    { 1,-1, 0}, { 0, 1, 1}, { 1, 0, 1}
};

void HillCriterion::eulerToBungeMatrix(double phi1, double Phi, double phi2, double R[3][3])
{
    double c1 = std::cos(phi1), s1 = std::sin(phi1);
    double c  = std::cos(Phi),  s  = std::sin(Phi);
    double c2 = std::cos(phi2), s2 = std::sin(phi2);

    R[0][0] =  c1*c2 - s1*s2*c;
    R[0][1] =  s1*c2 + c1*s2*c;
    R[0][2] =  s2*s;
    R[1][0] = -c1*s2 - s1*c2*c;
    R[1][1] = -s1*s2 + c1*c2*c;
    R[1][2] =  c2*s;
    R[2][0] =  s1*s;
    R[2][1] = -c1*s;
    R[2][2] =  c;
}

double HillCriterion::resolvedShearStress(const double stress[3][3],
                                          const double n[3],
                                          const double b[3])
{
    auto normalize = [](const double v[3], double out[3]) {
        double len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
        if (len < 1e-12) { out[0]=out[1]=out[2]=0; return; }
        out[0]=v[0]/len; out[1]=v[1]/len; out[2]=v[2]/len;
    };

    double nn[3], bb[3];
    normalize(n, nn);
    normalize(b, bb);

    // τ = b_i * σ_ij * n_j
    double tau = 0.0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            tau += bb[i] * stress[i][j] * nn[j];

    return tau;
}

std::vector<std::array<double,6>> HillCriterion::computeYieldPoints(
    ansysWrapper* wr,
    const std::vector<std::vector<float>>& local_cs)
{
    std::vector<std::array<double,6>> yield_points;

    int num_steps = (int)wr->eps_as_loading.size();
    if (num_steps == 0 || local_cs.empty()) {
        qWarning() << "HillCriterion: no load steps or local_cs is empty.";
        return yield_points;
    }

    for (int ls = 1; ls <= num_steps; ++ls)
    {
        wr->load_loadstep(ls);
        auto& results = wr->loadstep_results;

        if (results.empty()) continue;

        double max_tau     = 0.0;
        int    max_elem_idx = 0;

        for (int e = 0; e < (int)results.size(); ++e)
        {
            int grain_id = (int)results[e][ID];
            int array_idx = grain_id;

            // array_idx = grain_id - 1;

            if (array_idx < 0 || array_idx >= (int)local_cs.size()) {
                qWarning() << "Grain ID out of bounds:" << array_idx;
                continue;
            }

            double phi1 = local_cs[array_idx][0];
            double Phi  = local_cs[array_idx][1];
            double phi2 = local_cs[array_idx][2];

            double R[3][3];
            eulerToBungeMatrix(phi1, Phi, phi2, R);

            double sigma_g[3][3] = {
                {results[e][SX],  results[e][SXY], results[e][SXZ]},
                {results[e][SXY], results[e][SY],  results[e][SYZ]},
                {results[e][SXZ], results[e][SYZ], results[e][SZ] }
            };

            double sigma_l[3][3] = {0};
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    for (int k = 0; k < 3; ++k)
                        for (int l = 0; l < 3; ++l)
                            sigma_l[i][j] += R[i][k] * sigma_g[k][l] * R[j][l];

            for (int sys = 0; sys < NUM_SLIP_SYSTEMS; ++sys)
            {
                double tau = std::abs(resolvedShearStress(
                    sigma_l, SLIP_NORMALS[sys], SLIP_DIRECTIONS[sys]));
                if (tau > max_tau) {
                    max_tau      = tau;
                    max_elem_idx = e;
                }
            }
        }

        if (max_tau < 1e-9) continue;

        double k = CRSS / max_tau;

        std::array<double,6> sigma_yield = {
            results[max_elem_idx][SX]  * k,
            results[max_elem_idx][SY]  * k,
            results[max_elem_idx][SZ]  * k,
            results[max_elem_idx][SXY] * k,
            results[max_elem_idx][SYZ] * k,
            results[max_elem_idx][SXZ] * k
        };
        yield_points.push_back(sigma_yield);
    }

    qDebug() << "HillCriterion: computed" << yield_points.size() << "yield points.";
    return yield_points;
}

bool HillCriterion::fit(const std::vector<std::array<double,6>>& yield_points)
{
    m_isValid = false;

    int N = (int)yield_points.size();
    if (N < 21) {
        qWarning() << "HillCriterion::fit: need at least 21 points, got" << N;
        return false;
    }

    // param: P00,P01,P02,P03,P04,P05, P11,P12,P13,P14,P15, P22,...
    auto param_idx = [](int i, int j) -> int {
        if (i > j) std::swap(i, j);
        int idx = 0;
        for (int r = 0; r < i; ++r) idx += (6 - r);
        idx += (j - i);
        return idx;
    };

    //A^T A x = A^T b, where b = 1
    double ATA[21][21] = {0};
    double ATb[21]     = {0};

    for (int n = 0; n < N; ++n)
    {
        const auto& s = yield_points[n];

        // σ^T P σ = Σ_{i<=j} (2 - δ_ij) * P_ij * σ_i * σ_j = 1
        double A_row[21] = {0};
        for (int i = 0; i < 6; ++i)
            for (int j = i; j < 6; ++j) {
                double val = s[i] * s[j];
                if (i != j) val *= 2.0;
                A_row[param_idx(i,j)] += val;
            }

        for (int i = 0; i < 21; ++i) {
            for (int j = 0; j < 21; ++j)
                ATA[i][j] += A_row[i] * A_row[j];
            ATb[i] += A_row[i] * 1.0; // b = 1
        }
    }

    double x[21] = {0};
    if (!solveSystem21x21(ATA, ATb, x)) {
        qWarning() << "HillCriterion::fit: singular matrix in least squares.";
        return false;
    }

    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            m_P_Hill[i][j] = x[param_idx(i,j)];

    if (!computeCholesky()) {
        qWarning() << "HillCriterion::fit: P_Hill is not positive definite.";
        return false;
    }

    m_isValid = true;
    qDebug() << "HillCriterion::fit: P_Hill fitted successfully.";
    return true;
}

// =============================================================================
// computeCholesky — P_Hill = L * L^T, then L^{-1}
// =============================================================================
bool HillCriterion::computeCholesky()
{
    memset(m_L,    0, sizeof(m_L));
    memset(m_Linv, 0, sizeof(m_Linv));

    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j <= i; ++j) {
            double sum = 0;
            for (int k = 0; k < j; ++k) sum += m_L[i][k] * m_L[j][k];
            if (i == j) {
                double val = m_P_Hill[i][i] - sum;
                if (val <= 0) return false;
                m_L[i][j] = std::sqrt(val);
            } else {
                m_L[i][j] = (m_P_Hill[i][j] - sum) / m_L[j][j];
            }
        }
    }

    for (int i = 0; i < 6; ++i) {
        m_Linv[i][i] = 1.0 / m_L[i][i];
        for (int j = 0; j < i; ++j) {
            double sum = 0;
            for (int k = j; k < i; ++k) sum += m_L[i][k] * m_Linv[k][j];
            m_Linv[i][j] = -sum / m_L[i][i];
        }
    }

    return true;
}

// =============================================================================
// eps = S * P_Hill^{-1/2} * d = S * Linv^T * d
// =============================================================================
std::vector<std::vector<double>> HillCriterion::generateLoads(
    int num_samples,
    double strain_val,
    const double S[6][6])
{
    std::vector<std::vector<double>> load_cases;
    if (!m_isValid) {
        qWarning() << "HillCriterion::generateLoads: P_Hill not fitted.";
        return load_cases;
    }

    std::mt19937 gen(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int n = 0; n < num_samples; ++n)
    {
        double d[6], norm_sq = 0.0;
        for (int i = 0; i < 6; ++i) { d[i] = dist(gen); norm_sq += d[i]*d[i]; }
        double norm = std::sqrt(norm_sq);
        if (norm < 1e-12) { --n; continue; }
        for (int i = 0; i < 6; ++i) d[i] /= norm;

        double d_sigma[6] = {0};
        for (int i = 0; i < 6; ++i)
            for (int j = i; j < 6; ++j)   // Linv^T[i][j] = Linv[j][i]
                d_sigma[i] += m_Linv[j][i] * d[j];

        double d_eps[6] = {0};
        double eps_norm_sq = 0.0;
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j)
                d_eps[i] += S[i][j] * d_sigma[j];
            eps_norm_sq += d_eps[i] * d_eps[i];
        }
        double eps_norm = std::sqrt(eps_norm_sq);
        if (eps_norm < 1e-30) { --n; continue; }

        std::vector<double> eps(6);
        for (int i = 0; i < 6; ++i)
            eps[i] = (d_eps[i] / eps_norm) * strain_val;

        load_cases.push_back(eps);
    }

    return load_cases;
}

// =============================================================================
// HDF5
// =============================================================================
void HillCriterion::saveToHDF5(HDF5Wrapper& hdf5, const std::string& prefix)
{
    std::vector<std::vector<float>> mat(6, std::vector<float>(6));
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            mat[i][j] = static_cast<float>(m_P_Hill[i][j]);
    hdf5.write(prefix, "P_Hill", mat);
    qDebug() << "HillCriterion: P_Hill saved to HDF5.";
}

bool HillCriterion::loadFromHDF5(HDF5Wrapper& hdf5, const std::string& prefix)
{
    try {
        auto mat = hdf5.readVectorVectorFloat(prefix, "P_Hill");
        if (mat.empty() || (int)mat.size() != 6 || (int)mat[0].size() != 6)
            return false;

        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j)
                m_P_Hill[i][j] = static_cast<double>(mat[i][j]);

        if (!computeCholesky()) {
            qWarning() << "HillCriterion::loadFromHDF5: P_Hill not positive definite.";
            return false;
        }

        m_isValid = true;
        qDebug() << "HillCriterion: P_Hill loaded from HDF5.";
        return true;

    } catch (...) {
        qWarning() << "HillCriterion::loadFromHDF5: failed to read P_Hill.";
        return false;
    }
}

bool HillCriterion::solveSystem21x21(double A[21][21], double b[21], double x[21])
{
    const int N = 21;
    double M[N][N + 1];

    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) M[i][j] = A[i][j];
        M[i][N] = b[i];
    }

    for (int i = 0; i < N; ++i) {
        int pivot = i;
        for (int j = i + 1; j < N; ++j)
            if (std::abs(M[j][i]) > std::abs(M[pivot][i])) pivot = j;
        for (int k = 0; k <= N; ++k) std::swap(M[i][k], M[pivot][k]);

        if (std::abs(M[i][i]) < 1e-18) return false;

        for (int j = i + 1; j < N; ++j) {
            double factor = M[j][i] / M[i][i];
            for (int k = i; k <= N; ++k) M[j][k] -= factor * M[i][k];
        }
    }

    for (int i = N - 1; i >= 0; --i) {
        double sum = 0;
        for (int j = i + 1; j < N; ++j) sum += M[i][j] * x[j];
        x[i] = (M[i][N] - sum) / M[i][i];
    }
    return true;
}

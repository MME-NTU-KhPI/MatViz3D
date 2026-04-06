#include "hillcriterion.h"
#include "parameters.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <QDebug>

// 48 BCC slip systems: 24x{110}<111> + 24x{112}<111>
// All pairs verified: b·n = 0
const double HillCriterion::SLIP_NORMALS[48][3] = {
    // {110} family — 24 systems
    { 0, 1, 1}, { 0, 1, 1}, { 0, 1, 1}, { 0, 1, 1},
    { 0, 1,-1}, { 0, 1,-1}, { 0, 1,-1}, { 0, 1,-1},
    { 1, 0, 1}, { 1, 0, 1}, { 1, 0, 1}, { 1, 0, 1},
    { 1, 0,-1}, { 1, 0,-1}, { 1, 0,-1}, { 1, 0,-1},
    { 1, 1, 0}, { 1, 1, 0}, { 1, 1, 0}, { 1, 1, 0},
    { 1,-1, 0}, { 1,-1, 0}, { 1,-1, 0}, { 1,-1, 0},
    // {112} family — 24 systems
    { 1, 1, 2}, { 1, 1, 2}, { 1, 1,-2}, { 1, 1,-2},
    { 1,-1, 2}, { 1,-1, 2}, {-1, 1, 2}, {-1, 1, 2},
    { 1, 2, 1}, { 1, 2, 1}, { 1, 2,-1}, { 1, 2,-1},
    { 1,-2, 1}, { 1,-2, 1}, {-1, 2, 1}, {-1, 2, 1},
    { 2, 1, 1}, { 2, 1, 1}, { 2, 1,-1}, { 2, 1,-1},
    { 2,-1, 1}, { 2,-1, 1}, {-2, 1, 1}, {-2, 1, 1}
};

const double HillCriterion::SLIP_DIRECTIONS[48][3] = {
    // {110} directions (b·n=0 verified)
    {-1,-1, 1}, {-1, 1,-1}, { 1,-1, 1}, { 1, 1,-1},
    {-1,-1,-1}, {-1, 1, 1}, { 1,-1,-1}, { 1, 1, 1},
    {-1,-1, 1}, {-1, 1, 1}, { 1,-1,-1}, { 1, 1,-1},
    {-1,-1,-1}, {-1, 1,-1}, { 1,-1, 1}, { 1, 1, 1},
    {-1, 1,-1}, {-1, 1, 1}, { 1,-1,-1}, { 1,-1, 1},
    {-1,-1,-1}, {-1,-1, 1}, { 1, 1,-1}, { 1, 1, 1},
    // {112} directions (b·n=0 verified)
    {-1,-1, 1}, { 1, 1,-1}, {-1,-1,-1}, { 1, 1, 1},
    {-1, 1, 1}, { 1,-1,-1}, {-1, 1,-1}, { 1,-1, 1},
    {-1, 1,-1}, { 1,-1, 1}, {-1, 1, 1}, { 1,-1,-1},
    {-1,-1,-1}, { 1, 1, 1}, {-1,-1, 1}, { 1, 1,-1},
    {-1, 1, 1}, { 1,-1,-1}, {-1, 1,-1}, { 1,-1, 1},
    {-1,-1, 1}, { 1, 1,-1}, {-1,-1,-1}, { 1, 1, 1}
};

void HillCriterion::eulerToBungeMatrix(double phi1, double Phi, double phi2, double R[3][3]) {
    double c1 = std::cos(phi1), s1 = std::sin(phi1);
    double c  = std::cos(Phi),  s  = std::sin(Phi);
    double c2 = std::cos(phi2), s2 = std::sin(phi2);
    R[0][0] = c1*c2 - s1*s2*c;  R[0][1] = s1*c2 + c1*s2*c;  R[0][2] = s2*s;
    R[1][0] = -c1*s2 - s1*c2*c; R[1][1] = -s1*s2 + c1*c2*c; R[1][2] = c2*s;
    R[2][0] = s1*s;             R[2][1] = -c1*s;            R[2][2] = c;
}

double HillCriterion::resolvedShearStress(const double stress[3][3], const double n[3], const double b[3]) {
    auto normalize = [](const double v[3], double out[3]) {
        double len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
        if (len < 1e-12) { out[0]=out[1]=out[2]=0; return; }
        out[0]=v[0]/len; out[1]=v[1]/len; out[2]=v[2]/len;
    };
    double nn[3], bb[3];
    normalize(n, nn);
    normalize(b, bb);
    double tau = 0.0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            tau += bb[i] * stress[i][j] * nn[j];
    return tau;
}

std::vector<std::array<double,6>> HillCriterion::computeYieldPoints(
    ansysWrapper* wr, const std::vector<std::vector<float>>& local_cs, short int N, int32_t ***voxels)
{
    std::vector<std::array<double,6>> yield_points;
    int num_steps = (int)wr->eps_as_loading.size();
    if (num_steps == 0 || local_cs.empty()) return yield_points;

    for (int ls = 1; ls <= num_steps; ++ls) {
        wr->load_loadstep(ls);
        auto& results = wr->loadstep_results;
        if (results.empty()) continue;

        double max_tau = 0.0;
        int max_elem_idx = 0;

        for (int e = 0; e < (int)results.size(); ++e) {
            int elem_id = (int)results[e][ID];
            int idx = elem_id - 1;
            int iz = idx / (N * N);
            int iy = (idx / N) % N;
            int ix = idx % N;

            int grain_id = voxels[ix][iy][iz];
            int array_idx = grain_id - 1;

            if (array_idx < 0 || array_idx >= (int)local_cs.size()) continue;

            double phi1 = local_cs[array_idx][0], Phi = local_cs[array_idx][1], phi2 = local_cs[array_idx][2];
            double R[3][3]; eulerToBungeMatrix(phi1, Phi, phi2, R);

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

            for (int sys = 0; sys < NUM_SLIP_SYSTEMS; ++sys) {
                double tau = std::abs(resolvedShearStress(sigma_l, SLIP_NORMALS[sys], SLIP_DIRECTIONS[sys]));
                if (tau > max_tau) { max_tau = tau; max_elem_idx = e; }
            }
        }

        if (max_tau < 1e-9) continue;
        double k = CRSS / max_tau;

        std::array<double,6> sigma_yield = {
            results[max_elem_idx][SX]*k, results[max_elem_idx][SY]*k, results[max_elem_idx][SZ]*k,
            results[max_elem_idx][SXY]*k, results[max_elem_idx][SYZ]*k, results[max_elem_idx][SXZ]*k
        };

        // Remove hydrostatic component to create pure deviator
        double hydro = (sigma_yield[0] + sigma_yield[1] + sigma_yield[2]) / 3.0;
        sigma_yield[0] -= hydro;
        sigma_yield[1] -= hydro;
        sigma_yield[2] -= hydro;

        yield_points.push_back(sigma_yield);
    }
    return yield_points;
}

// 6D Voigt to 5D Lequeu/Deviator converters
void HillCriterion::sigma6D_to_v5D(const double s[6], double v[5]) {
    v[0] = (s[0] - s[1]) / std::sqrt(2.0);
    v[1] = (2.0*s[2] - s[0] - s[1]) / std::sqrt(6.0);
    v[2] = std::sqrt(2.0) * s[3];
    v[3] = std::sqrt(2.0) * s[4];
    v[4] = std::sqrt(2.0) * s[5];
}

void HillCriterion::v5D_to_sigma6D(const double v[5], double s[6]) {
    s[0] =  v[0]/std::sqrt(2.0) - v[1]/std::sqrt(6.0);
    s[1] = -v[0]/std::sqrt(2.0) - v[1]/std::sqrt(6.0);
    s[2] =  std::sqrt(2.0/3.0) * v[1];
    s[3] = v[2] / std::sqrt(2.0);
    s[4] = v[3] / std::sqrt(2.0);
    s[5] = v[4] / std::sqrt(2.0);
}

void HillCriterion::project5Dto6D_Matrix() {
    double T[5][6] = {0};
    T[0][0] = 1.0/std::sqrt(2.0);  T[0][1] = -1.0/std::sqrt(2.0);
    T[1][0] = -1.0/std::sqrt(6.0); T[1][1] = -1.0/std::sqrt(6.0); T[1][2] = 2.0/std::sqrt(6.0);
    T[2][3] = std::sqrt(2.0);
    T[3][4] = std::sqrt(2.0);
    T[4][5] = std::sqrt(2.0);

    for(int i=0; i<6; ++i)
        for(int j=0; j<6; ++j) {
            m_P_Hill[i][j] = 0;
            for(int k=0; k<5; ++k)
                for(int l=0; l<5; ++l)
                    m_P_Hill[i][j] += T[k][i] * m_P_Hill_5D[k][l] * T[l][j];
        }
}

void HillCriterion::project6Dto5D_Matrix() {
    double T[5][6] = {0};
    T[0][0] = 1.0/std::sqrt(2.0);  T[0][1] = -1.0/std::sqrt(2.0);
    T[1][0] = -1.0/std::sqrt(6.0); T[1][1] = -1.0/std::sqrt(6.0); T[1][2] = 2.0/std::sqrt(6.0);
    T[2][3] = std::sqrt(2.0);
    T[3][4] = std::sqrt(2.0);
    T[4][5] = std::sqrt(2.0);

    for(int i=0; i<5; ++i)
        for(int j=0; j<5; ++j) {
            m_P_Hill_5D[i][j] = 0;
            for(int k=0; k<6; ++k)
                for(int l=0; l<6; ++l)
                    m_P_Hill_5D[i][j] += T[i][k] * m_P_Hill[k][l] * T[j][l];
        }
}

// Least squares fit in 5D space (15 parameters)
bool HillCriterion::fit(const std::vector<std::array<double,6>>& yield_points) {
    m_isValid = false;
    int N = (int)yield_points.size();
    if (N < 15) return false;

    double scale = 1.0;
    for (const auto& pt : yield_points)
        for (int i = 0; i < 6; ++i)
            scale = std::max(scale, std::abs(pt[i]));
    if (scale < 1e-12) scale = 1.0;

    auto param_idx = [](int i, int j) -> int {
        if (i > j) std::swap(i, j);
        int idx = 0;
        for (int r = 0; r < i; ++r) idx += (5 - r);
        idx += (j - i);
        return idx;
    };

    double ATA[15][15] = {0};
    double ATb[15]     = {0};

    for (int n = 0; n < N; ++n) {
        double s[6];
        for(int i=0; i<6; ++i) s[i] = yield_points[n][i] / scale;

        // Project 6D stress to 5D Lequeu vector
        double v[5];
        sigma6D_to_v5D(s, v);

        double A_row[15] = {0};
        for (int i = 0; i < 5; ++i) {
            for (int j = i; j < 5; ++j) {
                double val = v[i] * v[j];
                if (i != j) val *= 2.0;
                A_row[param_idx(i,j)] += val;
            }
        }

        for (int i = 0; i < 15; ++i) {
            for (int j = 0; j < 15; ++j) ATA[i][j] += A_row[i] * A_row[j];
            ATb[i] += A_row[i] * 1.0;
        }
    }

    double x[15] = {0};

    for (int i = 0; i < 15; ++i)
        ATA[i][i] += 1e-3 * ATA[i][i];

    if (!solveSystem15x15(ATA, ATb, x)) return false;

    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 5; ++j)
            m_P_Hill_5D[i][j] = x[param_idx(i,j)] / (scale * scale);

    double shift = 1e-6 / (scale * scale);
    for (int i = 0; i < 5; ++i) {
        m_P_Hill_5D[i][i] += shift;
    }

    if (!computeCholesky5D()) {
        qWarning() << "HillCriterion::fit: P_Hill_5D is not positive definite.";
        return false;
    }

    // Reconstruct 6x6 matrix for HDF5 export
    project5Dto6D_Matrix();
    m_isValid = true;
    return true;
}

bool HillCriterion::computeCholesky5D() {
    memset(m_L_5D,    0, sizeof(m_L_5D));
    memset(m_Linv_5D, 0, sizeof(m_Linv_5D));

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j <= i; ++j) {
            double sum = 0;
            for (int k = 0; k < j; ++k) sum += m_L_5D[i][k] * m_L_5D[j][k];
            if (i == j) {
                double val = m_P_Hill_5D[i][i] - sum;
                if (val <= 1e-15) return false;
                m_L_5D[i][j] = std::sqrt(val);
            } else {
                m_L_5D[i][j] = (m_P_Hill_5D[i][j] - sum) / m_L_5D[j][j];
            }
        }
    }

    for (int i = 0; i < 5; ++i) {
        m_Linv_5D[i][i] = 1.0 / m_L_5D[i][i];
        for (int j = 0; j < i; ++j) {
            double sum = 0;
            for (int k = j; k < i; ++k) sum += m_L_5D[i][k] * m_Linv_5D[k][j];
            m_Linv_5D[i][j] = -sum / m_L_5D[i][i];
        }
    }
    return true;
}

std::vector<std::vector<double>> HillCriterion::generateLoads(
    int num_samples, double strain_val, const double S[6][6])
{
    std::vector<std::vector<double>> load_cases;
    if (!m_isValid) return load_cases;

    std::mt19937 gen(Parameters::seed);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int n = 0; n < num_samples; ++n) {
        // 1. Generate 5D sphere
        double d[5], norm_sq = 0.0;
        for (int i = 0; i < 5; ++i) { d[i] = dist(gen); norm_sq += d[i]*d[i]; }
        double norm = std::sqrt(norm_sq);
        if (norm < 1e-12) { --n; continue; }
        for (int i = 0; i < 5; ++i) d[i] /= norm;

        // 2. Vector on 5D ellipsoid
        double d_v[5] = {0};
        for (int i = 0; i < 5; ++i)
            for (int j = i; j < 5; ++j)
                d_v[i] += m_Linv_5D[j][i] * d[j];

        // 3. Return to 6D strictly deviatoric stress vector
        double d_sigma[6] = {0};
        v5D_to_sigma6D(d_v, d_sigma);

        // 4. Strain tensor
        double d_eps[6] = {0}, eps_norm_sq = 0.0;
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) d_eps[i] += S[i][j] * d_sigma[j];
            eps_norm_sq += d_eps[i] * d_eps[i];
        }
        double eps_norm = std::sqrt(eps_norm_sq);
        if (eps_norm < 1e-30) { --n; continue; }

        std::vector<double> eps(6);
        for (int i = 0; i < 6; ++i) eps[i] = (d_eps[i] / eps_norm) * strain_val;
        load_cases.push_back(eps);
    }
    return load_cases;
}

// HDF5 Export/Import
void HillCriterion::saveToHDF5(HDF5Wrapper& hdf5, const std::string& prefix) {
    std::vector<std::vector<float>> mat(6, std::vector<float>(6));
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            mat[i][j] = static_cast<float>(m_P_Hill[i][j]);
    hdf5.write(prefix, "P_Hill", mat);
}

bool HillCriterion::loadFromHDF5(HDF5Wrapper& hdf5, const std::string& prefix) {
    try {
        auto mat = hdf5.readVectorVectorFloat(prefix, "P_Hill");
        if (mat.empty() || mat.size() != 6 || mat[0].size() != 6) return false;

        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j)
                m_P_Hill[i][j] = static_cast<double>(mat[i][j]);

        project6Dto5D_Matrix();
        if (!computeCholesky5D()) return false;

        m_isValid = true;
        return true;
    } catch (...) { return false; }
}

// Solve 15x15 system using Gaussian elimination with partial pivoting
bool HillCriterion::solveSystem15x15(double A[15][15], double b[15], double x[15]) {
    const int N = 15;
    double M[15][16];

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

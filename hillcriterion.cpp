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

// ─────────────────────────────────────────────────────────────────────────────
//  Euler angles -> Bunge rotation matrix
//  phi1, Phi, phi2 — Euler angles (radians)
//  R[3][3]         — rotation matrix from global to crystal frame
// ─────────────────────────────────────────────────────────────────────────────
void HillCriterion::eulerToBungeMatrix(double phi1, double Phi, double phi2, double R[3][3]) {
    double c1 = std::cos(phi1), s1 = std::sin(phi1);
    double c  = std::cos(Phi),  s  = std::sin(Phi);
    double c2 = std::cos(phi2), s2 = std::sin(phi2);
    R[0][0] = c1*c2 - s1*s2*c;  R[0][1] = s1*c2 + c1*s2*c;  R[0][2] = s2*s;
    R[1][0] = -c1*s2 - s1*c2*c; R[1][1] = -s1*s2 + c1*c2*c; R[1][2] = c2*s;
    R[2][0] = s1*s;             R[2][1] = -c1*s;            R[2][2] = c;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Project stress sigma onto slip system (n, b) -> tau = b·sigma·n
// ─────────────────────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────────────────────
//  Compute yield points using the Schmid criterion (CRSS)
//  For each load step, finds the element with the highest tau
//  and scales its stress tensor so that tau = CRSS
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::array<double,6>> HillCriterion::computeYieldPoints(
    ansysWrapper* wr, const std::vector<std::vector<float>>& local_cs, short int N, int32_t ***voxels)
{
    qDebug() << "\n[HillCriterion::computeYieldPoints] ─────────────────────────────";
    qDebug() << "[HillCriterion::computeYieldPoints] Computing yield points";
    qDebug() << "[HillCriterion::computeYieldPoints]   Load steps        :" << (int)wr->eps_as_loading.size();
    qDebug() << "[HillCriterion::computeYieldPoints]   Grains (local_cs) :" << (int)local_cs.size();
    qDebug() << "[HillCriterion::computeYieldPoints]   Grid size N       :" << N;
    qDebug() << "[HillCriterion::computeYieldPoints]   CRSS              :" << CRSS << "Pa";
    qDebug() << "[HillCriterion::computeYieldPoints]   BCC slip systems  :" << NUM_SLIP_SYSTEMS;

    std::vector<std::array<double,6>> yield_points;
    int num_steps = (int)wr->eps_as_loading.size();
    if (num_steps == 0 || local_cs.empty()) {
        qWarning() << "[HillCriterion::computeYieldPoints] SKIP: no load steps or no grains";
        return yield_points;
    }

    int skipped_loadsteps = 0;
    int skipped_elements  = 0;

    for (int ls = 1; ls <= num_steps; ++ls) {
        qDebug() << QString("[HillCriterion::computeYieldPoints] --- Load step %1 / %2 ---").arg(ls).arg(num_steps);

        wr->load_loadstep(ls);
        auto& results = wr->loadstep_results;

        if (results.empty()) {
            qWarning() << QString("[HillCriterion::computeYieldPoints]   Step %1: results are empty, skipping").arg(ls);
            ++skipped_loadsteps;
            continue;
        }

        qDebug() << QString("[HillCriterion::computeYieldPoints]   Elements in step: %1").arg((int)results.size());

        double max_tau      = 0.0;
        int    max_elem_idx = 0;
        int    elem_skipped = 0;

        for (int e = 0; e < (int)results.size(); ++e) {
            int elem_id  = (int)results[e][ID];
            int idx      = elem_id - 1;
            int iz       = idx / (N * N);
            int iy       = (idx / N) % N;
            int ix       = idx % N;

            int grain_id  = voxels[ix][iy][iz];
            int array_idx = grain_id - 1;

            if (array_idx < 0 || array_idx >= (int)local_cs.size()) {
                ++elem_skipped;
                ++skipped_elements;
                continue;
            }

            // Rotate stress tensor from global frame into crystal frame
            double phi1 = local_cs[array_idx][0], Phi = local_cs[array_idx][1], phi2 = local_cs[array_idx][2];
            double R[3][3];
            eulerToBungeMatrix(phi1, Phi, phi2, R);

            double sigma_g[3][3] = {
                {results[e][SX],  results[e][SXY], results[e][SXZ]},
                {results[e][SXY], results[e][SY],  results[e][SYZ]},
                {results[e][SXZ], results[e][SYZ], results[e][SZ] }
            };

            // sigma_local = R * sigma_global * R^T
            double sigma_l[3][3] = {0};
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    for (int k = 0; k < 3; ++k)
                        for (int l = 0; l < 3; ++l)
                            sigma_l[i][j] += R[i][k] * sigma_g[k][l] * R[j][l];

            // Loop over all slip systems, track maximum |tau|
            for (int sys = 0; sys < NUM_SLIP_SYSTEMS; ++sys) {
                double tau = std::abs(resolvedShearStress(sigma_l, SLIP_NORMALS[sys], SLIP_DIRECTIONS[sys]));
                if (tau > max_tau) {
                    max_tau      = tau;
                    max_elem_idx = e;
                }
            }
        }

        if (elem_skipped > 0)
            qDebug() << QString("[HillCriterion::computeYieldPoints]   Elements skipped (invalid grain_id): %1").arg(elem_skipped);

        if (max_tau < 1e-9) {
            qWarning() << QString("[HillCriterion::computeYieldPoints]   Step %1: max tau < 1e-9 (~0), no yield point created").arg(ls);
            continue;
        }

        // Scale factor: sigma_yield = sigma * (CRSS / tau_max)
        double k = CRSS / max_tau;
        qDebug() << QString("[HillCriterion::computeYieldPoints]   Step %1: tau_max = %2 Pa  (element idx=%3)")
                        .arg(ls).arg(max_tau, 0, 'e', 4).arg(max_elem_idx);
        qDebug() << QString("[HillCriterion::computeYieldPoints]   Scale k = CRSS/tau_max = %1 / %2 = %3")
                        .arg(CRSS, 0, 'e', 4).arg(max_tau, 0, 'e', 4).arg(k, 0, 'f', 6);

        std::array<double,6> sigma_yield = {
            results[max_elem_idx][SX]*k, results[max_elem_idx][SY]*k, results[max_elem_idx][SZ]*k,
            results[max_elem_idx][SXY]*k, results[max_elem_idx][SYZ]*k, results[max_elem_idx][SXZ]*k
        };

        // Subtract hydrostatic part -> pure deviator
        double hydro = (sigma_yield[0] + sigma_yield[1] + sigma_yield[2]) / 3.0;
        qDebug() << QString("[HillCriterion::computeYieldPoints]   Hydrostatic p = (sx+sy+sz)/3 = %1 Pa (subtracted)").arg(hydro, 0, 'e', 4);
        sigma_yield[0] -= hydro;
        sigma_yield[1] -= hydro;
        sigma_yield[2] -= hydro;

        qDebug() << QString("[HillCriterion::computeYieldPoints]   Yield point #%1 [sx=%2  sy=%3  sz=%4  txy=%5  tyz=%6  txz=%7] (Pa)")
                        .arg(yield_points.size()+1)
                        .arg(sigma_yield[0],'e',3).arg(sigma_yield[1],'e',3).arg(sigma_yield[2],'e',3)
                        .arg(sigma_yield[3],'e',3).arg(sigma_yield[4],'e',3).arg(sigma_yield[5],'e',3);

        yield_points.push_back(sigma_yield);
    }

    qDebug() << "[HillCriterion::computeYieldPoints] ─────────────────────────────";
    qDebug() << "[HillCriterion::computeYieldPoints] Total yield points collected :" << (int)yield_points.size();
    qDebug() << "[HillCriterion::computeYieldPoints] Load steps skipped (empty)   :" << skipped_loadsteps;
    qDebug() << "[HillCriterion::computeYieldPoints] Elements skipped (total)     :" << skipped_elements;
    qDebug() << "[HillCriterion::computeYieldPoints] ─────────────────────────────\n";

    return yield_points;
}

// ─────────────────────────────────────────────────────────────────────────────
//  6D Voigt (deviatoric) -> 5D Lequeu/Deviatoric
//  Orthonormal representation of the deviatoric subspace
// ─────────────────────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────────────────────
//  Project 5D -> 6D (via transformation matrix T)
//  m_P_Hill[6][6] = T^T * m_P_Hill_5D * T
// ─────────────────────────────────────────────────────────────────────────────
void HillCriterion::project5Dto6D_Matrix() {
    qDebug() << "[HillCriterion::project5Dto6D] Projecting P_Hill_5D(5x5) -> P_Hill(6x6) via T";

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

    qDebug() << "[HillCriterion::project5Dto6D] P_Hill(6x6):";
    for (int i = 0; i < 6; ++i) {
        QString row;
        for (int j = 0; j < 6; ++j)
            row += QString::number(m_P_Hill[i][j], 'e', 3) + "  ";
        qDebug().noquote() << "  [" + QString::number(i) + "]  " + row;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Project 6D -> 5D (inverse operation)
//  m_P_Hill_5D[5][5] = T * m_P_Hill * T^T
// ─────────────────────────────────────────────────────────────────────────────
void HillCriterion::project6Dto5D_Matrix() {
    qDebug() << "[HillCriterion::project6Dto5D] Projecting P_Hill(6x6) -> P_Hill_5D(5x5) via T";

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

    qDebug() << "[HillCriterion::project6Dto5D] P_Hill_5D(5x5):";
    for (int i = 0; i < 5; ++i) {
        QString row;
        for (int j = 0; j < 5; ++j)
            row += QString::number(m_P_Hill_5D[i][j], 'e', 3) + "  ";
        qDebug().noquote() << "  [" + QString::number(i) + "]  " + row;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Least-squares fit of the Hill ellipsoid in 5D deviatoric space
//  Goal: find symmetric matrix P(5x5) such that
//        v^T * P * v = 1  for all yield points v
//  This is an LS problem over 15 independent parameters (upper triangle of P)
// ─────────────────────────────────────────────────────────────────────────────
bool HillCriterion::fit(const std::vector<std::array<double,6>>& yield_points) {
    qDebug() << "\n[HillCriterion::fit] ════════════════════════════════════════════";
    qDebug() << "[HillCriterion::fit] Least-squares fit of Hill ellipsoid in 5D space";
    qDebug() << "[HillCriterion::fit]   Yield points N     :" << (int)yield_points.size();
    qDebug() << "[HillCriterion::fit]   Minimum required   : 15 (number of P parameters)";

    m_isValid = false;
    int N = (int)yield_points.size();
    if (N < 15) {
        qWarning() << "[HillCriterion::fit] ERROR: not enough yield points (" << N << "< 15). Fit impossible.";
        return false;
    }

    // Normalisation scale for numerical stability
    double scale = 1.0;
    for (const auto& pt : yield_points)
        for (int i = 0; i < 6; ++i)
            scale = std::max(scale, std::abs(pt[i]));
    if (scale < 1e-12) scale = 1.0;

    qDebug() << "[HillCriterion::fit]   Normalisation scale =" << scale << "Pa";
    qDebug() << "[HillCriterion::fit]   All stresses divided by scale before assembling ATA";

    // Map upper-triangle parameters (5x5) to linear index [0..14]
    auto param_idx = [](int i, int j) -> int {
        if (i > j) std::swap(i, j);
        int idx = 0;
        for (int r = 0; r < i; ++r) idx += (5 - r);
        idx += (j - i);
        return idx;
    };

    qDebug() << "[HillCriterion::fit] --- Assembling normal system ATA*x = ATb ---";
    qDebug() << "[HillCriterion::fit]   Per point: sigma -> v(5D) -> row of A, b=1";

    double ATA[15][15] = {0};
    double ATb[15]     = {0};

    for (int n = 0; n < N; ++n) {
        double s[6];
        for(int i=0; i<6; ++i) s[i] = yield_points[n][i] / scale;

        // Project deviatoric 6D vector into 5D
        double v[5];
        sigma6D_to_v5D(s, v);

        // Row of A: A_row[param_idx(i,j)] = vi*vj (or 2*vi*vj when i != j)
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

    qDebug() << "[HillCriterion::fit]   ATA and ATb assembled. Diagonal sum of ATA:";
    double diag_norm = 0.0;
    for (int i = 0; i < 15; ++i) diag_norm += ATA[i][i];
    qDebug() << "   sum(diag) =" << diag_norm;

    // Tikhonov regularisation: ATA[i][i] += lambda * ATA[i][i]  (lambda = 1e-3)
    qDebug() << "[HillCriterion::fit] --- Tikhonov regularisation (lambda=1e-3) ---";
    for (int i = 0; i < 15; ++i)
        ATA[i][i] += 1e-3 * ATA[i][i];
    qDebug() << "[HillCriterion::fit]   Diagonal of ATA increased by 0.1% to prevent degeneracy";

    // Solve 15x15 system via Gaussian elimination with partial pivoting
    qDebug() << "[HillCriterion::fit] --- Solving 15x15 system (Gauss + partial pivoting) ---";
    double x[15] = {0};
    if (!solveSystem15x15(ATA, ATb, x)) {
        qWarning() << "[HillCriterion::fit] ERROR: solveSystem15x15 failed (system degenerate?)";
        return false;
    }

    qDebug() << "[HillCriterion::fit]   Solution x[0..14]:";
    for (int i = 0; i < 15; ++i)
        qDebug() << QString("    x[%1] = %2").arg(i, 2).arg(x[i], 0, 'e', 6);

    // Build P_Hill_5D from solution x, account for scale^2
    qDebug() << "[HillCriterion::fit] --- Building P_Hill_5D from x, dividing by scale^2 ---";
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 5; ++j)
            m_P_Hill_5D[i][j] = x[param_idx(i,j)] / (scale * scale);

    // Small diagonal shift to guarantee positive definiteness
    double shift = 1e-6 / (scale * scale);
    qDebug() << "[HillCriterion::fit]   Diagonal shift eps = 1e-6/scale^2 =" << shift << "(ensures PD)";
    for (int i = 0; i < 5; ++i)
        m_P_Hill_5D[i][i] += shift;

    qDebug() << "\n[HillCriterion::fit] === P_Hill_5D before Cholesky decomposition ===";
    for (int i = 0; i < 5; ++i) {
        QString row;
        for (int j = 0; j < 5; ++j)
            row += QString::number(m_P_Hill_5D[i][j], 'e', 4) + "  ";
        qDebug().noquote() << "  [" + QString::number(i) + "]  " + row;
    }
    qDebug() << "[HillCriterion::fit] ═══════════════════════════════════════════";

    // Cholesky: verifies that P_5D is symmetric and positive definite
    qDebug() << "[HillCriterion::fit] --- Cholesky decomposition P_5D = L*L^T ---";
    if (!computeCholesky5D()) {
        qWarning() << "[HillCriterion::fit] ERROR: P_Hill_5D is not positive definite. Fit failed.";
        return false;
    }
    qDebug() << "[HillCriterion::fit]   Cholesky succeeded. P_Hill_5D is valid.";

    // Project back to 6D for HDF5 export
    qDebug() << "[HillCriterion::fit] --- Projecting 5D -> 6D for HDF5 export ---";
    project5Dto6D_Matrix();

    m_isValid = true;
    qDebug() << "[HillCriterion::fit] Fit COMPLETED SUCCESSFULLY. m_isValid = true\n";
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cholesky decomposition P_5D = L*L^T
//  Also computes L^{-1} for subsequent load generation
// ─────────────────────────────────────────────────────────────────────────────
bool HillCriterion::computeCholesky5D() {
    qDebug() << "[HillCriterion::computeCholesky5D] Starting Cholesky decomposition (5x5)";

    memset(m_L_5D,    0, sizeof(m_L_5D));
    memset(m_Linv_5D, 0, sizeof(m_Linv_5D));

    // Dynamic tolerance based on the largest diagonal entry
    double max_diag = 0.0;
    for (int i = 0; i < 5; ++i)
        max_diag = std::max(max_diag, m_P_Hill_5D[i][i]);
    double tol = max_diag * 1e-12;

    qDebug() << "[HillCriterion::computeCholesky5D]   Max diagonal of P_5D =" << max_diag;
    qDebug() << "[HillCriterion::computeCholesky5D]   Dynamic tolerance tol = max_diag*1e-12 =" << tol;

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j <= i; ++j) {
            double sum = 0;
            for (int k = 0; k < j; ++k) sum += m_L_5D[i][k] * m_L_5D[j][k];

            if (i == j) {
                double val = m_P_Hill_5D[i][i] - sum;

                qDebug() << QString("[HillCriterion::computeCholesky5D]   L[%1][%1]: val = P[%1][%1] - sum(L^2) = %2 - %3 = %4  (tol %5)")
                                .arg(i).arg(m_P_Hill_5D[i][i],0,'e',4).arg(sum,0,'e',4).arg(val,0,'e',4).arg(tol,0,'e',2);

                if (val <= tol) {
                    qWarning() << "\n[HillCriterion::computeCholesky5D] === FAILED ===";
                    qWarning() << "[HillCriterion::computeCholesky5D]   Row/col            :" << i;
                    qWarning() << "[HillCriterion::computeCholesky5D]   val (must be > tol) =" << val;
                    qWarning() << "[HillCriterion::computeCholesky5D]   tol                 =" << tol;
                    qWarning() << "[HillCriterion::computeCholesky5D]   Matrix is NOT positive definite";
                    return false;
                }
                m_L_5D[i][j] = std::sqrt(val);
                qDebug() << QString("[HillCriterion::computeCholesky5D]     -> L[%1][%1] = sqrt(%2) = %3")
                                .arg(i).arg(val,0,'e',4).arg(m_L_5D[i][j],0,'e',4);
            } else {
                m_L_5D[i][j] = (m_P_Hill_5D[i][j] - sum) / m_L_5D[j][j];
            }
        }
    }

    qDebug() << "[HillCriterion::computeCholesky5D] L decomposition succeeded. Computing L^{-1}...";

    // Compute inverse of lower-triangular matrix L
    for (int i = 0; i < 5; ++i) {
        m_Linv_5D[i][i] = 1.0 / m_L_5D[i][i];
        for (int j = 0; j < i; ++j) {
            double sum = 0;
            for (int k = j; k < i; ++k) sum += m_L_5D[i][k] * m_Linv_5D[k][j];
            m_Linv_5D[i][j] = -sum / m_L_5D[i][i];
        }
    }

    qDebug() << "[HillCriterion::computeCholesky5D] L^{-1} computed. Diagonal of L:";
    for (int i = 0; i < 5; ++i)
        qDebug() << QString("    L[%1][%1] = %2  ->  L^{-1}[%1][%1] = %3")
                        .arg(i).arg(m_L_5D[i][i],0,'e',4).arg(m_Linv_5D[i][i],0,'e',4);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Generate random strain load cases on the Hill ellipsoid surface
//  Algorithm:
//    1. Uniform sampling on a 5D unit sphere (normal distribution trick)
//    2. Map to 5D ellipsoid via L^{-T}: d_v = L^{-T} * d
//    3. Convert to 6D deviatoric stress
//    4. Convert to strain via elastic compliance tensor S
//    5. Normalise to target strain_val
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::vector<double>> HillCriterion::generateLoads(
    int num_samples, double strain_val, const double S[6][6])
{
    qDebug() << "\n[HillCriterion::generateLoads] ─────────────────────────────────";
    qDebug() << "[HillCriterion::generateLoads] Generating HYBRID loads on the Hill ellipsoid";
    qDebug() << "[HillCriterion::generateLoads]   Requested samples :" << num_samples;

    std::vector<std::vector<double>> load_cases;
    if (!m_isValid) return load_cases;

    // =========================================================================
    // PART 1: DETERMINISTIC DIRECTIONS (Filling the "holes" on the yield surface facets)
    // =========================================================================

    // Define canonical stress states {sx, sy, sz, txy, tyz, txz}
    std::vector<std::array<double, 6>> canonical_dirs = {
        // Uniaxial tension/compression (Targeting the centers of the flat facets)
        { 1, 0, 0, 0, 0, 0}, {-1, 0, 0, 0, 0, 0},
        { 0, 1, 0, 0, 0, 0}, { 0,-1, 0, 0, 0, 0},
        { 0, 0, 1, 0, 0, 0}, { 0, 0,-1, 0, 0, 0},

        // Biaxial normal loading (Targeting the edges of the facets)
        { 1, 1, 0, 0, 0, 0}, {-1,-1, 0, 0, 0, 0}, { 1,-1, 0, 0, 0, 0}, {-1, 1, 0, 0, 0, 0},
        { 1, 0, 1, 0, 0, 0}, {-1, 0,-1, 0, 0, 0}, { 1, 0,-1, 0, 0, 0}, {-1, 0, 1, 0, 0, 0},
        { 0, 1, 1, 0, 0, 0}, { 0,-1,-1, 0, 0, 0}, { 0, 1,-1, 0, 0, 0}, { 0,-1, 1, 0, 0, 0},

        // Triaxial normal loading (Deviatoric components only)
        { 1, 1,-0.5, 0,0,0}, {-1,-1, 0.5, 0,0,0},

        // Pure shear states (To firmly anchor the corners/vertices of the yield surface)
        { 0, 0, 0, 1, 0, 0}, { 0, 0, 0,-1, 0, 0},
        { 0, 0, 0, 0, 1, 0}, { 0, 0, 0, 0,-1, 0},
        { 0, 0, 0, 0, 0, 1}, { 0, 0, 0, 0, 0,-1}
    };

    qDebug() << "[HillCriterion::generateLoads]   Injecting" << canonical_dirs.size() << "canonical directions.";

    // Lambda function to process any given 5D unit vector
    auto process_direction = [&](double d[5]) {
        // Project onto 5D ellipsoid surface via L^{-T}
        double d_v[5] = {0};
        for (int i = 0; i < 5; ++i)
            for (int j = i; j < 5; ++j)
                d_v[i] += m_Linv_5D[j][i] * d[j];

        // Convert 5D back to 6D deviatoric stress
        double d_sigma[6] = {0};
        v5D_to_sigma6D(d_v, d_sigma);

        // Convert stress to strain via elastic compliance tensor S
        double d_eps[6] = {0}, eps_norm_sq = 0.0;
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) d_eps[i] += S[i][j] * d_sigma[j];
            eps_norm_sq += d_eps[i] * d_eps[i];
        }

        double eps_norm = std::sqrt(eps_norm_sq);
        if (eps_norm < 1e-30) return false;

        // Normalise to target strain_val
        std::vector<double> eps(6);
        for (int i = 0; i < 6; ++i) eps[i] = (d_eps[i] / eps_norm) * strain_val;

        load_cases.push_back(eps);
        return true;
    };

    // Process all deterministic canonical directions
    for (const auto& s_dir : canonical_dirs) {
        double v[5], norm_sq = 0.0;
        sigma6D_to_v5D(s_dir.data(), v); // Map 6D Voigt stress to 5D Lequeu vector

        for (int i = 0; i < 5; ++i) norm_sq += v[i]*v[i];
        double norm = std::sqrt(norm_sq);

        if (norm > 1e-12) {
            for (int i = 0; i < 5; ++i) v[i] /= norm; // Normalise to unit vector 'd'
            process_direction(v);
        }
    }

    // =========================================================================
    // PART 2: RANDOM DIRECTIONS (Uniformly populating the remaining space)
    // =========================================================================
    std::mt19937 gen(Parameters::seed);
    std::normal_distribution<double> dist(0.0, 1.0);
    int remaining_samples = num_samples - load_cases.size();

    qDebug() << "[HillCriterion::generateLoads]   Generating" << remaining_samples << "random directions.";

    int retries = 0;
    for (int n = 0; n < remaining_samples; ++n) {
        double d[5], norm_sq = 0.0;

        // Generate random unit vector on a 5D sphere
        for (int i = 0; i < 5; ++i) { d[i] = dist(gen); norm_sq += d[i]*d[i]; }

        double norm = std::sqrt(norm_sq);
        if (norm < 1e-12) { --n; ++retries; continue; }

        for (int i = 0; i < 5; ++i) d[i] /= norm;

        // Attempt to process the random direction
        if (!process_direction(d)) {
            --n; ++retries;
        }
    }

    qDebug() << "[HillCriterion::generateLoads]   Total loads generated :" << load_cases.size();
    qDebug() << "[HillCriterion::generateLoads] ─────────────────────────────────\n";

    return load_cases;
}

// ─────────────────────────────────────────────────────────────────────────────
//  HDF5 Export/Import
// ─────────────────────────────────────────────────────────────────────────────
void HillCriterion::saveToHDF5(HDF5Wrapper& hdf5, const std::string& prefix) {
    qDebug() << "[HillCriterion::saveToHDF5] Writing P_Hill(6x6) to HDF5, prefix:" << QString::fromStdString(prefix);

    std::vector<std::vector<float>> mat(6, std::vector<float>(6));
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            mat[i][j] = static_cast<float>(m_P_Hill[i][j]);
    hdf5.write(prefix, "P_Hill", mat);

    qDebug() << "[HillCriterion::saveToHDF5] P_Hill written successfully";
}

bool HillCriterion::loadFromHDF5(HDF5Wrapper& hdf5, const std::string& prefix) {
    qDebug() << "[HillCriterion::loadFromHDF5] Loading P_Hill(6x6) from HDF5, prefix:" << QString::fromStdString(prefix);

    try {
        auto mat = hdf5.readVectorVectorFloat(prefix, "P_Hill");
        if (mat.empty() || mat.size() != 6 || mat[0].size() != 6) {
            qWarning() << "[HillCriterion::loadFromHDF5] ERROR: P_Hill not found or wrong dimensions";
            return false;
        }

        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j)
                m_P_Hill[i][j] = static_cast<double>(mat[i][j]);

        qDebug() << "[HillCriterion::loadFromHDF5] P_Hill loaded. Projecting 6D -> 5D...";
        project6Dto5D_Matrix();

        qDebug() << "[HillCriterion::loadFromHDF5] Running Cholesky on restored P_5D...";
        if (!computeCholesky5D()) {
            qWarning() << "[HillCriterion::loadFromHDF5] ERROR: loaded P_Hill failed Cholesky check";
            return false;
        }

        m_isValid = true;
        qDebug() << "[HillCriterion::loadFromHDF5] Load SUCCESSFUL. m_isValid = true";
        return true;
    } catch (...) {
        qWarning() << "[HillCriterion::loadFromHDF5] EXCEPTION while reading HDF5";
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Solve 15x15 linear system via Gaussian elimination with partial pivoting
// ─────────────────────────────────────────────────────────────────────────────
bool HillCriterion::solveSystem15x15(double A[15][15], double b[15], double x[15]) {
    const int N = 15;
    qDebug() << "[HillCriterion::solveSystem15x15] Gaussian elimination (15x15) with partial pivoting";

    double M[15][16];
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) M[i][j] = A[i][j];
        M[i][N] = b[i];
    }

    for (int i = 0; i < N; ++i) {
        // Find pivot in column i
        int pivot = i;
        for (int j = i + 1; j < N; ++j)
            if (std::abs(M[j][i]) > std::abs(M[pivot][i])) pivot = j;
        for (int k = 0; k <= N; ++k) std::swap(M[i][k], M[pivot][k]);

        if (std::abs(M[i][i]) < 1e-18) {
            qWarning() << QString("[HillCriterion::solveSystem15x15] ERROR: pivot at step %1 is ~0 (%2). Matrix is singular.")
                              .arg(i).arg(M[i][i], 0, 'e', 4);
            return false;
        }

        qDebug() << QString("[HillCriterion::solveSystem15x15]   Step %1/%2: pivot = M[%3][%3] = %4")
                        .arg(i+1).arg(N).arg(i).arg(M[i][i], 0, 'e', 4);

        // Forward elimination: zero out entries below the pivot
        for (int j = i + 1; j < N; ++j) {
            double factor = M[j][i] / M[i][i];
            for (int k = i; k <= N; ++k) M[j][k] -= factor * M[i][k];
        }
    }

    // Back substitution
    qDebug() << "[HillCriterion::solveSystem15x15]   Forward pass complete. Back substitution...";
    for (int i = N - 1; i >= 0; --i) {
        double sum = 0;
        for (int j = i + 1; j < N; ++j) sum += M[i][j] * x[j];
        x[i] = (M[i][N] - sum) / M[i][i];
    }

    qDebug() << "[HillCriterion::solveSystem15x15] Solution obtained successfully";
    return true;
}

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
//  Resolved local criterion / Hill space (Parameters::resolveYieldSettings)
// ─────────────────────────────────────────────────────────────────────────────
void HillCriterion::setYieldSettings(const QString& localCriterion, const QString& hillSpace, double yieldStress) {
    m_vonMises    = localCriterion.trimmed().toLower() == "vonmises";
    m_full6D      = hillSpace.trimmed().toLower() == "full6d";
    m_yieldStress = yieldStress;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Von Mises equivalent stress, s = [sx, sy, sz, sxy, syz, sxz] (global frame)
// ─────────────────────────────────────────────────────────────────────────────
double HillCriterion::vonMisesStress(const double s[6]) {
    const double dxy = s[0] - s[1], dyz = s[1] - s[2], dzx = s[2] - s[0];
    return std::sqrt(0.5 * (dxy*dxy + dyz*dyz + dzx*dzx)
                     + 3.0 * (s[3]*s[3] + s[4]*s[4] + s[5]*s[5]));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Compute yield points using the Schmid criterion (CRSS)
//  or the von Mises criterion (yield_stress), see setYieldSettings()
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
    if (m_vonMises) {
        qDebug() << "[HillCriterion::computeYieldPoints]   Criterion         : von Mises (global frame)";
        qDebug() << "[HillCriterion::computeYieldPoints]   Yield stress      :" << m_yieldStress << "Pa";
    } else {
        qDebug() << "[HillCriterion::computeYieldPoints]   CRSS              :" << CRSS << "Pa";
        qDebug() << "[HillCriterion::computeYieldPoints]   BCC slip systems  :" << NUM_SLIP_SYSTEMS;
    }
    qDebug() << "[HillCriterion::computeYieldPoints]   Hill space        :" << (m_full6D ? "full6d" : "deviatoric5d");

    std::vector<std::array<double,6>> yield_points;
    int num_steps = (int)wr->eps_as_loading.size();
    // von Mises needs neither local_cs nor grain ids (porous / multi-phase cells)
    if (num_steps == 0 || (!m_vonMises && local_cs.empty())) {
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

        struct ElemTau { int idx; double tau; };
        std::vector<ElemTau> tau_list;
        tau_list.reserve(results.size());

        int elem_skipped = 0;

        for (int e = 0; e < (int)results.size(); ++e) {
            if (m_vonMises) {
                const double s6[6] = { results[e][SX],  results[e][SY],  results[e][SZ],
                                       results[e][SXY], results[e][SYZ], results[e][SXZ] };
                tau_list.push_back({e, vonMisesStress(s6)});
                continue;
            }

            int elem_id  = (int)results[e][ID];
            int ansys_index = elem_id - 1;
            if (ansys_index < 0 || ansys_index >= (int)wr->ansys_to_voxel_map.size()) {
                ++elem_skipped;
                ++skipped_elements;
                continue;
            }

            int idx = wr->ansys_to_voxel_map[ansys_index];
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

            double phi1 = local_cs[array_idx][0], Phi = local_cs[array_idx][1], phi2 = local_cs[array_idx][2];
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

            double elem_max_tau = 0.0;
            for (int sys = 0; sys < NUM_SLIP_SYSTEMS; ++sys) {
                double tau = std::abs(resolvedShearStress(sigma_l, SLIP_NORMALS[sys], SLIP_DIRECTIONS[sys]));
                if (tau > elem_max_tau) {
                    elem_max_tau = tau;
                }
            }

            tau_list.push_back({e, elem_max_tau});
        }

        if (tau_list.empty()) continue;

        std::sort(tau_list.begin(), tau_list.end(), [](const ElemTau& a, const ElemTau& b) {
            return a.tau < b.tau;
        });

        double percentile_threshold = 0.98;

        int p_index = static_cast<int>(tau_list.size() * percentile_threshold);
        if (p_index >= static_cast<int>(tau_list.size()))
            p_index = static_cast<int>(tau_list.size()) - 1;

        double max_tau = tau_list[p_index].tau;
        int max_elem_idx = tau_list[p_index].idx;

        if (elem_skipped > 0)
            qDebug() << QString("[HillCriterion::computeYieldPoints]   Elements skipped (invalid grain_id): %1").arg(elem_skipped);

        if (max_tau < 10.0) {
            qWarning() << QString("[HillCriterion::computeYieldPoints]   Step %1: max tau is too small (%2), skipping to prevent math explosion").arg(ls).arg(max_tau);
            continue;
        }

        // Scale factor: sigma_yield = sigma * (threshold / tau_max),
        // threshold = CRSS (Schmid) or yield_stress (von Mises)
        const double threshold = m_vonMises ? m_yieldStress : CRSS;
        double k = threshold / max_tau;
        qDebug() << QString("[HillCriterion::computeYieldPoints]   Step %1: %2_max = %3 Pa  (element idx=%4)")
                        .arg(ls).arg(m_vonMises ? "vm" : "tau").arg(max_tau, 0, 'e', 4).arg(max_elem_idx);
        qDebug() << QString("[HillCriterion::computeYieldPoints]   Scale k = %1/%2_max = %3 / %4 = %5")
                        .arg(m_vonMises ? "yield_stress" : "CRSS").arg(m_vonMises ? "vm" : "tau")
                        .arg(threshold, 0, 'e', 4).arg(max_tau, 0, 'e', 4).arg(k, 0, 'f', 6);

        double macro_sx  = wr->loadstep_results_avg[SX]  * wr->m_solid_fraction;
        double macro_sy  = wr->loadstep_results_avg[SY]  * wr->m_solid_fraction;
        double macro_sz  = wr->loadstep_results_avg[SZ]  * wr->m_solid_fraction;
        double macro_sxy = wr->loadstep_results_avg[SXY] * wr->m_solid_fraction;
        double macro_syz = wr->loadstep_results_avg[SYZ] * wr->m_solid_fraction;
        double macro_sxz = wr->loadstep_results_avg[SXZ] * wr->m_solid_fraction;

        std::array<double,6> sigma_yield = {
            macro_sx * k, macro_sy * k, macro_sz * k,
            macro_sxy * k, macro_syz * k, macro_sxz * k
        };

        // Subtract hydrostatic part -> pure deviator (full6d keeps it: pressure-sensitive)
        if (!m_full6D) {
            double hydro = (sigma_yield[0] + sigma_yield[1] + sigma_yield[2]) / 3.0;
            qDebug() << QString("[HillCriterion::computeYieldPoints]   Hydrostatic p = (sx+sy+sz)/3 = %1 Pa (subtracted)").arg(hydro, 0, 'e', 4);
            sigma_yield[0] -= hydro;
            sigma_yield[1] -= hydro;
            sigma_yield[2] -= hydro;
        }

        qDebug() << QString("[HillCriterion::computeYieldPoints]   Yield point #%1 [sx=%2  sy=%3  sz=%4  txy=%5  tyz=%6  txz=%7] (Pa)")
                        .arg(yield_points.size()+1)
                        .arg(sigma_yield[0],0,'e',3).arg(sigma_yield[1],0,'e',3).arg(sigma_yield[2],0,'e',3)
                        .arg(sigma_yield[3],0,'e',3).arg(sigma_yield[4],0,'e',3).arg(sigma_yield[5],0,'e',3);

        yield_points.push_back(sigma_yield);
    }

    qDebug() << "[HillCriterion::computeYieldPoints] ─────────────────────────────";
    qDebug() << "[HillCriterion::computeYieldPoints] Total yield points collected :" << (int)yield_points.size();
    qDebug() << "[HillCriterion::computeYieldPoints] Load steps skipped (empty)   :" << skipped_loadsteps;
    qDebug() << "[HillCriterion::computeYieldPoints] Elements skipped (total)     :" << skipped_elements;
    qDebug() << "[HillCriterion::computeYieldPoints] ─────────────────────────────\n";

    return yield_points;
}


std::optional<std::array<double,6>> HillCriterion::computeYieldPointForStep(
    const std::vector<std::array<double,6>>& voxel_stress,
    const std::vector<int>&                  voxel_grain,
    const std::array<double,6>&              macro_stress,
    const std::vector<std::vector<float>>&   local_cs)
{
    const int nv = static_cast<int>(voxel_stress.size());
    if (nv == 0 || (!m_vonMises && local_cs.empty())) return std::nullopt;

    struct ElemTau { int idx; double tau; };
    std::vector<ElemTau> tau_list;
    tau_list.reserve(nv);

    for (int e = 0; e < nv; ++e) {
        if (m_vonMises) {
            // global frame, no orientation / grain id needed
            tau_list.push_back({ e, vonMisesStress(voxel_stress[e].data()) });
            continue;
        }

        const int grain_id = voxel_grain[e];
        if (grain_id < 0 || grain_id >= (int)local_cs.size()) continue;  // guard

        const double phi1 = local_cs[grain_id][0];
        const double Phi  = local_cs[grain_id][1];
        const double phi2 = local_cs[grain_id][2];
        double R[3][3];
        eulerToBungeMatrix(phi1, Phi, phi2, R);

        // pipeline stress [sx,sy,sz,sxy,syz,sxz] -> 3x3 (global/sample frame)
        const auto& s = voxel_stress[e];
        const double sigma_g[3][3] = {
            { s[0], s[3], s[5] },   // sx  sxy sxz
            { s[3], s[1], s[4] },   // sxy sy  syz
            { s[5], s[4], s[2] }    // sxz syz sz
        };

        // rotate into the crystal frame: sigma_l = R * sigma_g * R^T
        double sigma_l[3][3] = {0};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    for (int l = 0; l < 3; ++l)
                        sigma_l[i][j] += R[i][k] * sigma_g[k][l] * R[j][l];

        double elem_max_tau = 0.0;
        for (int sys = 0; sys < NUM_SLIP_SYSTEMS; ++sys) {
            const double tau = std::abs(
                resolvedShearStress(sigma_l, SLIP_NORMALS[sys], SLIP_DIRECTIONS[sys]));
            if (tau > elem_max_tau) elem_max_tau = tau;
        }
        tau_list.push_back({ e, elem_max_tau });
    }

    if (tau_list.empty()) return std::nullopt;

    // 98th-percentile element (robust against a single outlier voxel)
    std::sort(tau_list.begin(), tau_list.end(),
              [](const ElemTau& a, const ElemTau& b){ return a.tau < b.tau; });
    const double percentile = 0.98;
    int p_index = static_cast<int>(tau_list.size() * percentile);
    if (p_index >= (int)tau_list.size()) p_index = (int)tau_list.size() - 1;
    const double max_tau = tau_list[p_index].tau;

    if (max_tau < 10.0) {
        qWarning() << "[computeYieldPointForStep] max tau too small (" << max_tau
                   << "), skipping step to avoid math explosion";
        return std::nullopt;
    }

    // scale the macro stress so the critical element is exactly at the threshold
    const double k = (m_vonMises ? m_yieldStress : CRSS) / max_tau;
    std::array<double,6> sigma_yield = {
        macro_stress[0]*k, macro_stress[1]*k, macro_stress[2]*k,
        macro_stress[3]*k, macro_stress[4]*k, macro_stress[5]*k
    };

    // remove hydrostatic part -> pure deviator (full6d keeps it: pressure-sensitive)
    if (!m_full6D) {
        const double hydro = (sigma_yield[0] + sigma_yield[1] + sigma_yield[2]) / 3.0;
        sigma_yield[0] -= hydro; sigma_yield[1] -= hydro; sigma_yield[2] -= hydro;
    }

    return sigma_yield;
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
    if (m_full6D) {
        QString reason;
        if (fitFull6D(yield_points, &reason))
            return true;
        qWarning().noquote() << "[HillCriterion::fit] full6d fit rejected (" + reason
                                + ") -> falling back to deviatoric5d for this fit";
    }
    m_fittedFull6D = false;

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

    // --- АДАПТИВНАЯ РЕГУЛЯРИЗАЦИЯ ---
    // 1. Находим максимальный элемент на главной диагонали
    double max_diag = 0.0;
    for (int i = 0; i < 5; ++i) {
        if (m_P_Hill_5D[i][i] > max_diag) max_diag = m_P_Hill_5D[i][i];
    }
    if (max_diag < 1e-30) max_diag = 1e-6 / (scale * scale);

    // 2. Делаем бекап "чистой" матрицы
    double P_backup[5][5];
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            P_backup[i][j] = m_P_Hill_5D[i][j];
        }
    }

    // 3. Пытаемся разложить "чистую" матрицу (сработает для поликристаллов)
    qDebug() << "[HillCriterion::fit] --- Cholesky decomposition P_5D = L*L^T ---";
    bool is_pd = computeCholesky5D();

    int attempts = 0;
    double shift_multiplier = 1e-4; // Начинаем с очень мягкого сдвига (0.01%)

    // 4. Если матрица гиперболоид (DLCA), постепенно усиливаем сдвиг, пока она не станет эллипсоидом
    while (!is_pd && attempts < 6) {
        qWarning() << QString("[HillCriterion::fit] Matrix not PD. Retrying with dynamic shift: %1% of max_diag")
        .arg(shift_multiplier * 100);

        // Восстанавливаем чистую матрицу и добавляем новый сдвиг
        for (int i = 0; i < 5; ++i) {
            for (int j = 0; j < 5; ++j) {
                m_P_Hill_5D[i][j] = P_backup[i][j];
            }
            m_P_Hill_5D[i][i] += max_diag * shift_multiplier; // Усиливаем диагональ
        }

        is_pd = computeCholesky5D();
        shift_multiplier *= 10.0; // Если не помогло, на следующей итерации бьем в 10 раз сильнее
        attempts++;
    }

    if (!is_pd) {
        qWarning() << "[HillCriterion::fit] ERROR: P_Hill_5D is not positive definite even after heavy regularization. Fit failed.";
        return false;
    }
    qDebug() << "[HillCriterion::fit]   Cholesky succeeded. P_Hill_5D is valid.";
    // ----------------------------------------

    // Project back to 6D for HDF5 export
    qDebug() << "[HillCriterion::fit] --- Projecting 5D -> 6D for HDF5 export ---";
    project5Dto6D_Matrix();

    m_isValid = true;
    qDebug() << "[HillCriterion::fit] Fit COMPLETED SUCCESSFULLY. m_isValid = true\n";
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Least-squares fit of the Hill quadric in full 6D stress space
//  Goal: find symmetric matrix P(6x6) such that
//        sigma^T * P * sigma = 1  for all yield points sigma (hydrostatic part kept)
//  LS problem over 21 independent parameters (upper triangle of P).
//  No Tikhonov and no diagonal shift: a singular system, a non-PD P or a badly
//  conditioned P means the data are (nearly) pressure-insensitive, and the
//  caller falls back to the 5D deviatoric fit.
// ─────────────────────────────────────────────────────────────────────────────

// Data-support check for the hydrostatic axis: the fitted surface may extend at
// most this many times further along e_h = (1,1,1,0,0,0)/sqrt(3) than the
// furthest yield point does. Beyond that the hydrostatic coefficient is an
// extrapolation of round-off (nearly pressure-insensitive data), not a fit.
static constexpr double HYDRO_SUPPORT_FACTOR = 10.0;

bool HillCriterion::fitFull6D(const std::vector<std::array<double,6>>& yield_points, QString* reason) {
    qDebug() << "\n[HillCriterion::fitFull6D] ════════════════════════════════════════";
    qDebug() << "[HillCriterion::fitFull6D] Least-squares fit of Hill quadric in full 6D space";
    qDebug() << "[HillCriterion::fitFull6D]   Yield points N     :" << (int)yield_points.size();

    m_isValid = false;
    const int N = (int)yield_points.size();
    if (N < 21) {
        if (reason) *reason = QString("not enough yield points (%1 < 21)").arg(N);
        return false;
    }

    // Normalisation scale for numerical stability (same as the 5D fit)
    double scale = 1.0;
    for (const auto& pt : yield_points)
        for (int i = 0; i < 6; ++i)
            scale = std::max(scale, std::abs(pt[i]));
    if (scale < 1e-12) scale = 1.0;
    qDebug() << "[HillCriterion::fitFull6D]   Normalisation scale =" << scale << "Pa";

    // Map upper-triangle parameters (6x6) to linear index [0..20]
    auto param_idx = [](int i, int j) -> int {
        if (i > j) std::swap(i, j);
        int idx = 0;
        for (int r = 0; r < i; ++r) idx += (6 - r);
        idx += (j - i);
        return idx;
    };

    double ATA[21][21] = {{0}};
    double ATb[21]     = {0};

    for (int n = 0; n < N; ++n) {
        double s[6];
        for (int i = 0; i < 6; ++i) s[i] = yield_points[n][i] / scale;

        double A_row[21] = {0};
        for (int i = 0; i < 6; ++i)
            for (int j = i; j < 6; ++j)
                A_row[param_idx(i,j)] += (i != j) ? 2.0 * s[i] * s[j] : s[i] * s[j];

        for (int i = 0; i < 21; ++i) {
            for (int j = 0; j < 21; ++j) ATA[i][j] += A_row[i] * A_row[j];
            ATb[i] += A_row[i];
        }
    }

    double x[21] = {0};
    if (!solveSystem21x21(ATA, ATb, x)) {
        if (reason) *reason = "21x21 normal system is singular or ill-posed";
        return false;
    }

    double P[6][6];
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            P[i][j] = x[param_idx(i,j)] / (scale * scale);

    // Cholesky P = L*L^T without any diagonal shift: a shift would force PD and
    // hide nearly pressure-insensitive data instead of falling back.
    double L[6][6] = {{0}};
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j <= i; ++j) {
            double sum = 0.0;
            for (int k = 0; k < j; ++k) sum += L[i][k] * L[j][k];
            if (i == j) {
                const double val = P[i][i] - sum;
                if (!(val > 0.0)) {
                    if (reason) *reason = QString("P(6x6) is not positive definite (pivot %1 = %2)")
                                              .arg(i).arg(val, 0, 'e', 4);
                    return false;
                }
                L[i][i] = std::sqrt(val);
            } else {
                L[i][j] = (P[i][j] - sum) / L[j][j];
            }
        }
    }

    double min_d = L[0][0] * L[0][0], max_d = min_d;
    for (int i = 1; i < 6; ++i) {
        min_d = std::min(min_d, L[i][i] * L[i][i]);
        max_d = std::max(max_d, L[i][i] * L[i][i]);
    }
    const double cond_ratio = min_d / max_d;
    qDebug() << "[HillCriterion::fitFull6D]   min(L_ii^2)/max(L_ii^2) =" << cond_ratio;
    if (cond_ratio < 1e-8) {
        if (reason) *reason = QString("P(6x6) is badly conditioned (min/max L_ii^2 = %1 < 1e-8)")
                                  .arg(cond_ratio, 0, 'e', 4);
        return false;
    }

    // Hydrostatic extent of the surface vs. of the data:
    // r_h = 1/sqrt(e_h^T P e_h), p_max = max |sigma . e_h|
    double eh_P_eh = 0.0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            eh_P_eh += P[i][j] / 3.0;
    const double r_h = 1.0 / std::sqrt(eh_P_eh);
    double p_max = 0.0;
    for (const auto& pt : yield_points)
        p_max = std::max(p_max, std::abs(pt[0] + pt[1] + pt[2]) / std::sqrt(3.0));
    qDebug() << "[HillCriterion::fitFull6D]   Hydrostatic extent r_h =" << r_h << "Pa, data p_max =" << p_max << "Pa";
    if (!(r_h <= HYDRO_SUPPORT_FACTOR * p_max)) {
        if (reason) *reason = QString("hydrostatic extent not supported by data: r_h=%1, p_max=%2")
                                  .arg(r_h, 0, 'e', 4).arg(p_max, 0, 'e', 4);
        return false;
    }

    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            m_P_Hill[i][j] = P[i][j];

    qDebug() << "[HillCriterion::fitFull6D] P_Hill(6x6):";
    for (int i = 0; i < 6; ++i) {
        QString row;
        for (int j = 0; j < 6; ++j)
            row += QString::number(m_P_Hill[i][j], 'e', 3) + "  ";
        qDebug().noquote() << "  [" + QString::number(i) + "]  " + row;
    }

    // Keep the 5D working members consistent (deviatoric restriction of a PD P is PD)
    project6Dto5D_Matrix();
    if (!computeCholesky5D()) {
        if (reason) *reason = "deviatoric restriction of P(6x6) failed Cholesky";
        return false;
    }

    m_fittedFull6D = true;
    m_isValid = true;
    qDebug() << "[HillCriterion::fitFull6D] Fit COMPLETED SUCCESSFULLY. m_isValid = true\n";
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
//    1. Uniform sampling on a 5D unit sphere (normal distribution trick),
//       or on the 6D unit sphere in full6d (hydrostatic axis included)
//    2. Map to 5D ellipsoid via L^{-T}: d_v = L^{-T} * d
//    3. Convert to 6D deviatoric stress
//    4. Convert to strain via elastic compliance tensor S
//    5. Normalise to target strain_val
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::vector<double>> HillCriterion::generateLoads(
    int num_samples, double strain_val, const double S[6][6])
{
    std::vector<std::vector<double>> load_cases;
    if (!m_isValid) return load_cases;

    std::mt19937 gen(Parameters::seed);
    std::normal_distribution<double> dist(0.0, 1.0);

    // ── Крок 1: будуємо тензори Шміда для всіх 48 систем (у 6D Мандела)
    // m_alpha[i] = симетризований b⊗n у порядку [xx,yy,zz,yz,xz,xy]
    std::vector<std::array<double,6>> schmid(NUM_SLIP_SYSTEMS);
    for (int s = 0; s < NUM_SLIP_SYSTEMS; ++s) {
        double n[3], b[3];
        // нормуємо
        double nl = 0, bl = 0;
        for (int i=0;i<3;i++){nl+=SLIP_NORMALS[s][i]*SLIP_NORMALS[s][i]; bl+=SLIP_DIRECTIONS[s][i]*SLIP_DIRECTIONS[s][i];}
        nl=std::sqrt(nl); bl=std::sqrt(bl);
        for (int i=0;i<3;i++){n[i]=SLIP_NORMALS[s][i]/nl; b[i]=SLIP_DIRECTIONS[s][i]/bl;}
        // Мандел: [xx,yy,zz, sqrt2*yz, sqrt2*xz, sqrt2*xy]
        schmid[s] = {
            b[0]*n[0], b[1]*n[1], b[2]*n[2],
            (b[1]*n[2]+b[2]*n[1])*M_SQRT2*0.5,
            (b[0]*n[2]+b[2]*n[0])*M_SQRT2*0.5,
            (b[0]*n[1]+b[1]*n[0])*M_SQRT2*0.5
        };
    }

    // ── Крок 2: генеруємо цільові напрямки для ребер
    // Ребро (i,j): шукаємо σ таке що |m_i·σ|=|m_j·σ|=max по всім системам
    // Schmid edge directions only make sense for the Schmid local criterion.
    std::vector<std::array<double,6>> edge_dirs;

    for (int i = 0; i < NUM_SLIP_SYSTEMS && !m_vonMises; ++i) {
        for (int j = i+1; j < NUM_SLIP_SYSTEMS; ++j) {

            // Напрямок ребра: лінійна комбінація двох тензорів Шміда
            // σ_edge = alpha * m_i + (1-alpha) * m_j, нормоване
            // Перевіряємо що обидва tau максимальні
            for (double alpha : {0.3, 0.5, 0.7}) {
                std::array<double,6> dir;
                double norm_sq = 0;
                for (int k=0;k<6;k++){
                    dir[k] = alpha*schmid[i][k] + (1-alpha)*schmid[j][k];
                    norm_sq += dir[k]*dir[k];
                }
                if (norm_sq < 1e-20) continue;
                double norm = std::sqrt(norm_sq);
                for (int k=0;k<6;k++) dir[k] /= norm;

                // Перевіряємо що цей напрямок дійсно на ребрі:
                // два найбільших tau мають бути приблизно рівні
                double tau_vals[NUM_SLIP_SYSTEMS];
                for (int s=0;s<NUM_SLIP_SYSTEMS;s++){
                    tau_vals[s]=0;
                    for(int k=0;k<6;k++) tau_vals[s]+=schmid[s][k]*dir[k];
                    tau_vals[s]=std::abs(tau_vals[s]);
                }
                std::sort(tau_vals, tau_vals+NUM_SLIP_SYSTEMS, std::greater<double>());
                // якщо два найбільших відрізняються менш ніж на 5% — це ребро
                if (tau_vals[0] > 1e-10 && std::abs(tau_vals[0]-tau_vals[1])/tau_vals[0] < 0.05)
                    edge_dirs.push_back(dir);
            }
        }
    }

    qDebug() << "[generateLoads] Edge directions found:" << (int)edge_dirs.size();

    // ── Крок 3: розподіл точок
    // 70% рівномірно, 30% цілеспрямовано на ребра
    // (von Mises: all samples uniform)
    int n_uniform = m_vonMises ? num_samples : (int)(num_samples * 0.70);
    int n_edges   = num_samples - n_uniform;

    auto addLoad = [&](const std::array<double,6>& d_sigma) {
        double d_eps[6]={0}, eps_norm_sq=0;
        for(int i=0;i<6;i++){
            for(int j=0;j<6;j++) d_eps[i]+=S[i][j]*d_sigma[j];
            eps_norm_sq+=d_eps[i]*d_eps[i];
        }
        double eps_norm=std::sqrt(eps_norm_sq);
        if(eps_norm<1e-30) return false;
        std::vector<double> eps(6);
        for(int i=0;i<6;i++) eps[i]=(d_eps[i]/eps_norm)*strain_val;
        load_cases.push_back(eps);
        return true;
    };

    // Рівномірні точки (як раніше, але без L^{-T})
    int retries=0;
    for(int n=0; n<n_uniform; ){
        if (m_full6D) {
            // uniform on the 6D unit sphere, hydrostatic axis included
            std::array<double,6> ds; double norm_sq=0;
            for(int i=0;i<6;i++){ds[i]=dist(gen); norm_sq+=ds[i]*ds[i];}
            double norm=std::sqrt(norm_sq);
            if(norm<1e-12){++retries; continue;}
            for(int i=0;i<6;i++) ds[i]/=norm;
            if(addLoad(ds)) ++n; else ++retries;
            continue;
        }
        double d[5], norm_sq=0;
        for(int i=0;i<5;i++){d[i]=dist(gen); norm_sq+=d[i]*d[i];}
        double norm=std::sqrt(norm_sq);
        if(norm<1e-12){++retries; continue;}
        for(int i=0;i<5;i++) d[i]/=norm;
        double d_sigma[6]={0};
        v5D_to_sigma6D(d, d_sigma);
        std::array<double,6> ds; for(int i=0;i<6;i++) ds[i]=d_sigma[i];
        if(addLoad(ds)) ++n; else ++retries;
    }

    // Цільові точки на ребрах
    if (!edge_dirs.empty()) {
        std::uniform_int_distribution<int> idx_dist(0, (int)edge_dirs.size()-1);
        std::normal_distribution<double> jitter(0.0, 0.05); // невеликий шум
        for(int n=0; n<n_edges; ){
            auto dir = edge_dirs[idx_dist(gen)];
            // додаємо невеликий шум щоб не дублювати точки
            double norm_sq=0;
            for(int i=0;i<6;i++){dir[i]+=jitter(gen); norm_sq+=dir[i]*dir[i];}
            double norm=std::sqrt(norm_sq);
            if(norm<1e-12) continue;
            for(int i=0;i<6;i++) dir[i]/=norm;
            if(addLoad(dir)) ++n;
        }
    }

    qDebug() << "[generateLoads] Total loads:" << (int)load_cases.size()
             << "(uniform:" << n_uniform << ", edges:" << n_edges
             << ", rejected:" << retries << ")";

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

    // record how P_Hill was produced (hill_space = space actually used by the fit)
    hdf5.write(prefix, "local_criterion", QString(m_vonMises ? "vonmises" : "schmid"));
    hdf5.write(prefix, "hill_space",      QString(m_fittedFull6D ? "full6d" : "deviatoric5d"));

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

// ─────────────────────────────────────────────────────────────────────────────
//  Solve 21x21 linear system via Gaussian elimination with partial pivoting
//  (full 6D fit). Relative pivot tolerance 1e-12 * max|A|: a (near-)singular
//  normal system is reported as failure, never regularised.
// ─────────────────────────────────────────────────────────────────────────────
bool HillCriterion::solveSystem21x21(double A[21][21], double b[21], double x[21]) {
    const int N = 21;

    double M[21][22];
    double max_abs = 0.0;
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            M[i][j] = A[i][j];
            max_abs = std::max(max_abs, std::abs(A[i][j]));
        }
        M[i][N] = b[i];
    }
    const double tol = 1e-12 * max_abs;

    for (int i = 0; i < N; ++i) {
        int pivot = i;
        for (int j = i + 1; j < N; ++j)
            if (std::abs(M[j][i]) > std::abs(M[pivot][i])) pivot = j;
        for (int k = 0; k <= N; ++k) std::swap(M[i][k], M[pivot][k]);

        if (!(std::abs(M[i][i]) > tol)) {
            qWarning() << QString("[HillCriterion::solveSystem21x21] pivot at step %1 is %2 <= tol %3. Matrix is singular.")
                              .arg(i).arg(M[i][i], 0, 'e', 4).arg(tol, 0, 'e', 4);
            return false;
        }

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

// ============================================================================
//  matviz_homog.hpp  -  MatViz3D adapter for the FFT homogenizer.
//
//  Turns a MatViz3D voxel microstructure (grain-id field + per-grain Bunge ZXZ
//  Euler angles) into inputs for ffth::FFTHomogenizer, and reports the
//  effective (homogenized) stiffness plus a few derived engineering scalars.
//
//  Depends only on fft_homog.hpp (header-only).  Pure C++17, no Qt required
//  here so it stays unit-testable; the Qt bridge lives in a separate file.
// ============================================================================
#pragma once
#include "fft_homog.hpp"
#include <array>
#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace mvh {

using ffth::Mat6;
using Mat3 = std::array<std::array<double,3>,3>;
using C4   = std::array<std::array<std::array<std::array<double,3>,3>,3>,3>;

// --- Bunge ZXZ orientation matrix g:  [v]_crystal = g [v]_sample ------------
//  g = Rz(phi2) * Rx(Phi) * Rz(phi1)   (angles in radians)
inline Mat3 bunge_zxz(double phi1, double Phi, double phi2) {
    const double c1=std::cos(phi1), s1=std::sin(phi1);
    const double c =std::cos(Phi),  s =std::sin(Phi);
    const double c2=std::cos(phi2), s2=std::sin(phi2);
    Mat3 g;
    g[0][0]= c1*c2 - s1*s2*c;  g[0][1]= s1*c2 + c1*s2*c;  g[0][2]= s2*s;
    g[1][0]=-c1*s2 - s1*c2*c;  g[1][1]=-s1*s2 + c1*c2*c;  g[1][2]= c2*s;
    g[2][0]= s1*s;             g[2][1]=-c1*s;             g[2][2]= c;
    return g;
}

// --- Cubic single-crystal stiffness (crystal axes) as a 4th-order tensor -----
inline C4 cubic_C4(double C11, double C12, double C44) {
    C4 C{};
    for (int i=0;i<3;++i)for(int j=0;j<3;++j)for(int k=0;k<3;++k)for(int l=0;l<3;++l)
        C[i][j][k][l]=0.0;
    auto d=[](int a,int b){return a==b?1.0:0.0;};
    // cubic: C_ijkl = C12 d_ij d_kl + C44 (d_ik d_jl + d_il d_jk)
    //                 + (C11 - C12 - 2 C44) * delta on coincident axes
    const double H = C11 - C12 - 2.0*C44;
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)for(int k=0;k<3;++k)for(int l=0;l<3;++l){
        C[i][j][k][l] = C12*d(i,j)*d(k,l)
                      + C44*(d(i,k)*d(j,l)+d(i,l)*d(j,k))
                      + (i==j&&j==k&&k==l ? H : 0.0);
    }
    return C;
}

// --- Rotate a 4th-order tensor from crystal frame into the sample frame ------
//  C^sample_abcd = g_ia g_jb g_kc g_ld C^crystal_ijkl
inline C4 rotate_C4(const C4& Cc, const Mat3& g) {
    C4 Cs{};
    for(int a=0;a<3;++a)for(int b=0;b<3;++b)for(int c=0;c<3;++c)for(int d=0;d<3;++d){
        double s=0.0;
        for(int i=0;i<3;++i)for(int j=0;j<3;++j)for(int k=0;k<3;++k)for(int l=0;l<3;++l)
            s += g[i][a]*g[j][b]*g[k][c]*g[l][d]*Cc[i][j][k][l];
        Cs[a][b][c][d]=s;
    }
    return Cs;
}

// --- 4th-order tensor -> Mandel 6x6 -----------------------------------------
inline Mat6 to_mandel(const C4& C) {
    static const int I[6]={0,1,2,1,0,0};
    static const int J[6]={0,1,2,2,2,1};
    const double f[6]={1,1,1,std::sqrt(2.0),std::sqrt(2.0),std::sqrt(2.0)};
    Mat6 M{};
    for(int a=0;a<6;++a)for(int b=0;b<6;++b)
        M[a][b]=f[a]*f[b]*C[I[a]][J[a]][I[b]][J[b]];
    return M;
}

// One-shot: cubic grain with given orientation -> Mandel 6x6 (sample frame)
inline Mat6 cubic_grain_mandel(double C11,double C12,double C44,
                               double phi1,double Phi,double phi2) {
    return to_mandel(rotate_C4(cubic_C4(C11,C12,C44), bunge_zxz(phi1,Phi,phi2)));
}

// --- General anisotropic stiffness (Voigt 6x6, material axes) -> 4th order ---
//  Voigt index <-> index pair:  0=11 1=22 2=33 3=23 4=13 5=12, which is the
//  same pairing to_mandel() uses, and the same one the material database's
//  c11..c66 columns are written in. Stiffness (as opposed to compliance) needs
//  no factors of 2, so this is a plain re-indexing.
//
//  Anything the cubic path can express, this can too: cubic_C4(C11,C12,C44)
//  and voigt_to_C4() of the corresponding Voigt matrix agree exactly.
inline C4 voigt_to_C4(const double Cv[6][6]) {
    // (i,j) -> Voigt row/column
    static const int V[3][3] = { {0, 5, 4},
                                 {5, 1, 3},
                                 {4, 3, 2} };
    C4 C{};
    for (int i=0;i<3;++i)for(int j=0;j<3;++j)for(int k=0;k<3;++k)for(int l=0;l<3;++l)
        C[i][j][k][l] = Cv[V[i][j]][V[k][l]];
    return C;
}

// One-shot: general anisotropic material with a given orientation -> Mandel 6x6
// in the sample frame. The transversely isotropic and orthotropic rows of the
// material database go through here; cubic ones can too.
inline Mat6 aniso_grain_mandel(const double Cv[6][6],
                               double phi1,double Phi,double phi2) {
    return to_mandel(rotate_C4(voigt_to_C4(Cv), bunge_zxz(phi1,Phi,phi2)));
}

// --- Orientation matrix -> Bunge ZXZ (radians) -------------------------------
//  Exact inverse of bunge_zxz(): the rows of g are the crystal axes written in
//  sample coordinates, so a caller that knows where it wants the material axes
//  to point (a fiber, say) can build g directly and convert.
//
//  Phi is returned in [0, pi], which is the canonical Bunge range; at the
//  Phi = 0 / pi poles only phi1 + phi2 (resp. phi1 - phi2) is determined, and
//  the split is fixed by putting everything in phi1.
inline void bunge_from_matrix(const Mat3& g,
                              double& phi1, double& Phi, double& phi2) {
    double c = g[2][2];
    if (c >  1.0) c =  1.0;
    if (c < -1.0) c = -1.0;
    Phi = std::acos(c);

    const double s = std::sqrt(std::max(0.0, 1.0 - c * c));
    if (s < 1e-9) {                       // gimbal lock: the Z rotations merge
        phi2 = 0.0;
        phi1 = (c > 0.0) ? std::atan2(g[0][1],  g[0][0])    // Phi = 0
                         : std::atan2(g[0][1], -g[0][0]);   // Phi = pi
        return;
    }
    //  g[2][0] = sin(phi1) sin(Phi),  g[2][1] = -cos(phi1) sin(Phi)
    //  g[0][2] = sin(phi2) sin(Phi),  g[1][2] =  cos(phi2) sin(Phi)
    phi1 = std::atan2(g[2][0], -g[2][1]);
    phi2 = std::atan2(g[0][2],  g[1][2]);
}

// Zener anisotropy of a cubic crystal (=1 for isotropic)
inline double zener(double C11,double C12,double C44){ return 2.0*C44/(C11-C12); }

// ============================================================================
//  FCC {111}<110> Slip Systems & Schmid / CRSS Yield Criterion (Copper)
// ============================================================================

struct SlipSystem {
    std::array<double, 3> n; // Unit slip-plane normal {111} / sqrt(3)
    std::array<double, 3> b; // Unit slip direction    <110> / sqrt(2)
};

inline constexpr int kNumFCCSlipSystems = 12;
inline constexpr double kInvSqrt3 = 0.57735026918962576451; // 1 / sqrt(3)
inline constexpr double kInvSqrt2 = 0.70710678118654752440; // 1 / sqrt(2)

// 12 FCC slip systems: 4 {111} planes x 3 <110> directions (all b . n == 0)
inline constexpr std::array<SlipSystem, kNumFCCSlipSystems> kFCCSlipSystems = {{
    // Plane (1, 1, 1)
    { {  kInvSqrt3,  kInvSqrt3,  kInvSqrt3 }, { -kInvSqrt2,  kInvSqrt2,  0.0       } },
    { {  kInvSqrt3,  kInvSqrt3,  kInvSqrt3 }, { -kInvSqrt2,  0.0,        kInvSqrt2 } },
    { {  kInvSqrt3,  kInvSqrt3,  kInvSqrt3 }, {  0.0,       -kInvSqrt2,  kInvSqrt2 } },
    // Plane (-1, 1, 1)
    { { -kInvSqrt3,  kInvSqrt3,  kInvSqrt3 }, {  kInvSqrt2,  kInvSqrt2,  0.0       } },
    { { -kInvSqrt3,  kInvSqrt3,  kInvSqrt3 }, {  kInvSqrt2,  0.0,        kInvSqrt2 } },
    { { -kInvSqrt3,  kInvSqrt3,  kInvSqrt3 }, {  0.0,       -kInvSqrt2,  kInvSqrt2 } },
    // Plane (1, -1, 1)
    { {  kInvSqrt3, -kInvSqrt3,  kInvSqrt3 }, {  kInvSqrt2,  kInvSqrt2,  0.0       } },
    { {  kInvSqrt3, -kInvSqrt3,  kInvSqrt3 }, { -kInvSqrt2,  0.0,        kInvSqrt2 } },
    { {  kInvSqrt3, -kInvSqrt3,  kInvSqrt3 }, {  0.0,        kInvSqrt2,  kInvSqrt2 } },
    // Plane (1, 1, -1)
    { {  kInvSqrt3,  kInvSqrt3, -kInvSqrt3 }, { -kInvSqrt2,  kInvSqrt2,  0.0       } },
    { {  kInvSqrt3,  kInvSqrt3, -kInvSqrt3 }, {  kInvSqrt2,  0.0,        kInvSqrt2 } },
    { {  kInvSqrt3,  kInvSqrt3, -kInvSqrt3 }, {  0.0,        kInvSqrt2,  kInvSqrt2 } }
}};

enum class YieldCriterionMode : int {
    ModeA_SchmidThreshold = 0, // Compare m_max against user threshold (e.g. m > 0.4)
    ModeB_PhysicalCRSS    = 1  // Compare tau_max against physical CRSS tau_cr (Pa)
};

struct YieldCriterionParams {
    YieldCriterionMode mode = YieldCriterionMode::ModeB_PhysicalCRSS;
    double schmidThreshold  = 0.40;   // Mode A threshold (-)
    double crss             = 10.0e6; // Mode B CRSS for annealed Cu (~10 MPa, in Pa)
    bool   continuousOutput = false;  // false: binary {0.0, 1.0}; true: continuous ratio / m_max
    bool   useUnidirectionalLoad = false;
    std::array<double, 3> loadDir = { 1.0, 0.0, 0.0 }; // Global load vector if useUnidirectionalLoad
};

struct VoxelSlipResult {
    float m_max      = 0.0f; // Maximum Schmid factor across the 12 FCC systems [0, 0.5]
    float tau_max    = 0.0f; // Maximum Resolved Shear Stress (Pa)
    float yield_eval = 0.0f; // Mode-evaluated scalar (binary 0/1 or continuous ratio)
};

// Rotate a symmetric 3x3 global stress tensor (pipeline order: sx, sy, sz, sxy, syz, sxz)
// into the local crystal frame using g = bunge_zxz(phi1, Phi, phi2):
//   sigma_crystal = g * sigma_sample * g^T
inline Mat3 rotate_stress_to_crystal(const double sigma_pipeline[6], const Mat3& g) {
    const double sg[3][3] = {
        { sigma_pipeline[0], sigma_pipeline[3], sigma_pipeline[5] },
        { sigma_pipeline[3], sigma_pipeline[1], sigma_pipeline[4] },
        { sigma_pipeline[5], sigma_pipeline[4], sigma_pipeline[2] }
    };
    double tmp[3][3] = {{0.0}};
    for (int i = 0; i < 3; ++i)
        for (int l = 0; l < 3; ++l)
            tmp[i][l] = sg[0][l] * g[0][i] + sg[1][l] * g[1][i] + sg[2][l] * g[2][i];

    Mat3 sc{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            sc[i][j] = g[i][0] * tmp[j][0] + g[i][1] * tmp[j][1] + g[i][2] * tmp[j][2];
    return sc;
}

// Compute m_max, tau_max, and yield criterion for one voxel
inline VoxelSlipResult evaluate_fcc_voxel(
    const double sigma_pipeline[6],
    double phi1_rad, double Phi_rad, double phi2_rad,
    const YieldCriterionParams& params)
{
    const Mat3 g  = bunge_zxz(phi1_rad, Phi_rad, phi2_rad);
    const Mat3 sc = rotate_stress_to_crystal(sigma_pipeline, g);

    // Equivalent (von Mises) stress as the scalar reference for 3D multi-axial Schmid factor
    const double d01 = sigma_pipeline[0] - sigma_pipeline[1];
    const double d12 = sigma_pipeline[1] - sigma_pipeline[2];
    const double d20 = sigma_pipeline[2] - sigma_pipeline[0];
    const double seqv = std::sqrt(0.5 * (d01 * d01 + d12 * d12 + d20 * d20) +
                                  3.0 * (sigma_pipeline[3] * sigma_pipeline[3] +
                                         sigma_pipeline[4] * sigma_pipeline[4] +
                                         sigma_pipeline[5] * sigma_pipeline[5]));

    // Optional unidirectional load vector rotated into crystal frame: l_c = g * l_s
    std::array<double, 3> lc = { 0.0, 0.0, 0.0 };
    double loadMag = 0.0;
    if (params.useUnidirectionalLoad) {
        const auto& d = params.loadDir;
        loadMag = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (loadMag > 1e-30) {
            for (int i = 0; i < 3; ++i)
                lc[i] = (g[i][0] * d[0] + g[i][1] * d[1] + g[i][2] * d[2]) / loadMag;
        }
    }

    double max_tau = 0.0;
    double max_m   = 0.0;

    for (const auto& sys : kFCCSlipSystems) {
        // tau^(alpha) = | b_i * sigma^crystal_ij * n_j |
        double tau = 0.0;
        for (int i = 0; i < 3; ++i) {
            const double s_in = sc[i][0] * sys.n[0] + sc[i][1] * sys.n[1] + sc[i][2] * sys.n[2];
            tau += sys.b[i] * s_in;
        }
        tau = std::abs(tau);
        if (tau > max_tau) max_tau = tau;

        double m = 0.0;
        if (params.useUnidirectionalLoad && loadMag > 1e-30) {
            const double cos_phi    = lc[0] * sys.n[0] + lc[1] * sys.n[1] + lc[2] * sys.n[2];
            const double cos_lambda = lc[0] * sys.b[0] + lc[1] * sys.b[1] + lc[2] * sys.b[2];
            m = std::abs(cos_phi * cos_lambda);
        } else if (seqv > 1e-12) {
            m = std::clamp(tau / seqv, 0.0, 0.5);
        }
        if (m > max_m) max_m = m;
    }

    if (params.useUnidirectionalLoad && seqv <= 1e-12 && loadMag > 1e-30) {
        max_tau = max_m * loadMag;
    }

    VoxelSlipResult out;
    out.m_max   = static_cast<float>(max_m);
    out.tau_max = static_cast<float>(max_tau);

    if (params.mode == YieldCriterionMode::ModeA_SchmidThreshold) {
        out.yield_eval = params.continuousOutput
            ? static_cast<float>(max_m)
            : ((max_m >= params.schmidThreshold) ? 1.0f : 0.0f);
    } else {
        const double crss = std::max(1e-12, params.crss);
        out.yield_eval = params.continuousOutput
            ? static_cast<float>(max_tau / crss)
            : ((max_tau >= crss) ? 1.0f : 0.0f);
    }
    return out;
}

// Thread-safe OpenMP per-voxel field computation over dense N^3 grid (index = (z*N + y)*N + x).
// Supports both FFT (1-based grain_id direct indexing into local_cs when local_cs.size() > maxGrain)
// and ANSYS (0-based grain_id - 1 indexing), as well as radians or degrees in local_cs.
inline void compute_fcc_schmid_field(
    int N,
    int32_t*** voxels,
    const std::vector<std::vector<float>>& local_cs,
    const std::vector<std::array<float, 6>>& dense_stress_pipeline,
    const YieldCriterionParams& params,
    std::vector<float>& out_schmid_factor,
    std::vector<float>& out_yield_status)
{
    const std::size_t total = static_cast<std::size_t>(N) * N * N;
    out_schmid_factor.assign(total, 0.0f);
    out_yield_status.assign(total, 0.0f);
    if (N <= 0 || !voxels || local_cs.empty()) return;

    // Detect whether local_cs is stored in degrees (ANSYS LOCAL table) or radians (FFT session)
    bool anglesInDegrees = false;
    for (const auto& row : local_cs) {
        if (row.size() >= 3 && (std::abs(row[0]) > 6.2831855f ||
                                std::abs(row[1]) > 6.2831855f ||
                                std::abs(row[2]) > 6.2831855f)) {
            anglesInDegrees = true;
            break;
        }
    }
    const double angleScale = anglesInDegrees ? (3.14159265358979323846 / 180.0) : 1.0;
    const bool hasVoidSlot0 = (local_cs.size() >= 2 &&
                               local_cs[0].size() >= 3 &&
                               local_cs[0][0] == 0.0f &&
                               local_cs[0][1] == 0.0f &&
                               local_cs[0][2] == 0.0f);

    const int totalInt = static_cast<int>(total);
    #pragma omp parallel for schedule(static)
    for (int idx = 0; idx < totalInt; ++idx) {
        const int iz = idx / (N * N);
        const int iy = (idx / N) % N;
        const int ix = idx % N;

        const int32_t grain_id = voxels[ix][iy][iz];
        if (grain_id <= 0) continue; // Skip void voxels

        const int cs_idx = (hasVoidSlot0 && grain_id < static_cast<int32_t>(local_cs.size()))
                               ? grain_id
                               : (grain_id - 1);
        if (cs_idx < 0 || cs_idx >= static_cast<int>(local_cs.size()) || local_cs[cs_idx].size() < 3)
            continue;

        const double phi1 = local_cs[cs_idx][0] * angleScale;
        const double Phi  = local_cs[cs_idx][1] * angleScale;
        const double phi2 = local_cs[cs_idx][2] * angleScale;

        double s6[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        if (static_cast<std::size_t>(idx) < dense_stress_pipeline.size()) {
            for (int c = 0; c < 6; ++c)
                s6[c] = dense_stress_pipeline[static_cast<std::size_t>(idx)][c];
        }

        const VoxelSlipResult res = evaluate_fcc_voxel(s6, phi1, Phi, phi2, params);
        out_schmid_factor[static_cast<std::size_t>(idx)] = res.m_max;
        out_yield_status[static_cast<std::size_t>(idx)]  = res.yield_eval;
    }
}

} // namespace mvh

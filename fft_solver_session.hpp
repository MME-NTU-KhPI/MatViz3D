// ============================================================================
//  fft_solver_session.hpp  -  FFT analog of the parts of ansysWrapper that the
//                             stress-analysis / Hill-calibration pipeline uses.
//
//  Instead of generating APDL and launching ANSYS, every "load case" is solved
//  in memory with ffth::FFTHomogenizer (Moulinec-Suquet basic scheme).  The
//  session exposes:
//
//     * canonical compliance/stiffness S,C            (replaces computeSMatrix)
//     * solveLoadCase(macro_strain) -> per-voxel field (replaces one ANSYS LS)
//     * solid_fraction, local_cs, per-grain material library
//
//  Conventions (READ THIS)
//  -----------------------
//  Pipeline 6-vector order:  [ xx, yy, zz, xy, yz, xz ]
//     - strain vectors carry *tensor* shear (eps_xy = gamma_xy/2), matching
//       ansysWrapper::eps_as_loading (verified against load_loadstep()).
//     - stress vectors carry physical stress components in Pa.
//  ffth Mandel order:        [ 11, 22, 33, sqrt2*23, sqrt2*13, sqrt2*12 ]
//
//  All moduli in Pa, strains dimensionless (CRSS lives in Pa, so we must too).
//
//  Orientations are Bunge ZXZ Euler angles in RADIANS, one triple per grain id.
//  The same triple drives BOTH the elastic rotation (mvh::cubic_grain_mandel)
//  and the Schmid rotation downstream, so the two are always consistent.
//
//  Depends only on fft_homog.hpp + matviz_homog.hpp.  Pure C++17, no Qt, so it
//  stays unit-testable; the Qt orchestrator lives in stressanalysis_fft.cpp.
// ============================================================================
#pragma once
#include "fft_homog.hpp"
#include "matviz_homog.hpp"     // mvh::cubic_grain_mandel, mvh::Mat6
#include <array>
#include <vector>
#include <cstdint>
#include <cmath>
#include <random>
#include <memory>
#include <complex>
#include <stdexcept>

namespace fftsa {

using ffth::Mat6;
using Vec6 = std::array<double, 6>;

// Column indices of a per-voxel result row.  Kept identical to the meaningful
// subset of ansysWrapper's `tensor_components` enum so the HDF5 layout matches.
//   [0]=ID [1..3]=X,Y,Z [4..6]=UX,UY,UZ [7..12]=SX,SY,SZ,SXY,SYZ,SXZ
//   [13..18]=EpsX,EpsY,EpsZ,EpsXY,EpsYZ,EpsXZ [19]=USUM [20]=SEQV [21]=EpsEQV
enum ResCol { R_ID,R_X,R_Y,R_Z,R_UX,R_UY,R_UZ,
              R_SX,R_SY,R_SZ,R_SXY,R_SYZ,R_SXZ,
              R_EX,R_EY,R_EZ,R_EXY,R_EYZ,R_EXZ,
              R_USUM,R_SEQV,R_EEQV, R_NCOLS };

// ----------------------------------------------------------------------------
//  Basis conversion helpers  (pipeline <-> ffth Mandel)
// ----------------------------------------------------------------------------
//  pipeline strain index: 3=xy(12) 4=yz(23) 5=xz(13), shear = TENSOR strain.
//  Mandel index:          3=23     4=13     5=12,     factor sqrt2.
inline ffth::Vec6 strain_pipeline_to_mandel(const Vec6& e) {
    const double s2 = std::sqrt(2.0);
    return { e[0], e[1], e[2],
             s2 * e[4],    // Mandel[3]=sqrt2*eps23 ; pipeline[4]=eps_yz(tensor)
             s2 * e[5],    // Mandel[4]=sqrt2*eps13 ; pipeline[5]=eps_xz(tensor)
             s2 * e[3] };  // Mandel[5]=sqrt2*eps12 ; pipeline[3]=eps_xy(tensor)
}

//  Mandel stress -> pipeline stress [sx,sy,sz,sxy,syz,sxz]  (physical Pa).
inline Vec6 stress_mandel_to_pipeline(const ffth::Vec6& m) {
    const double is2 = 1.0 / std::sqrt(2.0);
    return { m[0], m[1], m[2],
             is2 * m[5],   // sxy = sig12 = Mandel[5]/sqrt2
             is2 * m[3],   // syz = sig23 = Mandel[3]/sqrt2
             is2 * m[4] }; // sxz = sig13 = Mandel[4]/sqrt2
}

//  Mandel strain -> engineering strain [ex,ey,ez, gxy,gyz,gxz] (gamma = 2*eps).
//  Used only to fill the EpsXY.. columns for HDF5 (ANSYS reports engineering).
inline Vec6 strain_mandel_to_engineering(const ffth::Vec6& m) {
    const double is2 = 1.0 / std::sqrt(2.0);
    return { m[0], m[1], m[2],
             2.0 * is2 * m[5],   // gxy = 2*eps12 = sqrt2*Mandel[5]
             2.0 * is2 * m[3],   // gyz = 2*eps23
             2.0 * is2 * m[4] }; // gxz = 2*eps13
}

// ----------------------------------------------------------------------------
//  6x6 inverse (Gauss-Jordan, partial pivot).  Returns false if singular.
// ----------------------------------------------------------------------------
inline bool invert6x6(const Mat6& A, Mat6& out) {
    double M[6][12];
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) { M[i][j] = A[i][j]; M[i][j + 6] = (i == j) ? 1.0 : 0.0; }
    }
    for (int c = 0; c < 6; ++c) {
        int piv = c;
        for (int r = c + 1; r < 6; ++r) if (std::abs(M[r][c]) > std::abs(M[piv][c])) piv = r;
        if (std::abs(M[piv][c]) < 1e-300) return false;
        for (int k = 0; k < 12; ++k) std::swap(M[c][k], M[piv][k]);
        const double d = M[c][c];
        for (int k = 0; k < 12; ++k) M[c][k] /= d;
        for (int r = 0; r < 6; ++r) {
            if (r == c) continue;
            const double f = M[r][c];
            for (int k = 0; k < 12; ++k) M[r][k] -= f * M[c][k];
        }
    }
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) out[i][j] = M[i][j + 6];
    return true;
}

// ----------------------------------------------------------------------------
//  The session
// ----------------------------------------------------------------------------
class FFTSolverSession {
public:
    struct StepResult {
        Vec6              macro_stress{};   // RVE-level [sx,sy,sz,sxy,syz,sxz], Pa
        Vec6              macro_strain{};   // applied (pipeline, tensor shear)
        std::vector<Vec6> voxel_stress;     // per SOLID voxel, pipeline stress (Pa)
        std::vector<int>  voxel_grain;      // parallel: grain id (>=1)
        std::vector<int>  voxel_idx;        // parallel: linear index iz*N*N+iy*N+ix
        int               iterations = 0;
        double            error      = 0.0;
    };

    // grain_field: per-voxel grain id, x-fastest (idx = (iz*ny+iy)*nx+ix).
    //              id 0 == void/empty; ids 1..G == grains.
    // orient_rad : per-grain Bunge ZXZ Euler angles (radians); index == grain id.
    //              orient_rad[0] is the (unused) void slot; size must be G+1.
    FFTSolverSession(int nx, int ny, int nz,
                     std::vector<int> grain_field,
                     std::vector<std::array<double,3>> orient_rad,
                     double C11 = 168.4e9, double C12 = 121.4e9, double C44 = 75.4e9,
                     double void_eta = 1e-4)         // soft-phase factor for pores
        : nx_(nx), ny_(ny), nz_(nz),
          N_(static_cast<std::size_t>(nx) * ny * nz),
          grain_(std::move(grain_field)),
          orient_(std::move(orient_rad))
    {
        if (grain_.size() != N_) throw std::invalid_argument("grain_field size != nx*ny*nz");
        const int G = static_cast<int>(orient_.size()) - 1;   // highest grain id
        if (G < 1) throw std::invalid_argument("need >=1 grain (orient size G+1)");

        // Material library indexed directly by grain id.  lib[g] = rotated
        // cubic stiffness (Mandel, Pa) for grain g.
        lib_.assign(orient_.size(), Mat6{});
        for (int g = 1; g <= G; ++g) {
            const auto& a = orient_[g];
            lib_[g] = mvh::cubic_grain_mandel(C11, C12, C44, a[0], a[1], a[2]);
        }
        // Void slot: a soft isotropic phase (eta * C11) to keep contrast finite.
        // If there is no void (dense structure) this is never referenced.
        lib_[0] = ffth::isotropic_C(void_eta * C11, 0.3);

        // Solid fraction (RVE-level scaling of averages already comes for free,
        // see note in solveLoadCase()).
        std::size_t solid = 0;
        for (int g : grain_) if (g != 0) ++solid;
        solid_fraction_ = N_ ? static_cast<double>(solid) / static_cast<double>(N_) : 1.0;

        // Copy of the local coordinate systems the downstream Schmid code reads:
        // local_cs[grain_id] = {phi1,Phi,phi2} in radians.  Index == grain id.
        local_cs_.assign(orient_.size(), std::vector<float>(3, 0.0f));
        for (std::size_t g = 0; g < orient_.size(); ++g)
            for (int k = 0; k < 3; ++k)
                local_cs_[g][k] = static_cast<float>(orient_[g][k]);
    }

    // Solver controls (forwarded to the FFT homogenizer).
    void set_tolerance(double t)  { tol_ = t; }
    void set_max_iters(int it)    { maxit_ = it; }

    double solid_fraction() const { return solid_fraction_; }
    const std::vector<std::vector<float>>& local_cs() const { return local_cs_; }
    int nx() const { return nx_; } int ny() const { return ny_; } int nz() const { return nz_; }

    // --- One load case -----------------------------------------------------
    //  macro_strain_pipeline: [exx,eyy,ezz, exy,eyz,exz] with TENSOR shear.
    //
    //  Note on RVE averaging: ffth averages sigma over ALL voxels (voids too).
    //  Because voids carry ~0 stress, that average already equals
    //  solid_fraction * <sigma>_solid, i.e. the RVE-level macro stress.  So we
    //  do NOT multiply by solid_fraction again (unlike the ANSYS path which
    //  averages solid-only and then scales).
    StepResult solveLoadCase(const Vec6& macro_strain_pipeline) {
        ensure_solver_built();

        const ffth::Vec6 E = strain_pipeline_to_mandel(macro_strain_pipeline);
        int it = 0;
        const ffth::Vec6 avg_m = H_->solve(E, &it);

        StepResult r;
        r.macro_strain = macro_strain_pipeline;
        r.macro_stress = stress_mandel_to_pipeline(avg_m);
        r.iterations   = it;
        r.error        = H_->last_error();

        r.voxel_stress.reserve(N_);
        r.voxel_grain.reserve(N_);
        r.voxel_idx.reserve(N_);

        // Pull the per-voxel stress field (Mandel components -> pipeline).
        const std::vector<std::complex<double>>* sc[6];
        for (int c = 0; c < 6; ++c) sc[c] = &H_->stress_component(c);

        for (std::size_t p = 0; p < N_; ++p) {
            const int g = grain_[p];
            if (g == 0) continue;                     // skip void voxels
            ffth::Vec6 sm;
            for (int c = 0; c < 6; ++c) sm[c] = (*sc[c])[p].real();
            r.voxel_stress.push_back(stress_mandel_to_pipeline(sm));
            r.voxel_grain.push_back(g);
            r.voxel_idx.push_back(static_cast<int>(p));
        }
        return r;
    }

    // Same as solveLoadCase but also returns per-voxel strain (engineering) so
    // the caller can fill full 22-column HDF5 rows.  Used only in phase 2.0.
    StepResult solveLoadCaseFull(const Vec6& macro_strain_pipeline,
                                 std::vector<Vec6>& voxel_strain_eng /*out*/) {
        StepResult r = solveLoadCase(macro_strain_pipeline);
        voxel_strain_eng.clear();
        voxel_strain_eng.reserve(r.voxel_idx.size());
        const std::vector<std::complex<double>>* ec[6];
        for (int c = 0; c < 6; ++c) ec[c] = &H_->strain_component(c);
        for (int p : r.voxel_idx) {
            ffth::Vec6 em;
            for (int c = 0; c < 6; ++c) em[c] = (*ec[c])[static_cast<std::size_t>(p)].real();
            voxel_strain_eng.push_back(strain_mandel_to_engineering(em));
        }
        return r;
    }

    // --- Phase 1.0: effective compliance/stiffness in the pipeline basis ----
    //  Six canonical unit tensor-strain solves.  Column j of C = macro stress
    //  produced by e_j.  Then S = C^{-1}.  Returns false if C is singular.
    bool computeCompliance(double S_out[6][6], double C_out[6][6] = nullptr,
                           int* total_iters = nullptr) {
        ensure_solver_built();
        Mat6 C{}; for (auto& row : C) row.fill(0.0);
        int itsum = 0;
        for (int j = 0; j < 6; ++j) {
            Vec6 e{}; e.fill(0.0); e[j] = 1.0;         // unit tensor strain e_j
            StepResult r = solveLoadCase(e);
            itsum += r.iterations;
            for (int i = 0; i < 6; ++i) C[i][j] = r.macro_stress[i];
        }
        // symmetrize small numerical asymmetry
        for (int i = 0; i < 6; ++i)
            for (int j = i + 1; j < 6; ++j) {
                const double m = 0.5 * (C[i][j] + C[j][i]);
                C[i][j] = C[j][i] = m;
            }
        Mat6 S{};
        if (!invert6x6(C, S)) return false;
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j) {
                S_out[i][j] = S[i][j];
                if (C_out) C_out[i][j] = C[i][j];
            }
        if (total_iters) *total_iters = itsum;
        return true;
    }

private:
    void ensure_solver_built() {
        if (H_) return;
        H_ = std::make_unique<ffth::FFTHomogenizer>(nx_, ny_, nz_);
        H_->set_materials(lib_);
        H_->set_phase_field(grain_);      // phase id == grain id (indexes lib_)
        H_->set_tolerance(tol_);
        H_->set_max_iterations(maxit_);
    }

    int nx_, ny_, nz_;
    std::size_t N_;
    std::vector<int>                   grain_;   // per-voxel grain id
    std::vector<std::array<double,3>>  orient_;  // per-grain Bunge ZXZ (rad)
    std::vector<Mat6>                  lib_;     // per-grain Mandel stiffness (Pa)
    std::vector<std::vector<float>>    local_cs_;
    double solid_fraction_ = 1.0;

    double tol_ = 1e-5;
    int    maxit_ = 500;
    std::unique_ptr<ffth::FFTHomogenizer> H_;
};

} // namespace fftsa

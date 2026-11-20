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
//     - effective S / C (computeCompliance) are in the ENGINEERING shear
//       basis (gamma = 2*eps, C44 = G), the same as the ANSYS path.
//  ffth Mandel order:        [ 11, 22, 33, sqrt2*23, sqrt2*13, sqrt2*12 ]
//
//  All moduli in Pa, strains dimensionless (CRSS lives in Pa, so we must too).
//
//  Orientations are Bunge ZXZ Euler angles in RADIANS, one triple per grain id.
//  The same triple drives BOTH the elastic rotation (mvh::aniso_grain_mandel)
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
#include <functional>

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
//  Symmetrize a PIPELINE-basis stiffness (column j = macro stress for a unit
//  ENGINEERING strain, gamma_j = 1 for shear). In that basis C is symmetric,
//  so this is the plain (C + C^T) / 2 average.
//
//  Mirrors symmetrizePipelineC() in stressresult.h; kept separate only because
//  this header is standalone and Mat6-typed.
// ----------------------------------------------------------------------------
inline void symmetrize_pipeline_C(Mat6& C) {
    for (int i = 0; i < 6; ++i)
        for (int j = i + 1; j < 6; ++j) {
            const double m = 0.5 * (C[i][j] + C[j][i]);
            C[i][j] = C[j][i] = m;
        }
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
    // Single-material constructor: any symmetry (cubic, hcp, ti, ...).
    // base_voigt_Pa is the full 6x6 Voigt stiffness in the material's own
    // axes, rotated here by each grain's orientation.
    FFTSolverSession(int nx, int ny, int nz,
                     std::vector<int> grain_field,
                     std::vector<std::array<double,3>> orient_rad,
                     const Mat6& base_voigt_Pa,
                     double void_eta = 1e-4)         // soft-phase factor for pores
        : nx_(nx), ny_(ny), nz_(nz),
          N_(static_cast<std::size_t>(nx) * ny * nz),
          grain_(std::move(grain_field)),
          orient_(std::move(orient_rad))
    {
        const int G = check_inputs();

        lib_.assign(orient_.size(), Mat6{});
        for (int g = 1; g <= G; ++g) {
            const auto& a = orient_[g];
            double Cv[6][6];
            for (int i = 0; i < 6; ++i)
                for (int j = 0; j < 6; ++j) Cv[i][j] = base_voigt_Pa[i][j];
            // Generic rotation, valid for any symmetry
            lib_[g] = mvh::aniso_grain_mandel(Cv, a[0], a[1], a[2]);
        }
        // C11 (element [0][0]) sizes the soft phase that stands in for pores
        finish_init(void_eta * base_voigt_Pa[0][0]);
    }

    // Previous cubic-only constructor, superseded by the Mat6 one above.
    // FFTSolverSession(int nx, int ny, int nz,
    //                  std::vector<int> grain_field,
    //                  std::vector<std::array<double,3>> orient_rad,
    //                  double C11 = 168.4e9, double C12 = 121.4e9, double C44 = 75.4e9,
    //                  double void_eta = 1e-4)         // soft-phase factor for pores
    //     : nx_(nx), ny_(ny), nz_(nz),
    //       N_(static_cast<std::size_t>(nx) * ny * nz),
    //       grain_(std::move(grain_field)),
    //       orient_(std::move(orient_rad))
    // {
    //     const int G = check_inputs();
    //
    //     // Material library indexed directly by grain id.  lib[g] = rotated
    //     // cubic stiffness (Mandel, Pa) for grain g.
    //     lib_.assign(orient_.size(), Mat6{});
    //     for (int g = 1; g <= G; ++g) {
    //         const auto& a = orient_[g];
    //         lib_[g] = mvh::cubic_grain_mandel(C11, C12, C44, a[0], a[1], a[2]);
    //     }
    //     finish_init(void_eta * C11);
    // }

    // Multi-phase overload: one Voigt stiffness per grain, given in that
    // grain's OWN material axes, rotated here by that grain's orientation.
    //
    // This is what a structure whose constituents differ -- a fiber-reinforced
    // composite -- feeds in: `grain_voigt_Pa[g]` is the constituent material of
    // grain g (index 0 is the unused void slot). Nothing here assumes cubic
    // symmetry, so a transversely isotropic fiber survives the trip intact.
    //
    // void_C11 sizes the soft phase that stands in for pores; pass the largest
    // C11 in play so the contrast stays comparable to the single-phase path.
    FFTSolverSession(int nx, int ny, int nz,
                     std::vector<int> grain_field,
                     std::vector<std::array<double,3>> orient_rad,
                     const std::vector<Mat6>& grain_voigt_Pa,
                     double void_C11,
                     double void_eta = 1e-4)
        : nx_(nx), ny_(ny), nz_(nz),
          N_(static_cast<std::size_t>(nx) * ny * nz),
          grain_(std::move(grain_field)),
          orient_(std::move(orient_rad))
    {
        const int G = check_inputs();
        if (static_cast<int>(grain_voigt_Pa.size()) < G + 1)
            throw std::invalid_argument("grain_voigt_Pa size < nGrains+1");

        lib_.assign(orient_.size(), Mat6{});
        for (int g = 1; g <= G; ++g) {
            const auto& a = orient_[g];
            double Cv[6][6];
            for (int i = 0; i < 6; ++i)
                for (int j = 0; j < 6; ++j) Cv[i][j] = grain_voigt_Pa[g][i][j];
            lib_[g] = mvh::aniso_grain_mandel(Cv, a[0], a[1], a[2]);
        }
        finish_init(void_eta * void_C11);
    }

    // Solver controls (forwarded to the FFT homogenizer).
    void set_tolerance(double t)  { tol_ = t; }
    void set_max_iters(int it)    { maxit_ = it; }

    // Which iterative scheme the homogenizer runs (see FFTHomogenizer::Scheme).
    // Auto picks CG whenever the cell has a void phase or a stiffness contrast
    // above 100 -- where the basic scheme needs thousands of iterations -- and
    // keeps the historical basic scheme for dense, moderate-contrast cells.
    enum class Scheme { Auto, Basic, CG };
    void set_scheme(Scheme s) { scheme_ = s; if (H_) H_->set_scheme(resolved_scheme()); }
    // Green-operator discretization (see FFTHomogenizer::Discretization).
    void set_discretization(ffth::FFTHomogenizer::Discretization d) { disc_ = d; if (H_) H_->set_discretization(d); }
    const char* discretization_name() const {
        return disc_ == ffth::FFTHomogenizer::Discretization::Rotated ? "rotated (Willot)" : "continuous (Moulinec-Suquet)";
    }
    ffth::FFTHomogenizer::Scheme resolved_scheme() const {
        if (scheme_ == Scheme::Basic) return ffth::FFTHomogenizer::Scheme::Basic;
        if (scheme_ == Scheme::CG)    return ffth::FFTHomogenizer::Scheme::CG;
        return (solid_fraction_ < 1.0 || phase_contrast() > 100.0)
                   ? ffth::FFTHomogenizer::Scheme::CG
                   : ffth::FFTHomogenizer::Scheme::Basic;
    }
    const char* scheme_name() const {
        return resolved_scheme() == ffth::FFTHomogenizer::Scheme::CG ? "cg" : "basic";
    }
    // Largest over smallest C11 among the grains (the void slot excluded).
    double phase_contrast() const {
        double lo = 1e300, hi = 0.0;
        for (std::size_t g = 1; g < lib_.size(); ++g) {
            const double c = lib_[g][0][0];
            if (!(c > 0.0)) continue;
            if (c < lo) lo = c;
            if (c > hi) hi = c;
        }
        return (lo > 0.0 && hi > 0.0 && lo < 1e300) ? hi / lo : 1.0;
    }

    double solid_fraction() const { return solid_fraction_; }
    // Mandel stiffness of grain g in the homogenizer's component order (yz, xz, xy shear).
    const Mat6& grain_stiffness_mandel(int g) const { return lib_[static_cast<std::size_t>(g)]; }
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
    StepResult solveLoadCase(const Vec6& macro_strain_pipeline,
                             const std::function<void(int, double)>& onIter = nullptr) {
        ensure_solver_built();

        const ffth::Vec6 E = strain_pipeline_to_mandel(macro_strain_pipeline);
        int it = 0;
        const ffth::Vec6 avg_m = H_->solve(E, &it, onIter);

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

    // Frobenius norm squared of a strain given as engineering components
    // [exx, eyy, ezz, gxy, gyz, gxz]: the shear entries enter as gamma^2/2.
    static double eng_strain_norm2(const Vec6& g) {
        return g[0] * g[0] + g[1] * g[1] + g[2] * g[2]
             + 0.5 * (g[3] * g[3] + g[4] * g[4] + g[5] * g[5]);
    }

    // Strain-concentration (sensitivity) index of one load case:
    //     <|eps|^2>_solid / |<eps>_solid|^2
    // the mean square of the local strain over the SOLID voxels divided by the
    // square of the mean strain of that same material. It is exactly 1 when
    // the material deforms uniformly (stretching-dominated plates, a solid
    // block) and grows when the strain localises in joints and bending walls,
    // where an error in the material law is amplified most (theorem T7 of the
    // lattice work). Pure bending drives the mean strain towards zero, so the
    // value is capped at 1e6. Normalising by the material's own mean strain,
    // not by the macroscopic strain, keeps the measure independent of how much
    // of the cell's deformation the voids absorb (see solid_strain_ratio).
    static double strain_concentration_index(const std::vector<Vec6>& voxel_strain_eng) {
        if (voxel_strain_eng.empty()) return 0.0;
        const double n = static_cast<double>(voxel_strain_eng.size());
        double acc = 0.0; Vec6 mean{}; mean.fill(0.0);
        for (const Vec6& g : voxel_strain_eng) {
            acc += eng_strain_norm2(g);
            for (int c = 0; c < 6; ++c) mean[c] += g[c];
        }
        for (int c = 0; c < 6; ++c) mean[c] /= n;
        const double num = acc / n, den = eng_strain_norm2(mean);
        if (!(num > 0.0)) return 0.0;
        return den > num * 1e-6 ? num / den : 1e6;
    }

    // Sensitivity tensor of the cell: the second moments of the six strain
    // fields, in Mandel components, averaged over the WHOLE cell (voids carry no
    // law change, so they drop out of the sum but stay in the volume):
    //     T[i][j][a][b] = (1/N) sum_{x in solid} eM_a^(i)(x) eM_b^(j)(x)
    // with eM = (exx, eyy, ezz, gxy/sqrt2, gyz/sqrt2, gxz/sqrt2). The six fields
    // must come from the unit ENGINEERING strain loads computeStiffnessMatrix()
    // applies (gamma_j = 1, i.e. e[j] = 0.5 for shear): then C[i][j] is the
    // energy pairing e^(i) : C : e^(j), and by Hill's lemma the first-order
    // change of the engineering-basis stiffness under a change dC of the
    // material law (given in Mandel form, Pa) is exactly
    //     dC[i][j] = sum_{a,b} T[i][j][a][b] * dC_Mandel[a][b],
    // and with dC = eta * C_s it reproduces eta * C to solver precision.
    // (Unit TENSOR shear loads would need T divided by 2 in the shear rows.)
    // Layout of the returned vector: ((i*6 + j)*6 + a)*6 + b, 1296 numbers.
    static std::vector<double> sensitivity_tensor(const std::vector<std::vector<Vec6>>& voxel_strain_eng_per_load,
                                                  std::size_t n_total_voxels) {
        std::vector<double> T(1296, 0.0);
        if (voxel_strain_eng_per_load.size() != 6 || n_total_voxels == 0) return T;
        const double r2 = std::sqrt(2.0);
        auto mandel = [&](const Vec6& g) {
            return Vec6{g[0], g[1], g[2], g[3] / r2, g[4] / r2, g[5] / r2};
        };
        const double invN = 1.0 / static_cast<double>(n_total_voxels);
        for (int i = 0; i < 6; ++i) {
            const auto& fi = voxel_strain_eng_per_load[static_cast<std::size_t>(i)];
            for (int j = i; j < 6; ++j) {
                const auto& fj = voxel_strain_eng_per_load[static_cast<std::size_t>(j)];
                if (fi.size() != fj.size()) continue;
                double acc[36] = {0.0};
                for (std::size_t v = 0; v < fi.size(); ++v) {
                    const Vec6 a = mandel(fi[v]), b = mandel(fj[v]);
                    for (int p = 0; p < 6; ++p)
                        for (int q = 0; q < 6; ++q) acc[p * 6 + q] += a[p] * b[q];
                }
                for (int p = 0; p < 6; ++p)
                    for (int q = 0; q < 6; ++q) {
                        const double m = acc[p * 6 + q] * invN;
                        T[static_cast<std::size_t>(((i * 6 + j) * 6 + p) * 6 + q)] = m;
                        if (j != i)     // T[j][i][q][p] is the same moment
                            T[static_cast<std::size_t>(((j * 6 + i) * 6 + q) * 6 + p)] = m;
                    }
            }
        }
        return T;
    }

    // Predicted first-order change of the engineering-basis stiffness for a
    // Mandel law change dC (6x6, Pa): dC_eff[i][j] = sum_ab T[i][j][a][b] dC[a][b].
    static Mat6 predict_stiffness_change(const std::vector<double>& T, const Mat6& dC_mandel) {
        Mat6 d{}; for (auto& r : d) r.fill(0.0);
        if (T.size() != 1296) return d;
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j) {
                double s = 0.0;
                for (int a = 0; a < 6; ++a)
                    for (int b = 0; b < 6; ++b)
                        s += T[static_cast<std::size_t>(((i * 6 + j) * 6 + a) * 6 + b)] * dC_mandel[a][b];
                d[i][j] = s;
            }
        return d;
    }

    // Share of the macroscopic strain actually carried by the material:
    //     <|eps|^2>_solid / |E|^2
    // 1 for a homogeneous solid; below 1 when the voids take up most of the
    // cell's deformation (a strut lattice loaded across its struts, a laminate
    // loaded across its void layers); above 1 when the material is strained
    // more than the cell as a whole. Tensor macro shear enters as 2 e^2.
    static double solid_strain_ratio(const std::vector<Vec6>& voxel_strain_eng,
                                     const Vec6& macro_strain_pipeline) {
        if (voxel_strain_eng.empty()) return 0.0;
        double acc = 0.0;
        for (const Vec6& g : voxel_strain_eng) acc += eng_strain_norm2(g);
        const Vec6& E = macro_strain_pipeline;
        const double En = E[0] * E[0] + E[1] * E[1] + E[2] * E[2]
                        + 2.0 * (E[3] * E[3] + E[4] * E[4] + E[5] * E[5]);
        return En > 0.0 ? (acc / static_cast<double>(voxel_strain_eng.size())) / En : 0.0;
    }

    // Same as solveLoadCase but also returns per-voxel strain (engineering) so
    // the caller can fill full 22-column HDF5 rows.  Used only in phase 2.0.
    StepResult solveLoadCaseFull(const Vec6& macro_strain_pipeline,
                                 std::vector<Vec6>& voxel_strain_eng /*out*/,
                                 const std::function<void(int, double)>& onIter = nullptr) {
        StepResult r = solveLoadCase(macro_strain_pipeline, onIter);
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
            Vec6 e{}; e.fill(0.0);
            e[j] = (j >= 3) ? 0.5 : 1.0;         // unit engineering strain (gamma_j = 1)
            StepResult r = solveLoadCase(e);
            itsum += r.iterations;
            for (int i = 0; i < 6; ++i) C[i][j] = r.macro_stress[i];
        }
        symmetrize_pipeline_C(C);
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
    // Shared by both constructors: validate and report the highest grain id.
    int check_inputs() const {
        if (grain_.size() != N_) throw std::invalid_argument("grain_field size != nx*ny*nz");
        const int G = static_cast<int>(orient_.size()) - 1;   // highest grain id
        if (G < 1) throw std::invalid_argument("need >=1 grain (orient size G+1)");
        return G;
    }

    // Shared by both constructors: void slot, solid fraction and the local
    // coordinate systems the downstream Schmid code reads. Only lib_[1..G]
    // differs between the single-material and multi-phase paths.
    void finish_init(double void_stiffness) {
        // Void slot: a soft isotropic phase to keep the contrast finite. If
        // there is no void (dense structure) this is never referenced.
        lib_[0] = ffth::isotropic_C(void_stiffness, 0.3);

        // Solid fraction (RVE-level scaling of averages already comes for free,
        // see note in solveLoadCase()).
        std::size_t solid = 0;
        for (int g : grain_) if (g != 0) ++solid;
        solid_fraction_ = N_ ? static_cast<double>(solid) / static_cast<double>(N_) : 1.0;

        // local_cs[grain_id] = {phi1,Phi,phi2} in radians. Index == grain id.
        local_cs_.assign(orient_.size(), std::vector<float>(3, 0.0f));
        for (std::size_t g = 0; g < orient_.size(); ++g)
            for (int k = 0; k < 3; ++k)
                local_cs_[g][k] = static_cast<float>(orient_[g][k]);
    }

    void ensure_solver_built() {
        if (H_) return;
        H_ = std::make_unique<ffth::FFTHomogenizer>(nx_, ny_, nz_);
        H_->set_materials(lib_);
        H_->set_phase_field(grain_);      // phase id == grain id (indexes lib_)
        H_->set_tolerance(tol_);
        H_->set_max_iterations(maxit_);
        H_->set_scheme(resolved_scheme());
        H_->set_discretization(disc_);
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
    Scheme scheme_ = Scheme::Auto;
    ffth::FFTHomogenizer::Discretization disc_ = ffth::FFTHomogenizer::Discretization::Continuous;
    std::unique_ptr<ffth::FFTHomogenizer> H_;
};

} // namespace fftsa

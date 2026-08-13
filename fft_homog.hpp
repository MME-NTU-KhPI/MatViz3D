// ============================================================================
//  fft_homog.hpp  -  Header-only FFT-based elasticity homogenization solver
//
//  Moulinec-Suquet "basic scheme" (Comput. Methods Appl. Mech. Eng., 1998).
//  Solves the Lippmann-Schwinger equation on a regular voxel grid to obtain
//  the local strain/stress fields and the effective (homogenized) stiffness
//  of a periodic RVE.
//
//  - No external dependencies (self-contained complex FFT: radix-2 + Bluestein,
//    so any grid size works, e.g. 5x5x5, 50x50x50, 128x128x128).
//  - Requires only C++17 and <complex>, <vector>, <array>, <cmath>.
//  - Anisotropic per-voxel stiffness supported (full 6x6 in Mandel notation),
//    so crystallographic grain orientations map in directly.
//
//  Conventions
//  -----------
//  Symmetric 2nd-order tensors are stored as 6-vectors in **Mandel** notation:
//      [ s11, s22, s33, sqrt2*s23, sqrt2*s13, sqrt2*s12 ]
//  In Mandel notation the double contraction sigma = C : eps becomes an ordinary
//  6x6 matrix-vector product, and C is symmetric/orthonormal (nicer than Voigt).
//
//  Voxel index layout: idx = (iz*ny + iy)*nx + ix   (x fastest).
// ============================================================================
#pragma once

#include <vector>
#include <array>
#include <complex>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <functional>

namespace ffth {

using cd  = std::complex<double>;
using Vec6 = std::array<double, 6>;
using Mat6 = std::array<std::array<double, 6>, 6>;

// ----------------------------------------------------------------------------
//  1D complex FFT (unnormalized).  Forward = e^{-i...}.  Handles any length N
//  via radix-2 when N is a power of two, otherwise Bluestein's algorithm.
// ----------------------------------------------------------------------------
namespace detail {

inline void fft_radix2(std::vector<cd>& a, bool inverse) {
    const std::size_t n = a.size();
    // bit-reversal permutation
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double ang = 2.0 * M_PI / static_cast<double>(len) * (inverse ? 1.0 : -1.0);
        const cd wlen(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len) {
            cd w(1.0, 0.0);
            for (std::size_t k = 0; k < len / 2; ++k) {
                const cd u = a[i + k];
                const cd v = a[i + k + len / 2] * w;
                a[i + k]           = u + v;
                a[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

inline std::size_t next_pow2(std::size_t n) {
    std::size_t m = 1;
    while (m < n) m <<= 1;
    return m;
}

// Real-flop count of one 1D complex transform of length n, matching the two
// code paths used below.  Radix-2 is the textbook 5*n*log2(n); Bluestein pays
// three radix-2 transforms of the padded length m plus the chirp multiplies
// (~6 flops per complex multiply-ish element), which is why an awkward grid
// size is several times more expensive per line than a power-of-two one.
inline double fft1d_flops(std::size_t n) {
    if (n < 2) return 0.0;
    const double dn = static_cast<double>(n);
    if ((n & (n - 1)) == 0) return 5.0 * dn * std::log2(dn);
    const double dm = static_cast<double>(next_pow2(2 * n - 1));
    return 3.0 * 5.0 * dm * std::log2(dm) + 12.0 * dm + 12.0 * dn;
}

} // namespace detail

// Estimated real flops of one in-place 3D transform on an nx*ny*nz grid: each
// pass runs one 1D transform per line along that axis.
inline double fft3d_flops(int nx, int ny, int nz) {
    const double dnx = static_cast<double>(nx), dny = static_cast<double>(ny),
                 dnz = static_cast<double>(nz);
    return dnz * dny * detail::fft1d_flops(static_cast<std::size_t>(nx))
         + dnz * dnx * detail::fft1d_flops(static_cast<std::size_t>(ny))
         + dny * dnx * detail::fft1d_flops(static_cast<std::size_t>(nz));
}

namespace detail {

// Forward DFT of arbitrary length via Bluestein (chirp-z).
inline void fft_bluestein(std::vector<cd>& a) {
    const std::size_t n = a.size();
    const std::size_t m = next_pow2(2 * n - 1);

    std::vector<cd> A(m, cd(0, 0)), B(m, cd(0, 0));
    // chirp:  w_j = exp(-i * pi * j^2 / n)
    std::vector<cd> chirp(n);
    for (std::size_t j = 0; j < n; ++j) {
        // (j*j) mod (2n) keeps the angle well-conditioned for large n
        const double ang = -M_PI * static_cast<double>((j * j) % (2 * n)) / static_cast<double>(n);
        chirp[j] = cd(std::cos(ang), std::sin(ang));
        A[j] = a[j] * chirp[j];
        B[j] = std::conj(chirp[j]);
    }
    for (std::size_t j = 1; j < n; ++j) B[m - j] = B[j];   // b is even

    fft_radix2(A, false);
    fft_radix2(B, false);
    for (std::size_t i = 0; i < m; ++i) A[i] *= B[i];
    fft_radix2(A, true);
    const double inv = 1.0 / static_cast<double>(m);
    for (std::size_t i = 0; i < m; ++i) A[i] *= inv;       // inverse normalization

    for (std::size_t k = 0; k < n; ++k) a[k] = A[k] * chirp[k];
}

inline void fft_forward(std::vector<cd>& a) {
    const std::size_t n = a.size();
    if (n < 2) return;
    if ((n & (n - 1)) == 0) fft_radix2(a, false);
    else                    fft_bluestein(a);
}

// Inverse (unnormalized: caller divides by N).  Uses the conjugate trick so we
// only need a forward transform for both directions.
inline void fft_inverse(std::vector<cd>& a) {
    for (auto& x : a) x = std::conj(x);
    fft_forward(a);
    for (auto& x : a) x = std::conj(x);
}

} // namespace detail

// ----------------------------------------------------------------------------
//  3D FFT of a single scalar field, in place.  forward=true => e^{-i...},
//  forward=false => inverse (unnormalized; divide by nx*ny*nz yourself).
// ----------------------------------------------------------------------------
inline void fft3d(std::vector<cd>& f, int nx, int ny, int nz, bool forward) {
    auto run = [&](std::vector<cd>& line) {
        if (forward) detail::fft_forward(line);
        else         detail::fft_inverse(line);
    };
    // Each pass transforms a set of mutually independent 1D lines, so the
    // lines are split across threads; each thread gets its own scratch
    // buffer (declared inside the parallel region, not shared).
    // along x (contiguous)
    {
        const std::size_t nlines = static_cast<std::size_t>(nz) * ny;
        #pragma omp parallel
        {
            std::vector<cd> line(nx);
            #pragma omp for schedule(static)
            for (std::size_t li = 0; li < nlines; ++li) {
                const int iz = static_cast<int>(li / ny);
                const int iy = static_cast<int>(li % ny);
                const std::size_t base = (static_cast<std::size_t>(iz) * ny + iy) * nx;
                for (int ix = 0; ix < nx; ++ix) line[ix] = f[base + ix];
                run(line);
                for (int ix = 0; ix < nx; ++ix) f[base + ix] = line[ix];
            }
        }
    }
    // along y
    {
        const std::size_t nlines = static_cast<std::size_t>(nz) * nx;
        #pragma omp parallel
        {
            std::vector<cd> line(ny);
            #pragma omp for schedule(static)
            for (std::size_t li = 0; li < nlines; ++li) {
                const int iz = static_cast<int>(li / nx);
                const int ix = static_cast<int>(li % nx);
                for (int iy = 0; iy < ny; ++iy)
                    line[iy] = f[(static_cast<std::size_t>(iz) * ny + iy) * nx + ix];
                run(line);
                for (int iy = 0; iy < ny; ++iy)
                    f[(static_cast<std::size_t>(iz) * ny + iy) * nx + ix] = line[iy];
            }
        }
    }
    // along z
    {
        const std::size_t nlines = static_cast<std::size_t>(ny) * nx;
        #pragma omp parallel
        {
            std::vector<cd> line(nz);
            #pragma omp for schedule(static)
            for (std::size_t li = 0; li < nlines; ++li) {
                const int iy = static_cast<int>(li / nx);
                const int ix = static_cast<int>(li % nx);
                for (int iz = 0; iz < nz; ++iz)
                    line[iz] = f[(static_cast<std::size_t>(iz) * ny + iy) * nx + ix];
                run(line);
                for (int iz = 0; iz < nz; ++iz)
                    f[(static_cast<std::size_t>(iz) * ny + iy) * nx + ix] = line[iz];
            }
        }
    }
}

// ----------------------------------------------------------------------------
//  Small helpers
// ----------------------------------------------------------------------------

// Isotropic stiffness in Mandel notation from Young's modulus E and Poisson nu.
inline Mat6 isotropic_C(double E, double nu) {
    const double lam = E * nu / ((1.0 + nu) * (1.0 - 2.0 * nu));
    const double mu  = 0.5 * E / (1.0 + nu);
    Mat6 C{};
    for (auto& row : C) row.fill(0.0);
    const double a = lam + 2.0 * mu;
    C[0][0] = C[1][1] = C[2][2] = a;
    C[0][1] = C[1][0] = C[0][2] = C[2][0] = C[1][2] = C[2][1] = lam;
    C[3][3] = C[4][4] = C[5][5] = 2.0 * mu;   // Mandel: shear diagonal = 2*mu
    return C;
}

// C : eps   (6x6 * 6)
inline Vec6 mat6_vec6(const Mat6& C, const Vec6& e) {
    Vec6 s{};
    for (int i = 0; i < 6; ++i) {
        double acc = 0.0;
        for (int j = 0; j < 6; ++j) acc += C[i][j] * e[j];
        s[i] = acc;
    }
    return s;
}

// ----------------------------------------------------------------------------
//  Homogenization solver
// ----------------------------------------------------------------------------
class FFTHomogenizer {
public:
    // nx,ny,nz : grid dimensions.  dx,dy,dz : voxel sizes (only the aspect
    // ratio matters; defaults to cubic voxels).
    FFTHomogenizer(int nx, int ny, int nz,
                   double dx = 1.0, double dy = 1.0, double dz = 1.0)
        : nx_(nx), ny_(ny), nz_(nz), N_(static_cast<std::size_t>(nx) * ny * nz),
          dx_(dx), dy_(dy), dz_(dz) {
        if (nx < 1 || ny < 1 || nz < 1) throw std::invalid_argument("bad grid size");
        phase_.assign(N_, 0);
    }

    // Register the material library: one 6x6 Mandel stiffness per phase id.
    void set_materials(std::vector<Mat6> C) { C_ = std::move(C); }

    // Per-voxel phase id (indexes into the material library). Size = nx*ny*nz,
    // x-fastest layout.  For MatViz3D this is your grain/phase map.
    void set_phase_field(std::vector<int> phase) {
        if (phase.size() != N_) throw std::invalid_argument("phase field size");
        phase_ = std::move(phase);
    }

    // Solver controls
    void set_tolerance(double tol)     { tol_ = tol; }
    void set_max_iterations(int it)    { maxit_ = it; }
    // Reference medium (Lame constants).  If left <=0, chosen automatically
    // from the phase-averaged stiffness.
    void set_reference(double lambda0, double mu0) { lam0_ = lambda0; mu0_ = mu0; }

    // Solve for one macroscopic strain E (Mandel 6-vector).
    // Fills eps_ / sig_ (per-voxel, Mandel) and returns the volume-average
    // stress <sigma> (Mandel).  Number of iterations returned via *iters.
    // onIter, if set, is invoked with (iteration, equilibrium_error) after
    // every iteration -- lets a caller stream live convergence data (e.g. to
    // a GUI) without waiting for the whole solve to finish.
    Vec6 solve(const Vec6& E, int* iters = nullptr,
               const std::function<void(int, double)>& onIter = nullptr) {
        if (C_.empty()) throw std::runtime_error("no materials set");
        pick_reference();

        // strain/stress fields as 6 complex components each
        for (int c = 0; c < 6; ++c) { eps_[c].assign(N_, cd(0,0)); sig_[c].assign(N_, cd(0,0)); }

        // init: eps = E everywhere
        #pragma omp parallel for schedule(static)
        for (std::size_t p = 0; p < N_; ++p)
            for (int c = 0; c < 6; ++c) eps_[c][p] = cd(E[c], 0.0);

        const FlopModel fm = flop_model();
        const auto t_start = std::chrono::steady_clock::now();
        auto seconds_since = [](const std::chrono::steady_clock::time_point& t) {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
        };
        double flops = 0.0;

        int it = 0;
        double err = 0.0;
        bool converged = false;
        for (; it < maxit_; ++it) {
            // (1) real-space constitutive update  sigma = C(x):eps(x)
            local_stress();

            // (2) forward FFT of stress
            for (int c = 0; c < 6; ++c) fft3d(sig_[c], nx_, ny_, nz_, true);

            // (3) equilibrium error from the transformed stress
            err = equilibrium_error();

            flops += fm.per_iter_head;

            if (onIter) onIter(it, err);

            // Progress: printed straight to stdout (no Qt dependency here, see
            // file header) so a slow solve is visibly making progress rather
            // than looking hung. Throttled to avoid flooding the console.
            // The flop figure is a model of the work done so far (see
            // flop_model()), not a hardware counter, so the rate is an estimate
            // of sustained throughput -- useful to compare grid sizes and
            // thread counts, not to be taken as a benchmark number.
            // (the converged iteration is reported by the summary below instead)
            if (err >= tol_ && (it == 0 || it % 20 == 0)) {
                const double secs = seconds_since(t_start);
                std::fprintf(stdout,
                             "[FFTHomogenizer::solve] iter %4d  err = %.3e  (tol = %.1e)  "
                             "%.2f s  %.2f GFLOP  %.2f GFLOP/s\n",
                             it, err, tol_, secs, flops * 1e-9,
                             secs > 0.0 ? flops * 1e-9 / secs : 0.0);
                std::fflush(stdout);
            }
            if (err < tol_) { converged = true; break; }

            // (4) FFT of current strain, apply Green operator, back-transform
            for (int c = 0; c < 6; ++c) fft3d(eps_[c], nx_, ny_, nz_, true);
            apply_green(E);
            const double invN = 1.0 / static_cast<double>(N_);
            for (int c = 0; c < 6; ++c) {
                fft3d(eps_[c], nx_, ny_, nz_, false);
                #pragma omp parallel for schedule(static)
                for (std::size_t p = 0; p < N_; ++p) eps_[c][p] *= invN;
            }
            flops += fm.per_iter_tail;
        }
        if (iters) *iters = it;

        // final real-space stress and its average
        local_stress();
        flops += fm.finalize;
        Vec6 avg{};
        for (int c = 0; c < 6; ++c) {
            double s = 0.0;
            #pragma omp parallel for reduction(+:s) schedule(static)
            for (std::size_t p = 0; p < N_; ++p) s += sig_[c][p].real();
            avg[c] = s / static_cast<double>(N_);
        }

        const double secs = seconds_since(t_start);
        const double gflops_rate = secs > 0.0 ? flops * 1e-9 / secs : 0.0;
        if (converged) {
            // `it` is the iteration index at which the error test passed, i.e.
            // the number of Green-operator updates actually applied.
            std::fprintf(stdout,
                         "[FFTHomogenizer::solve] converged at iteration %d: err = %.3e < tol = %.1e  "
                         "(%.2f s, %.2f GFLOP, %.2f GFLOP/s)\n",
                         it, err, tol_, secs, flops * 1e-9, gflops_rate);
            std::fflush(stdout);
        } else {
            std::fprintf(stderr,
                         "[FFTHomogenizer::solve] did not converge in %d iterations (err = %.3e, tol = %.1e)  "
                         "(%.2f s, %.2f GFLOP, %.2f GFLOP/s)\n",
                         maxit_, err, tol_, secs, flops * 1e-9, gflops_rate);
            std::fflush(stderr);
        }

        last_err_    = err;
        last_secs_   = secs;
        last_flops_  = flops;
        return avg;
    }

    // Full effective 6x6 stiffness: six unit-strain solves.  Column j is the
    // average stress produced by unit macro strain e_j.
    Mat6 effective_stiffness(int* total_iters = nullptr) {
        Mat6 Ceff{}; for (auto& r : Ceff) r.fill(0.0);
        int itsum = 0;
        for (int j = 0; j < 6; ++j) {
            Vec6 E{}; E.fill(0.0); E[j] = 1.0;
            int it = 0;
            const Vec6 avg = solve(E, &it);
            itsum += it;
            for (int i = 0; i < 6; ++i) Ceff[i][j] = avg[i];
        }
        // symmetrize (removes small numerical asymmetry)
        for (int i = 0; i < 6; ++i)
            for (int j = i + 1; j < 6; ++j) {
                const double m = 0.5 * (Ceff[i][j] + Ceff[j][i]);
                Ceff[i][j] = Ceff[j][i] = m;
            }
        if (total_iters) *total_iters = itsum;
        return Ceff;
    }

    // Accessors for the local fields (Mandel components; real parts are the
    // physical field after solve()).
    const std::vector<cd>& strain_component(int c) const { return eps_[c]; }
    const std::vector<cd>& stress_component(int c) const { return sig_[c]; }
    double last_error() const { return last_err_; }
    // Modelled flops and wall time of the last solve() (see flop_model()).
    double last_flops()   const { return last_flops_; }
    double last_seconds() const { return last_secs_; }
    double last_gflops_per_second() const {
        return last_secs_ > 0.0 ? last_flops_ * 1e-9 / last_secs_ : 0.0;
    }

private:
    // Modelled real-flop counts of the three parts of a solve() step.
    struct FlopModel {
        double per_iter_head;   // local_stress + 6 forward FFT + equilibrium_error
        double per_iter_tail;   // 6 forward FFT + Green operator + 6 inverse FFT
        double finalize;        // closing local_stress + the 6 volume averages
    };

    // Analytic flop estimate -- an operation count derived from the loops
    // below, not a measurement. Per voxel: the constitutive update is a 6x6
    // mat-vec (36 mul + 30 add ~= 72), the Green operator is ~150 (two 3x3
    // complex mat-vecs plus the symmetric outer products), and the equilibrium
    // error ~60 (one complex 3x3 mat-vec plus the norms). Those constants are
    // deliberately coarse: on any realistic grid the FFTs dominate.
    FlopModel flop_model() const {
        const double n  = static_cast<double>(N_);
        const double f3 = fft3d_flops(nx_, ny_, nz_);
        FlopModel fm{};
        fm.per_iter_head = 72.0 * n + 6.0 * f3 + 60.0 * n;
        fm.per_iter_tail = 6.0 * f3 + 150.0 * n + 6.0 * f3 + 12.0 * n; // +complex scaling
        fm.finalize      = 72.0 * n + 6.0 * n;
        return fm;
    }

    // sigma(x) = C(phase(x)) : eps(x)   (real part of eps used)
    void local_stress() {
        #pragma omp parallel for schedule(static)
        for (std::size_t p = 0; p < N_; ++p) {
            const Mat6& C = C_[static_cast<std::size_t>(phase_[p])];
            Vec6 e;
            for (int c = 0; c < 6; ++c) e[c] = eps_[c][p].real();
            const Vec6 s = mat6_vec6(C, e);
            for (int c = 0; c < 6; ++c) sig_[c][p] = cd(s[c], 0.0);
        }
    }

    // Choose the reference medium unless the user supplied one.
    //
    // The Moulinec-Suquet basic scheme is a fixed-point iteration whose
    // spectral radius is controlled by  C(x) - C0.  It converges only when the
    // reference is at least as stiff as every phase; the standard robust choice
    // is  mu0 = (mu_min + mu_max)/2  ,  lam0 = (lam_min + lam_max)/2 , which for
    // high contrast is dominated by the stiffest phase.  (A mean reference
    // diverges for large stiffness contrast.)
    void pick_reference() {
        if (mu0_ > 0.0) return;
        double muMin = 1e300, muMax = -1e300, lamMin = 1e300, lamMax = -1e300;
        std::vector<char> present(C_.size(), 0);
        for (std::size_t p = 0; p < N_; ++p) present[static_cast<std::size_t>(phase_[p])] = 1;
        for (std::size_t ph = 0; ph < C_.size(); ++ph) {
            if (!present[ph]) continue;
            const Mat6& C = C_[ph];
            // effective isotropic estimate of each phase's moduli
            const double mu  = 0.25 * (C[3][3] + C[4][4] + C[5][5]); // Mandel: diag = 2*mu
            const double lam = (C[0][1] + C[0][2] + C[1][2]) / 3.0;
            muMin = std::min(muMin, mu);   muMax = std::max(muMax, mu);
            lamMin = std::min(lamMin, lam); lamMax = std::max(lamMax, lam);
        }
        mu0_  = 0.5 * (muMin + muMax);
        lam0_ = 0.5 * (lamMin + lamMax);
        if (mu0_ <= 0.0) mu0_ = 1.0;                    // safety
        if (lam0_ < 0.0) lam0_ = 0.0;
    }

    // Frequency (wave number) along an axis, FFT ordering.
    static double freq(int k, int n, double d) {
        const int kk = (k <= n / 2) ? k : k - n;
        return static_cast<double>(kk) / (static_cast<double>(n) * d);
    }

    // Mandel 6-vector -> symmetric 3x3 tensor and back.
    static inline void mandel_to_tensor(const std::array<cd,6>& v, cd t[3][3]) {
        const double s = 1.0 / std::sqrt(2.0);
        t[0][0]=v[0]; t[1][1]=v[1]; t[2][2]=v[2];
        t[1][2]=t[2][1]=v[3]*s;
        t[0][2]=t[2][0]=v[4]*s;
        t[0][1]=t[1][0]=v[5]*s;
    }
    static inline void tensor_to_mandel(const cd t[3][3], std::array<cd,6>& v) {
        const double s = std::sqrt(2.0);
        v[0]=t[0][0]; v[1]=t[1][1]; v[2]=t[2][2];
        v[3]=t[1][2]*s; v[4]=t[0][2]*s; v[5]=t[0][1]*s;
    }

    // Apply the periodic Green operator of the reference medium in Fourier
    // space:  eps_hat(xi) <- eps_hat(xi) - Gamma0(xi) : sig_hat(xi),  xi != 0.
    // The xi=0 mode is set to the imposed macroscopic strain (times N because
    // the FFT here is unnormalized).
    void apply_green(const Vec6& E) {
        const double b = (lam0_ + mu0_) / (mu0_ * (lam0_ + 2.0 * mu0_));
        const double half_mu_inv = 1.0 / (2.0 * mu0_);

        #pragma omp parallel for collapse(3) schedule(static)
        for (int kz = 0; kz < nz_; ++kz)
        for (int ky = 0; ky < ny_; ++ky)
        for (int kx = 0; kx < nx_; ++kx) {
            const std::size_t p = (static_cast<std::size_t>(kz) * ny_ + ky) * nx_ + kx;

            if (kx == 0 && ky == 0 && kz == 0) {
                // mean strain = E  ->  eps_hat(0) = N*E
                const double Nn = static_cast<double>(N_);
                for (int c = 0; c < 6; ++c) eps_[c][p] = cd(E[c] * Nn, 0.0);
                continue;
            }

            double xi[3] = { freq(kx, nx_, dx_), freq(ky, ny_, dy_), freq(kz, nz_, dz_) };
            const double xi2 = xi[0]*xi[0] + xi[1]*xi[1] + xi[2]*xi[2];
            // unit direction (Gamma0 is homogeneous of degree 0 in xi)
            const double inv = 1.0 / std::sqrt(xi2);
            const double n[3] = { xi[0]*inv, xi[1]*inv, xi[2]*inv };

            // stress tensor at this mode
            std::array<cd,6> sv;
            for (int c = 0; c < 6; ++c) sv[c] = sig_[c][p];
            cd tau[3][3]; mandel_to_tensor(sv, tau);

            // m_i = tau_ij n_j ,  q = n_i tau_ij n_j
            cd m[3]; for (int i = 0; i < 3; ++i)
                m[i] = tau[i][0]*n[0] + tau[i][1]*n[1] + tau[i][2]*n[2];
            const cd q = m[0]*n[0] + m[1]*n[1] + m[2]*n[2];

            // Gamma0 : tau  =  (1/2mu0)(n_i m_j + n_j m_i) - b q n_i n_j
            cd g[3][3];
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    g[i][j] = half_mu_inv * (n[i]*m[j] + n[j]*m[i]) - b * q * n[i]*n[j];

            std::array<cd,6> gv; tensor_to_mandel(g, gv);
            for (int c = 0; c < 6; ++c) eps_[c][p] -= gv[c];
        }
    }

    // Equilibrium error:  ||div sigma|| / ||<sigma>||  (evaluated in Fourier).
    double equilibrium_error() {
        double num = 0.0;
        #pragma omp parallel for collapse(3) reduction(+:num) schedule(static)
        for (int kz = 0; kz < nz_; ++kz)
        for (int ky = 0; ky < ny_; ++ky)
        for (int kx = 0; kx < nx_; ++kx) {
            if (kx == 0 && ky == 0 && kz == 0) continue;
            const std::size_t p = (static_cast<std::size_t>(kz) * ny_ + ky) * nx_ + kx;
            const double xi[3] = { freq(kx, nx_, dx_), freq(ky, ny_, dy_), freq(kz, nz_, dz_) };
            cd tau[3][3];
            std::array<cd,6> sv; for (int c = 0; c < 6; ++c) sv[c] = sig_[c][p];
            mandel_to_tensor(sv, tau);
            // (div sigma)_i = sum_j xi_j tau_ij   (magnitude only)
            for (int i = 0; i < 3; ++i) {
                const cd di = tau[i][0]*xi[0] + tau[i][1]*xi[1] + tau[i][2]*xi[2];
                num += std::norm(di);
            }
        }
        // denominator: norm of the mean stress (xi=0 mode)
        std::array<cd,6> s0; for (int c = 0; c < 6; ++c) s0[c] = sig_[c][0];
        double den = 0.0; for (int c = 0; c < 6; ++c) den += std::norm(s0[c]);
        den = std::sqrt(den);
        if (den < 1e-300) den = 1.0;
        return std::sqrt(num) / den;
    }

    int nx_, ny_, nz_;
    std::size_t N_;
    double dx_, dy_, dz_;
    std::vector<int>  phase_;
    std::vector<Mat6> C_;
    std::array<std::vector<cd>, 6> eps_, sig_;

    double tol_   = 1e-6;
    int    maxit_ = 1000;
    double lam0_  = -1.0, mu0_ = -1.0;   // <=0 => auto
    double last_err_ = 0.0;
    double last_flops_ = 0.0;
    double last_secs_  = 0.0;
};

} // namespace ffth

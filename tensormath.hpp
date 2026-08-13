// ============================================================================
//  tensormath.hpp  -  Tensor math for MatViz3D's visualization layer.
//
//  Two independent groups of helpers, shared by the elastic-surface view
//  (material database window) and the per-voxel glyph/streamline overlays
//  (main 3D view):
//
//    1. Symmetric 3x3 eigen-decomposition + Kindlmann superquadric glyph
//       parameterization, for stress/strain tensor glyphs.
//    2. 6x6 <-> 4th-order elastic tensor conversions and directional elastic
//       properties (E, G, nu, linear compressibility, rotated components).
//
//  Pure C++17, no Qt and no GL, in the style of matviz_homog.hpp.  The Mat6/C4
//  aliases below are the *same types* as ffth::Mat6 / mvh::C4 (both are plain
//  std::array nests), so values pass between the two headers freely -- but this
//  header deliberately does not include fft_homog.hpp, so the material database
//  window does not drag in the FFT solver.
//
//  ---- Convention note, read before touching anything here --------------------
//  Three 6-vector orderings coexist in this codebase.  Getting them confused is
//  the single most likely source of a silently-rotated result:
//
//    Voigt    (material_properties table, GPa) : xx yy zz yz xz xy
//    Mandel   (mvh::to_mandel, ffth)           : xx yy zz yz xz xy, shear * sqrt2
//    pipeline (StiffnessMatrixResult, Pa)      : xx yy zz xy yz xz
//
//  Voigt and Mandel share an index order and differ only by the sqrt2 factors
//  (mvh::to_mandel uses I[6]={0,1,2,1,0,0}, J[6]={0,1,2,2,2,1}, i.e. index 3=yz,
//  4=xz, 5=xy -- exactly Voigt).  The pipeline order permutes the shear block;
//  kMandelToPipeline below is that permutation.
// ============================================================================
#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace mvt {

// ----------------------------------------------------------------------------
//  Basic types
// ----------------------------------------------------------------------------
/// Not M_PI: that is POSIX, and needs _USE_MATH_DEFINES before <cmath> on MSVC.
inline constexpr double kPi = 3.14159265358979323846;

using Vec3 = std::array<double, 3>;
using Mat3 = std::array<std::array<double, 3>, 3>;          // == mvh::Mat3
using Mat6 = std::array<std::array<double, 6>, 6>;          // == ffth::Mat6
using C4   = std::array<std::array<std::array<std::array<double, 3>, 3>, 3>, 3>;  // == mvh::C4

/// Symmetric 3x3 tensor.  Named fields, so no 6-vector ordering ambiguity.
struct Sym3 { double xx = 0, yy = 0, zz = 0, xy = 0, yz = 0, xz = 0; };

/// Eigen-decomposition of a Sym3.
///  - lambda is sorted ALGEBRAICALLY (lambda[0] >= lambda[1] >= lambda[2]), so
///    lambda[0]/e[0] is the true maximum-principal pair.  Stress is indefinite,
///    so this is NOT the same as sorting by magnitude -- see sortedMagnitudes().
///  - e is orthonormal and right-handed (det = +1).  The handedness fix is not
///    cosmetic: a left-handed frame mirrors the glyph and flips every analytic
///    normal inward, which renders as a uniformly black glyph under the lit
///    shader (looks like a lighting bug, is not one).
struct Eig3 {
    double lambda[3] = {0, 0, 0};
    Vec3   e[3]      = {{{1, 0, 0}}, {{0, 1, 0}}, {{0, 0, 1}}};
};

// ---- small vector helpers --------------------------------------------------
inline double dot(const Vec3& a, const Vec3& b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
inline Vec3   cross(const Vec3& a, const Vec3& b) {
    return { a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0] };
}
inline double norm(const Vec3& a) { return std::sqrt(dot(a, a)); }
inline Vec3   normalized(const Vec3& a) {
    const double n = norm(a);
    return (n > 1e-300) ? Vec3{ a[0]/n, a[1]/n, a[2]/n } : Vec3{ 0, 0, 1 };
}
inline Vec3   scaled(const Vec3& a, double s) { return { a[0]*s, a[1]*s, a[2]*s }; }

/// Direction on the unit sphere.  theta = polar from +z, phi = azimuth from +x.
inline Vec3 sphereDir(double theta, double phi) {
    const double st = std::sin(theta);
    return { st * std::cos(phi), st * std::sin(phi), std::cos(theta) };
}

/// An orthonormal frame whose first axis is n, spun about n by spinRad.
/// Columns of the result are (n, t1, t2) -- i.e. g[i][0] = n[i] -- matching the
/// index convention of mvh::rotate_C4 (C'_abcd = g_ia g_jb g_kc g_ld C_ijkl).
inline Mat3 frameAbout(const Vec3& n, double spinRad) {
    const Vec3 up = (std::abs(n[2]) > 0.9) ? Vec3{ 1, 0, 0 } : Vec3{ 0, 0, 1 };
    Vec3 t1 = normalized(cross(up, n));
    Vec3 t2 = cross(n, t1);
    const double c = std::cos(spinRad), s = std::sin(spinRad);
    const Vec3 u1{ c*t1[0] + s*t2[0], c*t1[1] + s*t2[1], c*t1[2] + s*t2[2] };
    const Vec3 u2{ -s*t1[0] + c*t2[0], -s*t1[1] + c*t2[1], -s*t1[2] + c*t2[2] };
    Mat3 g{};
    for (int i = 0; i < 3; ++i) { g[i][0] = n[i]; g[i][1] = u1[i]; g[i][2] = u2[i]; }
    return g;
}

// ----------------------------------------------------------------------------
//  Symmetric 3x3 eigensolver -- cyclic Jacobi
// ----------------------------------------------------------------------------
//  Deliberately NOT the analytic (Cardano/Smith) route.  That gets eigenvalues
//  cheaply and accurately, but recovers eigenvectors as normalized cross
//  products of rows of (A - lambda I).  When two eigenvalues coalesce that
//  matrix drops to rank 1, both candidate rows become parallel, and the cross
//  product divides catastrophic cancellation by catastrophic cancellation: the
//  vectors lose all significant digits and are not even mutually orthogonal.
//
//  Near-degeneracy is exactly the regime that matters here -- cl ~ cp is the
//  glyph shape branch switch, and lambda2 ~ lambda3 is where the streamline
//  cross-section frame is ill-conditioned.  Jacobi accumulates the eigenvector
//  matrix as a product of Givens rotations, each orthogonal to machine
//  precision, so the basis is orthonormal BY CONSTRUCTION regardless of
//  clustering.  Within a degenerate subspace the vectors are arbitrary, but
//  they are always a valid orthonormal basis -- which is all the geometry needs.
//
//  ~150 flops, ~0.2 us.  At 3000 glyphs that is 0.6 ms; not worth optimizing.
// ----------------------------------------------------------------------------
inline Eig3 eigenSym3(const Sym3& A)
{
    double a[3][3] = { { A.xx, A.xy, A.xz },
                       { A.xy, A.yy, A.yz },
                       { A.xz, A.yz, A.zz } };
    double v[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

    for (int sweep = 0; sweep < 24; ++sweep) {
        const double off = a[0][1]*a[0][1] + a[0][2]*a[0][2] + a[1][2]*a[1][2];
        if (off <= 1e-32 * (a[0][0]*a[0][0] + a[1][1]*a[1][1] + a[2][2]*a[2][2] + 1e-300))
            break;

        for (int p = 0; p < 2; ++p) {
            for (int q = p + 1; q < 3; ++q) {
                if (std::abs(a[p][q]) < 1e-300) continue;

                // Jacobi rotation annihilating a[p][q].
                const double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                const double t = (theta >= 0.0 ? 1.0 : -1.0) /
                                 (std::abs(theta) + std::sqrt(theta*theta + 1.0));
                const double c = 1.0 / std::sqrt(t*t + 1.0);
                const double s = t * c;

                const int r = 3 - p - q;   // the index not touched by this rotation
                const double app = a[p][p], aqq = a[q][q], apq = a[p][q];
                const double apr = a[p][r], aqr = a[q][r];

                a[p][p] = app - t * apq;
                a[q][q] = aqq + t * apq;
                a[p][q] = a[q][p] = 0.0;
                a[p][r] = a[r][p] = c * apr - s * aqr;
                a[q][r] = a[r][q] = s * apr + c * aqr;

                for (int i = 0; i < 3; ++i) {
                    const double vip = v[i][p], viq = v[i][q];
                    v[i][p] = c * vip - s * viq;
                    v[i][q] = s * vip + c * viq;
                }
            }
        }
    }

    // Collect, then sort algebraically descending.
    int idx[3] = { 0, 1, 2 };
    const double d[3] = { a[0][0], a[1][1], a[2][2] };
    if (d[idx[0]] < d[idx[1]]) std::swap(idx[0], idx[1]);
    if (d[idx[1]] < d[idx[2]]) std::swap(idx[1], idx[2]);
    if (d[idx[0]] < d[idx[1]]) std::swap(idx[0], idx[1]);

    Eig3 out;
    for (int k = 0; k < 3; ++k) {
        out.lambda[k] = d[idx[k]];
        out.e[k] = { v[0][idx[k]], v[1][idx[k]], v[2][idx[k]] };
    }

    // Force right-handedness (det = +1).  See the Eig3 comment.
    if (dot(cross(out.e[0], out.e[1]), out.e[2]) < 0.0)
        out.e[2] = scaled(out.e[2], -1.0);

    return out;
}

/// Deviatoric part: A - tr(A)/3 * I.  Removing the hydrostatic part is what
/// makes shear structure legible; it also drives trace to zero, which is
/// precisely why the Westin metrics below must never divide by the trace.
inline Sym3 deviatoric(const Sym3& A)
{
    const double m = (A.xx + A.yy + A.zz) / 3.0;
    return { A.xx - m, A.yy - m, A.zz - m, A.xy, A.yz, A.xz };
}

// ----------------------------------------------------------------------------
//  Westin anisotropy metrics
// ----------------------------------------------------------------------------
/// Eigenvalue MAGNITUDES sorted descending.
///
/// Stress is indefinite -- unlike the diffusion tensors superquadric glyphs
/// were invented for.  Feeding raw (signed) eigenvalues into the Westin
/// formulas divides by the trace, which goes to zero for deviatoric stress, and
/// negative half-axes would mirror the glyph and invert its normals.  Taking
/// magnitudes makes the denominator strictly positive for any nonzero tensor.
/// This is the single most important line in the glyph path.
inline void sortedMagnitudes(const Eig3& E, double m[3], int order[3])
{
    for (int k = 0; k < 3; ++k) { m[k] = std::abs(E.lambda[k]); order[k] = k; }
    if (m[0] < m[1]) { std::swap(m[0], m[1]); std::swap(order[0], order[1]); }
    if (m[1] < m[2]) { std::swap(m[1], m[2]); std::swap(order[1], order[2]); }
    if (m[0] < m[1]) { std::swap(m[0], m[1]); std::swap(order[0], order[1]); }
}

/// Westin linear / planar / spherical barycentric coordinates; cl+cp+cs == 1.
/// Expects m1 >= m2 >= m3 >= 0 (i.e. the output of sortedMagnitudes).
inline void westin(double m1, double m2, double m3, double& cl, double& cp, double& cs)
{
    const double sum = m1 + m2 + m3;
    if (sum < 1e-300) { cl = 0.0; cp = 0.0; cs = 1.0; return; }
    cl = (m1 - m2) / sum;
    cp = 2.0 * (m2 - m3) / sum;
    cs = 3.0 * m3 / sum;
}

// ----------------------------------------------------------------------------
//  Kindlmann superquadric glyphs
// ----------------------------------------------------------------------------
/// Signed power: preserves the sign of x while raising |x| to p.
inline double sgnpow(double x, double p)
{
    if (x == 0.0) return 0.0;
    return (x < 0.0 ? -1.0 : 1.0) * std::pow(std::abs(x), p);
}

/// Lower clamp on the superquadric exponents.  Keeps the dual exponents (2-a)
/// in [1.0, 1.88] so the normal formula never hits a negative power, and avoids
/// pow(0,0) at the parameter seam.  Also stops edges from becoming infinitely
/// sharp, which aliases badly at glyph scale.
inline constexpr double kSqExponentMin = 0.12;

struct SqShape {
    double alpha = 1.0;    ///< profile exponent along the distinguished axis
    double beta  = 1.0;    ///< cross-section exponent in the plane normal to it
    bool   xAxis = true;   ///< true: distinguished axis is x (linear-dominant)
};

/// Map Westin coordinates to superquadric exponents.
///
/// The governing requirement is rotational symmetry, and it is a correctness
/// constraint rather than an aesthetic one.  In the linear limit
/// (lambda1 >> lambda2 == lambda3) the tensor is rotationally symmetric about
/// e1, so e2 and e3 are arbitrary within their plane; any non-circular cross
/// section perpendicular to e1 would draw an orientation the tensor does not
/// determine.  So the cross-section exponent must go to 1 (round) as cp -> 0,
/// while the profile exponent along e1 goes small (boxy).  The planar limit is
/// the mirror image about e3.
///
/// Note this is round-in-the-degenerate-plane, edged-where-eigenvalues-differ.
/// Swapping alpha and beta here still passes a cl == cp crossover check (both
/// branches give alpha == beta there), so that test alone does not pin the
/// assignment down -- selfTest() checks the rotational symmetry directly.
inline SqShape kindlmann(double cl, double cp, double gamma)
{
    SqShape s;
    if (cl >= cp) {                                  // linear-dominant: axis = x
        s.alpha = std::pow(1.0 - cl, gamma);
        s.beta  = std::pow(1.0 - cp, gamma);
        s.xAxis = true;
    } else {                                         // planar-dominant: axis = z
        s.alpha = std::pow(1.0 - cp, gamma);
        s.beta  = std::pow(1.0 - cl, gamma);
        s.xAxis = false;
    }
    s.alpha = std::clamp(s.alpha, kSqExponentMin, 1.0);
    s.beta  = std::clamp(s.beta,  kSqExponentMin, 1.0);
    return s;
}

/// Point on the unit superquadric.  theta in [0,pi], phi in [0,2pi).
inline Vec3 superquadricPoint(const SqShape& s, double theta, double phi)
{
    const double st = sgnpow(std::sin(theta), s.alpha);
    const double ct = sgnpow(std::cos(theta), s.alpha);
    const double sp = sgnpow(std::sin(phi),   s.beta);
    const double cp = sgnpow(std::cos(phi),   s.beta);
    return s.xAxis ? Vec3{ ct, -st * sp, st * cp }
                   : Vec3{ st * cp, st * sp, ct };
}

/// Outward unit normal of the unit superquadric at (theta, phi).
///
/// Uses the dual-exponent identity: the normal field of a superquadric with
/// exponents (a, b) has the same parametric form with exponents (2-a, 2-b).
/// Analytic normals matter because the lit shader applies Blinn-Phong with a
/// specular term -- face normals from cross products would facet visibly on
/// exactly the flat faces that carry the shape information.
inline Vec3 superquadricNormal(const SqShape& s, double theta, double phi)
{
    const double a = 2.0 - s.alpha, b = 2.0 - s.beta;
    const double st = sgnpow(std::sin(theta), a);
    const double ct = sgnpow(std::cos(theta), a);
    const double sp = sgnpow(std::sin(phi),   b);
    const double cp = sgnpow(std::cos(phi),   b);
    const Vec3 n = s.xAxis ? Vec3{ ct, -st * sp, st * cp }
                           : Vec3{ st * cp, st * sp, ct };
    return normalized(n);
}

// ----------------------------------------------------------------------------
//  Elastic tensor conversions
// ----------------------------------------------------------------------------
/// Mandel/Voigt index -> the (i,j) pair it stands for.  Identical to the I/J
/// tables in mvh::to_mandel, and identical to Voigt: 3=yz, 4=xz, 5=xy.
inline constexpr int kMandelI[6] = { 0, 1, 2, 1, 0, 0 };
inline constexpr int kMandelJ[6] = { 0, 1, 2, 2, 2, 1 };

/// Mandel index -> pipeline index.  Pipeline shear order is xy, yz, xz, so
/// Mandel 3(yz)->4, 4(xz)->5, 5(xy)->3.
inline constexpr int kMandelToPipeline[6] = { 0, 1, 2, 4, 5, 3 };

inline double mandelFactor(int a) { return a < 3 ? 1.0 : std::sqrt(2.0); }

/// Voigt 6x6 (as stored in material_properties, any consistent unit) -> Mandel.
/// Voigt and Mandel share an index order, so this is pure scaling with no
/// permutation: M[a][b] = f_a * f_b * V[a][b].
inline Mat6 voigtC_to_mandel(const double Cv[6][6])
{
    Mat6 M{};
    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < 6; ++b)
            M[a][b] = mandelFactor(a) * mandelFactor(b) * Cv[a][b];
    return M;
}

/// Pipeline-basis stiffness (StiffnessMatrixResult::C -- unit TENSOR strain in,
/// physical stress out, Pa) -> Mandel.
///
/// Four cases, because only one side of each entry carries the sqrt2.  Working
/// a shear column through strain_pipeline_to_mandel / stress_mandel_to_pipeline
/// gives Cp[i][j] == 2 * Cp[j][i] for i<3<=j: the normal<->shear coupling block
/// of a pipeline C is asymmetric BY CONSTRUCTION.  Averaging it (as the solvers
/// historically did) destroys both entries -- see symmetrizePipelineC() in
/// stressresult.h.  This function assumes the un-averaged form.
inline Mat6 pipelineC_to_mandel(const double Cp[6][6])
{
    const double s2 = std::sqrt(2.0);
    Mat6 M{};
    for (int a = 0; a < 6; ++a) {
        for (int b = 0; b < 6; ++b) {
            const int pa = kMandelToPipeline[a], pb = kMandelToPipeline[b];
            if      (a < 3 && b < 3) M[a][b] = Cp[pa][pb];
            else if (a < 3)          M[a][b] = Cp[pa][pb] / s2;
            else if (b < 3)          M[a][b] = s2 * Cp[pa][pb];
            else                     M[a][b] = Cp[pa][pb];
        }
    }
    return M;
}

/// Mandel 6x6 -> 4th-order tensor, filling all minor and major index
/// symmetries.  Inverse of mvh::to_mandel.  Valid for stiffness and compliance
/// alike: in Mandel notation both carry the same sqrt2 convention, which is
/// exactly what makes C_m and S_m true matrix inverses of one another.
inline C4 mandel_to_C4(const Mat6& M)
{
    C4 T{};
    for (int a = 0; a < 6; ++a) {
        for (int b = 0; b < 6; ++b) {
            const double v = M[a][b] / (mandelFactor(a) * mandelFactor(b));
            const int i = kMandelI[a], j = kMandelJ[a];
            const int k = kMandelI[b], l = kMandelJ[b];
            T[i][j][k][l] = v;  T[j][i][k][l] = v;
            T[i][j][l][k] = v;  T[j][i][l][k] = v;
            T[k][l][i][j] = v;  T[l][k][i][j] = v;
            T[k][l][j][i] = v;  T[l][k][j][i] = v;
        }
    }
    return T;
}

/// 6x6 inverse (Gauss-Jordan, partial pivot).  Returns false if singular --
/// which a user-entered database row genuinely can be (a freshly added material
/// is all zeros), so callers must check rather than render NaN geometry.
inline bool invertMat6(const Mat6& A, Mat6& out)
{
    double M[6][12];
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) { M[i][j] = A[i][j]; M[i][j + 6] = (i == j) ? 1.0 : 0.0; }

    for (int c = 0; c < 6; ++c) {
        int piv = c;
        for (int r = c + 1; r < 6; ++r)
            if (std::abs(M[r][c]) > std::abs(M[piv][c])) piv = r;
        if (std::abs(M[piv][c]) < 1e-300) return false;
        if (piv != c) for (int j = 0; j < 12; ++j) std::swap(M[c][j], M[piv][j]);

        const double inv = 1.0 / M[c][c];
        for (int j = 0; j < 12; ++j) M[c][j] *= inv;
        for (int r = 0; r < 6; ++r) {
            if (r == c) continue;
            const double f = M[r][c];
            if (f == 0.0) continue;
            for (int j = 0; j < 12; ++j) M[r][j] -= f * M[c][j];
        }
    }
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) out[i][j] = M[i][j + 6];
    return true;
}

// ----------------------------------------------------------------------------
//  Directional elastic properties
// ----------------------------------------------------------------------------
/// Full contraction T_ijkl a_i b_j c_k d_l, evaluated in factored form
/// (3 + 9 + 27 + 81 = 120 multiplies instead of 3^4 * 4).
inline double contract4(const C4& T, const Vec3& a, const Vec3& b,
                        const Vec3& c, const Vec3& d)
{
    double si = 0.0;
    for (int i = 0; i < 3; ++i) {
        double sj = 0.0;
        for (int j = 0; j < 3; ++j) {
            double sk = 0.0;
            for (int k = 0; k < 3; ++k) {
                double sl = 0.0;
                for (int l = 0; l < 3; ++l) sl += T[i][j][k][l] * d[l];
                sk += sl * c[k];
            }
            sj += sk * b[j];
        }
        si += sj * a[i];
    }
    return si;
}

/// Directional Young's modulus.  E(n) = 1 / (S_ijkl n_i n_j n_k n_l).
inline double youngsE(const C4& S, const Vec3& n)
{
    const double snnnn = contract4(S, n, n, n, n);
    return (std::abs(snnnn) > 1e-300) ? 1.0 / snnnn : 0.0;
}

/// Linear compressibility beta(n) = S_ijkk n_i n_j.
inline double linearCompressibility(const C4& S, const Vec3& n)
{
    const Vec3 ex{ 1, 0, 0 }, ey{ 0, 1, 0 }, ez{ 0, 0, 1 };
    return contract4(S, n, n, ex, ex) + contract4(S, n, n, ey, ey) + contract4(S, n, n, ez, ez);
}

/// Shear modulus and Poisson's ratio are NOT functions of n alone -- both need
/// a second direction m perpendicular to n, and vary as m sweeps that plane.
/// Reporting "G(n)" without saying which of min/max/mean is meant is
/// meaningless, so both helpers return all three.
inline void shearG(const C4& S, const Vec3& n, int nAzimuth,
                   double& gMin, double& gMax, double& gMean)
{
    gMin = 1e300; gMax = -1e300; gMean = 0.0;
    const Mat3 f = frameAbout(n, 0.0);
    const Vec3 t1{ f[0][1], f[1][1], f[2][1] };
    const Vec3 t2{ f[0][2], f[1][2], f[2][2] };
    int used = 0;
    for (int k = 0; k < nAzimuth; ++k) {
        const double psi = kPi * double(k) / double(nAzimuth);    // G has period pi in m
        const double c = std::cos(psi), s = std::sin(psi);
        const Vec3 m{ c*t1[0] + s*t2[0], c*t1[1] + s*t2[1], c*t1[2] + s*t2[2] };
        const double denom = 4.0 * contract4(S, n, m, n, m);
        if (std::abs(denom) < 1e-300) continue;
        const double g = 1.0 / denom;
        gMin = std::min(gMin, g); gMax = std::max(gMax, g); gMean += g; ++used;
    }
    if (used == 0) { gMin = gMax = gMean = 0.0; return; }
    gMean /= double(used);
}

inline void poissonNu(const C4& S, const Vec3& n, int nAzimuth,
                      double& nuMin, double& nuMax, double& nuMean)
{
    nuMin = 1e300; nuMax = -1e300; nuMean = 0.0;
    const double snnnn = contract4(S, n, n, n, n);
    if (std::abs(snnnn) < 1e-300) { nuMin = nuMax = nuMean = 0.0; return; }

    const Mat3 f = frameAbout(n, 0.0);
    const Vec3 t1{ f[0][1], f[1][1], f[2][1] };
    const Vec3 t2{ f[0][2], f[1][2], f[2][2] };
    for (int k = 0; k < nAzimuth; ++k) {
        const double psi = kPi * double(k) / double(nAzimuth);    // nu has period pi in m
        const double c = std::cos(psi), s = std::sin(psi);
        const Vec3 m{ c*t1[0] + s*t2[0], c*t1[1] + s*t2[1], c*t1[2] + s*t2[2] };
        const double nu = -contract4(S, m, m, n, n) / snnnn;
        nuMin = std::min(nuMin, nu); nuMax = std::max(nuMax, nu); nuMean += nu;
    }
    nuMean /= double(nAzimuth);
}

/// Component (a,b,c,d) of T expressed in the frame whose columns are the new
/// basis vectors -- T'_abcd = g_ia g_jb g_kc g_ld T_ijkl, matching
/// mvh::rotate_C4 exactly, but evaluating one component instead of all 81.
inline double rotatedComponent(const C4& T, const Mat3& g, int a, int b, int c, int d)
{
    const Vec3 ga{ g[0][a], g[1][a], g[2][a] };
    const Vec3 gb{ g[0][b], g[1][b], g[2][b] };
    const Vec3 gc{ g[0][c], g[1][c], g[2][c] };
    const Vec3 gd{ g[0][d], g[1][d], g[2][d] };
    return contract4(T, ga, gb, gc, gd);
}

/// Rotated Mandel component T'[a][b], for the "any component of C or S" surface
/// mode.  a and b are Mandel/Voigt indices (0=xx 1=yy 2=zz 3=yz 4=xz 5=xy).
inline double rotatedMandelComponent(const C4& T, const Mat3& g, int a, int b)
{
    const double v = rotatedComponent(T, g, kMandelI[a], kMandelJ[a],
                                            kMandelI[b], kMandelJ[b]);
    return mandelFactor(a) * mandelFactor(b) * v;
}

/// Voigt-Reuss-Hill bulk modulus from Mandel stiffness and compliance.
inline double vrhBulkModulus(const Mat6& Cm, const Mat6& Sm)
{
    double kv = 0.0;
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) kv += Cm[i][j];
    kv /= 9.0;
    double sr = 0.0;
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) sr += Sm[i][j];
    const double kr = (std::abs(sr) > 1e-300) ? 1.0 / sr : 0.0;
    return 0.5 * (kv + kr);
}

/// True if the Mandel matrix has cubic symmetry, and if so report the Voigt
/// constants.  Used only to decide whether a Zener ratio is meaningful.
inline bool detectCubic(const Mat6& Cm, double& c11, double& c12, double& c44,
                        double relTol = 1e-6)
{
    c11 = Cm[0][0];
    c12 = Cm[0][1];
    c44 = Cm[3][3] / 2.0;                       // Mandel shear diagonal is 2*C44
    const double scale = std::abs(c11) + std::abs(c12) + std::abs(c44);
    if (scale < 1e-300) return false;
    const double tol = relTol * scale;

    for (int i = 0; i < 3; ++i) {
        if (std::abs(Cm[i][i] - c11) > tol) return false;
        if (std::abs(Cm[i + 3][i + 3] - 2.0 * c44) > tol) return false;
        for (int j = 0; j < 3; ++j) {
            if (i == j) continue;
            if (std::abs(Cm[i][j] - c12) > tol) return false;
        }
        for (int j = 3; j < 6; ++j) {
            if (std::abs(Cm[i][j]) > tol) return false;
            if (std::abs(Cm[j][i]) > tol) return false;
        }
    }
    for (int i = 3; i < 6; ++i)
        for (int j = 3; j < 6; ++j)
            if (i != j && std::abs(Cm[i][j]) > tol) return false;
    return true;
}

/// Zener anisotropy ratio; 1 for an elastically isotropic cubic crystal.
inline double zener(double c11, double c12, double c44)
{
    const double d = c11 - c12;
    return (std::abs(d) > 1e-300) ? 2.0 * c44 / d : 1.0;
}

} // namespace mvt

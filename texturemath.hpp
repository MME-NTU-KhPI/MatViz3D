#ifndef TEXTUREMATH_HPP
#define TEXTUREMATH_HPP

// ─────────────────────────────────────────────────────────────────────────────
//  texturemath.hpp -- Qt-free math behind the Texture Editor's plots.
//
//  Kept separate from TextureController so it can be unit-tested with a plain
//  g++ compile (no Qt, no moc), the way matviz_homog.hpp / fft_homog.hpp are.
//
//  Conventions
//  -----------
//  Orientations are Bunge ZXZ Euler triples (phi1, Phi, phi2) in DEGREES, the
//  same triples TextureLibrary::sampleNextBunge() produces and
//  buildGrainOrientations() feeds to both stress solvers.
//
//  The passive matrix g (sample -> crystal) is
//      g = Rz(phi2) Rx(Phi) Rz(phi1)
//  exactly as in TextureLibrary::bungeToMatrix() and mvh::bunge_zxz(). Its
//  transpose (crystal -> sample) is what maps a crystal direction into the
//  sample frame, which is what a pole figure draws.
//
//  Sample frame: x = RD, y = TD, z = ND. Pole figures are drawn looking down
//  ND with RD up and TD right, so screen_x = d_y, screen_y = -d_x.
// ─────────────────────────────────────────────────────────────────────────────

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace texmath {

#ifndef TEXMATH_PI
#define TEXMATH_PI 3.14159265358979323846
#endif

using Bunge = std::array<double, 3>;      // (phi1, Phi, phi2), degrees
using Mat3  = std::array<std::array<double, 3>, 3>;

inline constexpr double kDeg = TEXMATH_PI / 180.0;

// Crystal -> sample rotation (the transpose of the passive Bunge matrix).
inline Mat3 crystalToSample(const Bunge& b)
{
    const double p1 = b[0] * kDeg, P = b[1] * kDeg, p2 = b[2] * kDeg;
    const double c1 = std::cos(p1), s1 = std::sin(p1);
    const double c  = std::cos(P),  s  = std::sin(P);
    const double c2 = std::cos(p2), s2 = std::sin(p2);

    // passive g (sample -> crystal), identical to TextureLibrary::bungeToMatrix
    const Mat3 g{{
        { c1*c2 - s1*s2*c,   s1*c2 + c1*s2*c,   s2*s },
        {-c1*s2 - s1*c2*c,  -s1*s2 + c1*c2*c,   c2*s },
        { s1*s,             -c1*s,              c    }
    }};

    Mat3 a{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) a[i][j] = g[j][i];
    return a;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pole figures
// ─────────────────────────────────────────────────────────────────────────────

enum class PoleFamily { F100 = 0, F110 = 1, F111 = 2 };

// Symmetry-distinct members of each family (only one of each +-pair: h and -h
// are the same crystallographic direction, and the projection folds the lower
// hemisphere up anyway).
inline const std::vector<std::array<double, 3>>& familyDirections(PoleFamily f)
{
    static const std::vector<std::array<double, 3>> d100 = {
        {1,0,0}, {0,1,0}, {0,0,1}
    };
    static const std::vector<std::array<double, 3>> d110 = {
        {1,1,0}, {1,0,1}, {0,1,1}, {1,-1,0}, {1,0,-1}, {0,1,-1}
    };
    static const std::vector<std::array<double, 3>> d111 = {
        {1,1,1}, {1,-1,1}, {1,1,-1}, {1,-1,-1}
    };
    switch (f) {
    case PoleFamily::F110: return d110;
    case PoleFamily::F111: return d111;
    case PoleFamily::F100: default: return d100;
    }
}

// Stereographic projection of one sample-frame unit vector onto the equatorial
// disc, folded to the upper hemisphere. Returns screen coordinates in [-1, 1]
// (RD up, TD right); |(x,y)| <= 1 always.
inline std::array<float, 2> stereographic(double dx, double dy, double dz)
{
    if (dz < 0.0) { dx = -dx; dy = -dy; dz = -dz; }   // h and -h are equivalent
    const double denom = 1.0 + dz;
    const double X = (denom > 1e-12) ? dx / denom : 0.0;
    const double Y = (denom > 1e-12) ? dy / denom : 0.0;
    return { static_cast<float>(Y), static_cast<float>(-X) };
}

// One pole figure: every grain contributes |family| poles.
inline std::vector<std::array<float, 2>>
poleFigure(const std::vector<Bunge>& orientations, PoleFamily fam)
{
    const auto& dirs = familyDirections(fam);

    std::vector<std::array<float, 2>> out;
    out.reserve(orientations.size() * dirs.size());

    for (const auto& b : orientations) {
        const Mat3 a = crystalToSample(b);
        for (const auto& h : dirs) {
            const double n = std::sqrt(h[0]*h[0] + h[1]*h[1] + h[2]*h[2]);
            const double hx = h[0]/n, hy = h[1]/n, hz = h[2]/n;
            // sample-frame direction of this crystal direction
            const double dx = a[0][0]*hx + a[0][1]*hy + a[0][2]*hz;
            const double dy = a[1][0]*hx + a[1][1]*hy + a[1][2]*hz;
            const double dz = a[2][0]*hx + a[2][1]*hy + a[2][2]*hz;
            out.push_back(stereographic(dx, dy, dz));
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ODF on the reduced (0..90)^3 Euler box
//
//  This is a HISTOGRAM estimate: bin the symmetry-reduced orientations, divide
//  by the Euler volume element (which is sin(Phi), integrated exactly over each
//  bin, so the Phi -> 0 row does not blow up), smooth with a separable [1,2,1]
//  kernel, and rescale so the volume-weighted mean is 1. Values are therefore
//  in "x random" units: 1.0 == what a texture-free material would give.
//
//  Caveat: the smoothing kernel is applied in Euler coordinates, not in
//  orientation space, so it is slightly anisotropic near Phi = 0. Good enough
//  to read component strengths off; it is not a quantitative ODF inversion.
// ─────────────────────────────────────────────────────────────────────────────

struct OdfGrid {
    int    n   = 18;                  // bins per axis across 0..90 deg
    double bin = 5.0;                 // degrees
    std::vector<double> d;            // n^3, index = (i*n + j)*n + k  (phi1, Phi, phi2)

    double at(int i, int j, int k) const { return d[(std::size_t(i)*n + j)*n + k]; }
    double& at(int i, int j, int k)      { return d[(std::size_t(i)*n + j)*n + k]; }
    double  center(int idx) const        { return (idx + 0.5) * bin; }
};

// Exact integral of sin(Phi) over bin j -- the Euler-space volume weight.
inline double phiWeight(int j, double bin)
{
    const double lo = j * bin * kDeg, hi = (j + 1) * bin * kDeg;
    return std::cos(lo) - std::cos(hi);
}

// Bin count that keeps the histogram noise floor put as the grain count
// changes: aim for ~6 variants per bin. Measured "x random" maximum of a
// RANDOM texture (i.e. pure noise) with this rule and 3 smoothing passes:
//     200 grains -> 1.38    1000 -> 1.48    5000 -> 1.47   20000 -> 1.53
// so anything a contour draws above 2x random is signal, not sampling noise.
inline int adaptiveBins(std::size_t variantCount, double perBin = 6.0)
{
    if (variantCount == 0) return 6;
    int n = int(std::lround(std::cbrt(double(variantCount) / perBin)));
    if (n < 6)  n = 6;
    if (n > 18) n = 18;
    return n;
}

inline OdfGrid odfFromVariants(const std::vector<Bunge>& fzVariants,
                               int nbins = 18, int smoothPasses = 3)
{
    OdfGrid g;
    g.n   = nbins;
    g.bin = 90.0 / nbins;
    g.d.assign(std::size_t(nbins) * nbins * nbins, 0.0);

    // 1. raw counts
    for (const auto& b : fzVariants) {
        int i = int(b[0] / g.bin), j = int(b[1] / g.bin), k = int(b[2] / g.bin);
        if (i < 0) i = 0; if (i >= nbins) i = nbins - 1;
        if (j < 0) j = 0; if (j >= nbins) j = nbins - 1;
        if (k < 0) k = 0; if (k >= nbins) k = nbins - 1;
        g.at(i, j, k) += 1.0;
    }

    // 2. divide by the volume element -> density
    double totalCount = 0.0, totalVol = 0.0;
    for (int j = 0; j < nbins; ++j) {
        const double w = phiWeight(j, g.bin);
        for (int i = 0; i < nbins; ++i)
            for (int k = 0; k < nbins; ++k) {
                totalCount += g.at(i, j, k);
                totalVol   += w;
                g.at(i, j, k) = (w > 1e-15) ? g.at(i, j, k) / w : 0.0;
            }
    }
    if (totalCount <= 0.0 || totalVol <= 0.0) return g;

    // 3. separable [1,2,1] smoothing, edges replicated
    std::vector<double> tmp(g.d.size());
    auto clampIdx = [nbins](int v) { return v < 0 ? 0 : (v >= nbins ? nbins - 1 : v); };
    for (int pass = 0; pass < smoothPasses; ++pass) {
        for (int axis = 0; axis < 3; ++axis) {
            for (int i = 0; i < nbins; ++i)
                for (int j = 0; j < nbins; ++j)
                    for (int k = 0; k < nbins; ++k) {
                        double lo, mid = g.at(i, j, k), hi;
                        if (axis == 0)      { lo = g.at(clampIdx(i-1), j, k); hi = g.at(clampIdx(i+1), j, k); }
                        else if (axis == 1) { lo = g.at(i, clampIdx(j-1), k); hi = g.at(i, clampIdx(j+1), k); }
                        else                { lo = g.at(i, j, clampIdx(k-1)); hi = g.at(i, j, clampIdx(k+1)); }
                        tmp[(std::size_t(i)*nbins + j)*nbins + k] = 0.25*lo + 0.5*mid + 0.25*hi;
                    }
            g.d.swap(tmp);
        }
    }

    // 4. rescale so the volume-weighted mean is exactly 1 ("x random")
    double num = 0.0, den = 0.0;
    for (int j = 0; j < nbins; ++j) {
        const double w = phiWeight(j, g.bin);
        for (int i = 0; i < nbins; ++i)
            for (int k = 0; k < nbins; ++k) { num += g.at(i, j, k) * w; den += w; }
    }
    const double mean = (den > 0.0) ? num / den : 0.0;
    if (mean > 1e-15)
        for (auto& v : g.d) v /= mean;

    return g;
}

// Constant-phi2 section, linearly interpolated between the two bracketing bin
// centres. Returns an n*n slice indexed [i*n + j] = (phi1 bin i, Phi bin j).
inline std::vector<double> odfSection(const OdfGrid& g, double phi2_deg)
{
    const int n = g.n;
    double t = phi2_deg / g.bin - 0.5;             // position in bin-centre units
    int k0 = int(std::floor(t));
    double f = t - k0;
    int k1 = k0 + 1;
    if (k0 < 0)      { k0 = 0; k1 = 0; f = 0.0; }
    if (k1 > n - 1)  { k0 = n - 1; k1 = n - 1; f = 0.0; }

    std::vector<double> out(std::size_t(n) * n, 0.0);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            out[std::size_t(i)*n + j] = (1.0 - f) * g.at(i, j, k0) + f * g.at(i, j, k1);
    return out;
}

// Catmull-Rom upsample of a section, purely for drawing: the statistics support
// only ~10 deg bins at typical grain counts, but marching squares on an 8x8
// grid produces visible staircases. This adds no information -- it just makes
// the contour lines smooth. Output nodes sit at (u+0.5)/m in the same
// normalised [0,1] domain the source nodes use, so contour()'s mapping is
// unchanged.
inline std::vector<double> resampleSection(const std::vector<double>& sec, int n, int m)
{
    auto src = [&](int i, int j) {
        if (i < 0) i = 0; if (i >= n) i = n - 1;
        if (j < 0) j = 0; if (j >= n) j = n - 1;
        return sec[std::size_t(i)*n + j];
    };
    auto cr = [](double p0, double p1, double p2, double p3, double t) {
        return 0.5 * ((2.0*p1)
                    + (-p0 + p2) * t
                    + (2.0*p0 - 5.0*p1 + 4.0*p2 - p3) * t*t
                    + (-p0 + 3.0*p1 - 3.0*p2 + p3) * t*t*t);
    };

    std::vector<double> out(std::size_t(m) * m, 0.0);
    for (int u = 0; u < m; ++u) {
        const double xs = ((u + 0.5) / m) * n - 0.5;
        const int    i1 = int(std::floor(xs));
        const double tx = xs - i1;
        for (int v = 0; v < m; ++v) {
            const double ys = ((v + 0.5) / m) * n - 0.5;
            const int    j1 = int(std::floor(ys));
            const double ty = ys - j1;

            double col[4];
            for (int a = -1; a <= 2; ++a)
                col[a + 1] = cr(src(i1 + a, j1 - 1), src(i1 + a, j1),
                                src(i1 + a, j1 + 1), src(i1 + a, j1 + 2), ty);

            double val = cr(col[0], col[1], col[2], col[3], tx);
            if (val < 0.0) val = 0.0;      // cubic overshoot near sharp peaks
            out[std::size_t(u)*m + v] = val;
        }
    }
    return out;
}

inline double sectionMax(const std::vector<double>& sec)
{
    double m = 0.0;
    for (double v : sec) if (v > m) m = v;
    return m;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Marching squares -- contour lines of one section.
//  Output coordinates are normalised to [0,1]: x = phi1/90, y = Phi/90, which
//  is exactly the mapping the Euler plot already uses.
// ─────────────────────────────────────────────────────────────────────────────

struct Segment { float x1, y1, x2, y2; };

inline std::vector<Segment> contour(const std::vector<double>& sec, int n, double level)
{
    std::vector<Segment> segs;
    if (n < 2) return segs;

    auto val = [&](int i, int j) { return sec[std::size_t(i)*n + j]; };
    // grid node (i,j) sits at the bin centre
    auto px = [&](double i) { return float((i + 0.5) / n); };
    auto py = [&](double j) { return float((j + 0.5) / n); };

    for (int i = 0; i < n - 1; ++i) {
        for (int j = 0; j < n - 1; ++j) {
            const double a = val(i,   j  );      // corner 0 (x-, y-)
            const double b = val(i+1, j  );      // corner 1 (x+, y-)
            const double c = val(i+1, j+1);      // corner 2 (x+, y+)
            const double dd= val(i,   j+1);      // corner 3 (x-, y+)

            int idx = (a >= level ? 1 : 0) | (b >= level ? 2 : 0)
                    | (c >= level ? 4 : 0) | (dd >= level ? 8 : 0);
            if (idx == 0 || idx == 15) continue;

            auto lerp = [&](double v0, double v1) {
                const double dv = v1 - v0;
                if (std::fabs(dv) < 1e-30) return 0.5;
                double t = (level - v0) / dv;
                return t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
            };
            // edge crossing points
            const float e0x = px(i + lerp(a, b)),  e0y = py(j);              // a-b
            const float e1x = px(i + 1),           e1y = py(j + lerp(b, c)); // b-c
            const float e2x = px(i + lerp(dd, c)), e2y = py(j + 1);          // d-c
            const float e3x = px(i),               e3y = py(j + lerp(a, dd));// a-d

            // NB: not named "emit" -- that is a Qt keyword macro, and this
            // header is included from Qt translation units.
            auto addSeg = [&](float x1, float y1, float x2, float y2) {
                segs.push_back({x1, y1, x2, y2});
            };

            switch (idx) {
            case 1:  case 14: addSeg(e3x,e3y, e0x,e0y); break;
            case 2:  case 13: addSeg(e0x,e0y, e1x,e1y); break;
            case 3:  case 12: addSeg(e3x,e3y, e1x,e1y); break;
            case 4:  case 11: addSeg(e1x,e1y, e2x,e2y); break;
            case 6:  case 9:  addSeg(e0x,e0y, e2x,e2y); break;
            case 7:  case 8:  addSeg(e3x,e3y, e2x,e2y); break;
            case 5: case 10: {
                // saddle -- resolve with the cell average
                const bool centreInside = ((a + b + c + dd) / 4.0) >= level;
                const bool aInside = (idx == 5);   // corners a,c inside
                if (centreInside == aInside) { addSeg(e3x,e3y, e0x,e0y); addSeg(e1x,e1y, e2x,e2y); }
                else                         { addSeg(e0x,e0y, e1x,e1y); addSeg(e3x,e3y, e2x,e2y); }
                break;
            }
            default: break;
            }
        }
    }
    return segs;
}

// Conventional ODF contour levels, in x random.
inline const std::vector<double>& defaultLevels()
{
    static const std::vector<double> lv = { 1.0, 2.0, 4.0, 8.0, 16.0, 32.0 };
    return lv;
}

} // namespace texmath

#endif // TEXTUREMATH_HPP

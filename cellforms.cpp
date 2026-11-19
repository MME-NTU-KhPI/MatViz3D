#include "cellforms.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace cellforms {

namespace {

constexpr double kTau = 2.0 * M_PI;
constexpr double kInf = 1e300;
constexpr double kMaxPiece = 0.25;   ///< longest piece of a segment form, cell edges

using V3 = std::array<double,3>;

inline double wrapc(double v) { return v - std::round(v); }
inline double frac(double v)  { return v - std::floor(v); }

/// Uniform draws from the algorithm's generator (as mycelium.cpp does).
struct Rand {
    std::mt19937& g;
    std::uniform_real_distribution<double> U{ 0.0, 1.0 };
    double u() { return U(g); }
    std::uint64_t bits() { return (std::uint64_t(g()) << 32) ^ std::uint64_t(g()); }
};

// splitmix64 finalizer: a seeded hash of small integer tuples, so per-edge
// weights, plate choices and pit heights need no stored tables.
inline std::uint64_t mix(std::uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}
inline double hash01(std::uint64_t salt, int a, int b, int c = -1)
{
    std::uint64_t h = mix(salt ^ std::uint64_t(a + 1));
    h = mix(h ^ (std::uint64_t(b + 1) << 20));
    h = mix(h ^ (std::uint64_t(c + 1) << 40));
    return double(h >> 11) * (1.0 / 9007199254740992.0);
}

/// Collects pieces no longer than kMaxPiece, each moved so it starts in [0, 1)^3.
struct Pieces {
    std::vector<mycelium::Segment> out;
    void add(const V3& a, const V3& b, double w)
    {
        double L2 = 0.0;
        for (int i = 0; i < 3; ++i) L2 += (b[i] - a[i]) * (b[i] - a[i]);
        const int n = std::max(1, static_cast<int>(std::ceil(std::sqrt(L2) / kMaxPiece - 1e-9)));
        for (int k = 0; k < n; ++k) {
            mycelium::Segment s;
            for (int i = 0; i < 3; ++i) {
                const double p = a[i] + (b[i] - a[i]) * k / n, q = a[i] + (b[i] - a[i]) * (k + 1) / n;
                const double sh = std::floor(p);
                s.a[i] = static_cast<float>(p - sh);
                s.b[i] = static_cast<float>(q - sh);
            }
            s.w = static_cast<float>(w);
            out.push_back(s);
        }
    }
    void ball(const V3& p, double w) { add(p, p, w); }
};

// ── segment forms ───────────────────────────────────────────────────────────

void glassSponge(const Knobs& kn, Pieces& P)
{
    const double s  = 0.5;                                  // two squares per wall
    const double q  = s * (0.15 + 0.20 * kn.a);             // diagonals this far in from the corners
    const double wd = 0.5 + 0.5 * kn.b;
    for (int ax = 0; ax < 3; ++ax) {                        // the square grid: lines along every axis
        const int u = (ax + 1) % 3, w = (ax + 2) % 3;
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j) {
                V3 a{}; a[u] = i * s; a[w] = j * s;
                V3 b = a; b[ax] = 1.0;
                P.add(a, b, 1.0);
            }
    }
    const int planes = kn.c > 0.5 ? 3 : 2;                  // vertical walls, then the horizontal planes
    for (int ax = 0; ax < planes; ++ax) {
        const int u = (ax + 1) % 3, w = (ax + 2) % 3;
        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < 2; ++i)
                for (int j = 0; j < 2; ++j) {
                    if ((i + j + k) % 2) continue;          // checkerboard of braced squares
                    V3 o{}; o[ax] = k * s; o[u] = i * s; o[w] = j * s;
                    const double d[4][4] = { { q, 0, s, s - q }, { 0, q, s - q, s },
                                             { q, s, s, q },     { 0, s - q, s - q, 0 } };
                    for (const auto& e : d) {
                        V3 a = o, b = o;
                        a[u] += e[0]; a[w] += e[1]; b[u] += e[2]; b[w] += e[3];
                        P.add(a, b, wd);
                    }
                }
    }
}

void chiralTwist(const Knobs& kn, Pieces& P)
{
    const double R  = 0.18 + 0.14 * kn.a;
    const double al = (25.0 + 40.0 * kn.b) * M_PI / 180.0;
    const double wb = 0.6 + 0.5 * kn.c;
    const int    m  = 48;
    for (int ax = 0; ax < 3; ++ax) {                        // a ring in every face plane
        const int u = (ax + 1) % 3, w = (ax + 2) % 3;
        for (int k = 0; k < m; ++k) {
            V3 a{ 0.5, 0.5, 0.5 }, b{ 0.5, 0.5, 0.5 };
            a[ax] = b[ax] = 0.0;
            a[u] += R * std::cos(kTau * k / m);       a[w] += R * std::sin(kTau * k / m);
            b[u] += R * std::cos(kTau * (k + 1) / m); b[w] += R * std::sin(kTau * (k + 1) / m);
            P.add(a, b, 1.0);
        }
    }
    // One bar from the ring of the face z = 0 to the ring of the face x = 1;
    // the 2-fold axis through the midpoint of their shared edge maps one end
    // onto the other, and the 24 rotations of the cube give the other 11.
    const V3 A{ 0.5 + R * std::cos(al), 0.5 + R * std::sin(al), 0.0 };
    const V3 B{ 1.0, 0.5 - R * std::sin(al), 0.5 - R * std::cos(al) };
    std::set<std::array<long long,6>> seen;
    int perm[3] = { 0, 1, 2 };
    do {
        for (int sg = 0; sg < 8; ++sg) {
            double M[3][3] = {};
            for (int i = 0; i < 3; ++i) M[i][perm[i]] = (sg >> i & 1) ? -1.0 : 1.0;
            const double det = M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
                             - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0])
                             + M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
            if (det < 0.0) continue;                        // proper rotations only: keep the handedness
            V3 a{}, b{};
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) { a[i] += M[i][j] * (A[j] - 0.5); b[i] += M[i][j] * (B[j] - 0.5); }
            for (int i = 0; i < 3; ++i) { a[i] += 0.5; b[i] += 0.5; }
            std::array<long long,3> ka, kb;
            for (int i = 0; i < 3; ++i) {
                ka[i] = std::llround(frac(a[i] + 1e-9) * 1e6) % 1000000;
                kb[i] = std::llround(frac(b[i] + 1e-9) * 1e6) % 1000000;
            }
            if (kb < ka) std::swap(ka, kb);
            if (!seen.insert({ ka[0], ka[1], ka[2], kb[0], kb[1], kb[2] }).second) continue;
            P.add(a, b, wb);
        }
    } while (std::next_permutation(perm, perm + 3));
}

void woven(const Knobs& kn, Pieces& P)
{
    const int    L = kn.a < 0.5 ? 3 : 4;                    // layers; two yarns each way per layer
    const double h = 1.0 / L;
    const double A = 0.25 * h * (0.8 + 0.4 * kn.b);         // crimp: yarns of radius >= max(A, h/2 - A) touch
    const int    m = 32;
    for (int l = 0; l < L; ++l) {
        const double z0 = (l + 0.5) * h;
        for (int j = 0; j < 2; ++j)
            for (int k = 0; k < m; ++k) {
                const double t0 = double(k) / m, t1 = double(k + 1) / m;
                const double c0 = A * std::cos(kTau * t0 + M_PI * j), c1 = A * std::cos(kTau * t1 + M_PI * j);
                P.add({ t0, 0.5 * j, z0 + c0 }, { t1, 0.5 * j, z0 + c1 }, 1.0);   // warp over / under
                P.add({ 0.5 * j, t0, z0 - c0 }, { 0.5 * j, t1, z0 - c1 }, 1.0);   // weft under / over
            }
    }
}

void bouligand(const Knobs& kn, Pieces& P)
{
    // Eight plies, each turned by the next lattice direction (0, 27, 45, 63, 90,
    // 117, 135, 153 deg). A fibre is a flat ribbon, two lines h = 1/32 above and
    // below the ply's mid plane: a ribbon is solid for r >= h and touches the next
    // ply's for r >= 1/16 - h, so every fibre of radius >= 1/32 joins the plies.
    static const int d8[8][2] = { {1,0}, {2,1}, {1,1}, {1,2}, {0,1}, {-1,2}, {-1,1}, {-2,1} };
    const int L = 8;
    const double sp = 0.25 + 0.20 * kn.a;                   // target fibre spacing, cell edges
    const double h  = 0.25 / L;
    for (int l = 0; l < L; ++l) {
        const int dx = d8[l][0], dy = d8[l][1];
        const double len2 = double(dx * dx + dy * dy), len = std::sqrt(len2);
        const int M = std::max(1, static_cast<int>(std::lround(1.0 / (sp * len))));
        const double z = (l + 0.5) / L;
        for (int j = 0; j < M; ++j) {
            // The fibre dy x - dx y = j / M: its point nearest the origin, then one
            // full period along the lattice vector (dx, dy).
            const double t = double(j) / M;
            for (double dz : { -h, h }) {
                const V3 a{ t * dy / len2, -t * dx / len2, z + dz };
                P.add(a, { a[0] + dx, a[1] + dy, z + dz }, 1.0);
            }
        }
    }
}

void diamond(const Knobs& kn, Pieces& P)
{
    static const double fcc[4][3] = { {0, 0, 0}, {0, 0.5, 0.5}, {0.5, 0, 0.5}, {0.5, 0.5, 0} };
    static const double bond[4][3] = { {-1, -1, -1}, {1, 1, -1}, {1, -1, 1}, {-1, 1, 1} };
    const double wa = 1.0 + 1.6 * kn.a;                     // atom radius over bond radius
    for (const auto& f : fcc) {
        const V3 b{ f[0] + 0.25, f[1] + 0.25, f[2] + 0.25 };
        for (const auto& v : bond)
            P.add(b, { b[0] + 0.25 * v[0], b[1] + 0.25 * v[1], b[2] + 0.25 * v[2] }, 1.0);
        P.ball(b, wa);
        P.ball({ f[0], f[1], f[2] }, wa);
    }
}

void reentrant3D(const Knobs& kn, Pieces& P)
{
    const double H  = 0.55 + 0.35 * kn.a;                   // rib length; bars fall by H - 1/2
    const double wb = 0.6 + 0.5 * kn.b;
    const double wn = 1.0 + 0.8 * kn.c;
    P.add({ 0, 0, 0 }, { 0, 0, H }, 1.0);
    P.add({ 0.5, 0.5, 0.5 }, { 0.5, 0.5, 0.5 + H }, 1.0);
    for (double sx : { -0.5, 0.5 })
        for (double sy : { -0.5, 0.5 }) {
            P.add({ 0, 0, H }, { sx, sy, 0.5 }, wb);
            P.add({ 0.5, 0.5, 0.5 + H }, { 0.5 + sx, 0.5 + sy, 1.0 }, wb);
        }
    for (const V3& n : { V3{ 0, 0, 0 }, V3{ 0, 0, H }, V3{ 0.5, 0.5, 0.5 }, V3{ 0.5, 0.5, 0.5 + H } })
        P.ball(n, wn);
}

// ── fields ──────────────────────────────────────────────────────────────────

inline double gyroid(double x, double y, double z, double g[3])
{
    const double sx = std::sin(x), cx = std::cos(x), sy = std::sin(y), cy = std::cos(y), sz = std::sin(z), cz = std::cos(z);
    g[0] = cx * cy - sz * sx;
    g[1] = -sx * sy + cy * cz;
    g[2] = -sy * sz + cz * cx;
    return sx * cy + sy * cz + sz * cx;
}

/// Hard-core random sites in [0,1)^dim; `scale` stretches the metric per axis.
std::vector<V3> hardCoreSites(Rand& r, int n, int dim, double rmin, const V3& scale)
{
    std::vector<V3> pts;
    int tries = 0;
    while (static_cast<int>(pts.size()) < n) {
        V3 p{ r.u(), r.u(), dim == 3 ? r.u() : 0.0 };
        bool ok = true;
        if (++tries < 20000)
            for (const V3& q : pts) {
                double d2 = 0.0;
                for (int i = 0; i < dim; ++i) { const double d = wrapc(p[i] - q[i]) * scale[i]; d2 += d * d; }
                if (d2 < rmin * rmin) { ok = false; break; }
            }
        if (ok) pts.push_back(p);
    }
    return pts;
}

/// Random 2D waves (integer kx, ky, one of each +-k pair) with |k| in [lo, hi], amplitude 1/|k|,
/// scaled to unit variance; widens the band until at least `want` / 2 vectors are available.
std::vector<std::array<double,4>> waves2D(Rand& r, double lo, double hi, int want)
{
    std::vector<std::array<int,2>> ks;
    for (;;) {
        ks.clear();
        for (int a = -8; a <= 8; ++a)
            for (int b = 0; b <= 8; ++b) {
                if (b == 0 && a <= 0) continue;
                const double k = std::hypot(a, b);
                if (k >= lo && k <= hi) ks.push_back({ a, b });
            }
        if (static_cast<int>(ks.size()) * 2 >= want || hi > 12.0) break;
        lo = std::max(1.0, lo - 0.25); hi += 0.25;
    }
    for (int i = static_cast<int>(ks.size()) - 1; i > 0; --i)    // Fisher-Yates on the uniform draws
        std::swap(ks[static_cast<size_t>(i)], ks[static_cast<size_t>(std::min(i, static_cast<int>(r.u() * (i + 1))))]);
    const int n = std::min<int>(want, static_cast<int>(ks.size()));
    std::vector<std::array<double,4>> w;
    double var = 0.0;
    for (int i = 0; i < n; ++i) {
        const double k = std::hypot(ks[i][0], ks[i][1]);
        w.push_back({ double(ks[i][0]), double(ks[i][1]), 1.0 / k, kTau * r.u() });
        var += 0.5 / (k * k);
    }
    for (auto& x : w) x[2] /= std::sqrt(std::max(var, 1e-12));
    return w;
}

/// Sum of 2D waves and its gradient (per cell edge) at (x, y).
inline double sum2D(const std::vector<std::array<double,4>>& w, double x, double y, double g[2])
{
    double f = 0.0; g[0] = g[1] = 0.0;
    for (const auto& t : w) {
        const double ph = kTau * (t[0] * x + t[1] * y) + t[3];
        f += t[2] * std::cos(ph);
        const double d = -t[2] * kTau * std::sin(ph);
        g[0] += d * t[0]; g[1] += d * t[1];
    }
    return f;
}

// Products of cosine waves for the TPMS expansions.
using WaveList = std::vector<Wave>;
WaveList cosWave(int kx, int ky, int kz, double amp, double phase) { return { Wave{ { double(kx), double(ky), double(kz) }, amp, phase } }; }
WaveList sinWave(int kx, int ky, int kz, double amp) { return cosWave(kx, ky, kz, amp, -M_PI / 2.0); }
WaveList times(const WaveList& A, const WaveList& B)
{
    WaveList out;
    for (const Wave& a : A)
        for (const Wave& b : B)
            for (int s : { 1, -1 }) {   // cos a cos b = (cos(a + b) + cos(a - b)) / 2
                Wave w;
                for (int i = 0; i < 3; ++i) w.k[i] = a.k[i] + s * b.k[i];
                w.amp = 0.5 * a.amp * b.amp;
                w.phase = a.phase + s * b.phase;
                out.push_back(w);
            }
    return out;
}
WaveList plus(WaveList A, const WaveList& B) { A.insert(A.end(), B.begin(), B.end()); return A; }
/// One wave per +-k pair: k in the upper half space, equal vectors merged (as complex amplitudes).
WaveList canonical(const WaveList& in)
{
    std::map<std::tuple<int,int,int>, std::pair<double,double>> acc;
    for (Wave w : in) {
        int k[3] = { int(std::lround(w.k[0])), int(std::lround(w.k[1])), int(std::lround(w.k[2])) };
        const bool flip = k[0] < 0 || (k[0] == 0 && (k[1] < 0 || (k[1] == 0 && k[2] < 0)));
        if (flip) { for (int& x : k) x = -x; w.phase = -w.phase; }
        auto& z = acc[{ k[0], k[1], k[2] }];
        z.first  += w.amp * std::cos(w.phase);
        z.second += w.amp * std::sin(w.phase);
    }
    WaveList out;
    for (const auto& [k, z] : acc) {
        const bool zero = std::get<0>(k) == 0 && std::get<1>(k) == 0 && std::get<2>(k) == 0;
        const double amp = zero ? z.first : std::hypot(z.first, z.second);   // a constant keeps its sign
        if (std::abs(amp) < 1e-12) continue;
        out.push_back({ { double(std::get<0>(k)), double(std::get<1>(k)), double(std::get<2>(k)) },
                        amp, zero ? 0.0 : std::atan2(z.second, z.first) });
    }
    return out;
}

} // namespace

bool isSegmentKind(Kind k)
{
    switch (k) {
        case Kind::GlassSponge: case Kind::ChiralTwist: case Kind::Woven:
        case Kind::Bouligand: case Kind::Diamond: case Kind::Reentrant3D:
            return true;
        default:
            return false;
    }
}

const char* knobLabel(Kind k, int knob)
{
    static const char* t[12][3] = {
        { "diagonal offset",      "diagonal thickness",  "horizontal bracing" },   // GlassSponge
        { "ring radius",          "bar attachment",      "bar thickness" },        // ChiralTwist
        { "layers (3 | 4)",       "crimp",               nullptr },                // Woven
        { "fibre spacing",        nullptr,               nullptr },                // Bouligand
        { "atom size",            nullptr,               nullptr },                // Diamond
        { "rib length",           "bar thickness",       "node size" },            // Reentrant3D
        { "shell radius",         "pore size",           "spine thickness" },      // Radiolarian
        { "cells",                "stretch along Z",     "plate share" },          // Bone
        { "tracheids",            "latewood contrast",   "pit size" },             // Wood
        { "chambers",             "waviness",            "labyrinth scale" },      // Cuttlebone
        { "feed / kill",          "pattern scale",       nullptr },                // Turing
        { "scale ratio (2..4)",   "outer wall",          nullptr },                // HierGyroid
    };
    const int i = static_cast<int>(k);
    return (i >= 0 && i < 12 && knob >= 0 && knob < 3) ? t[i][knob] : nullptr;
}

std::vector<mycelium::Segment> segments(Kind k, const Knobs& kn)
{
    Pieces P;
    switch (k) {
        case Kind::GlassSponge: glassSponge(kn, P); break;
        case Kind::ChiralTwist: chiralTwist(kn, P); break;
        case Kind::Woven:       woven(kn, P); break;
        case Kind::Bouligand:   bouligand(kn, P); break;
        case Kind::Diamond:     diamond(kn, P); break;
        case Kind::Reentrant3D: reentrant3D(kn, P); break;
        default: break;
    }
    return P.out;
}

void Field::clear()
{
    m_img.clear(); m_img.shrink_to_fit();
    m_imgSite.clear(); m_imgSite.shrink_to_fit();
    m_g.clear(); m_h.clear();
    m_v.clear(); m_v.shrink_to_fit();
}

void Field::build(Kind k, const Knobs& kn, std::mt19937& rng)
{
    clear();
    m_kind = k;
    m_kn   = kn;
    m_wmin = 1.0;
    Rand r{ rng };
    switch (k) {
        case Kind::Radiolarian: {
            m_R[0] = 0.24 + 0.10 * kn.a;                     // outer shell
            m_R[1] = 0.5 * m_R[0];                          // inner shell
            m_R[2] = 0.55 * m_R[0];                         // shell about the corner
            m_pore = 0.7 + 0.6 * kn.b;
            m_spineW = 0.8 + 0.8 * kn.c;
            m_spineEnd = std::sqrt(3.0) / 2.0 - 0.9 * m_R[2];
            m_wmin = std::min(1.0, m_spineW);
            break;
        }
        case Kind::Bone: {
            const int n = static_cast<int>(std::lround(8.0 + 16.0 * kn.a));
            m_stretch = 1.0 + kn.b;
            m_plates  = 0.4 * kn.c;
            m_salt    = r.bits();
            const V3 scale{ 1.0, 1.0, 1.0 / m_stretch };
            const auto sites = hardCoreSites(r, n, 3, 0.6 * std::cbrt(1.0 / n), scale);
            for (int s = 0; s < n; ++s)
                for (int ox = -1; ox <= 1; ++ox)
                    for (int oy = -1; oy <= 1; ++oy)
                        for (int oz = -1; oz <= 1; ++oz) {
                            m_img.push_back({ sites[s][0] + ox, sites[s][1] + oy, (sites[s][2] + oz) / m_stretch });
                            m_imgSite.push_back(s);
                        }
            m_wmin = m_plates > 0.0 ? 0.6 : 0.75;
            break;
        }
        case Kind::Wood: {
            const int n = static_cast<int>(std::lround(5.0 + 9.0 * kn.a));
            m_late = 0.2 + 0.4 * kn.b;
            m_pit  = 0.03 + 0.04 * kn.c;
            m_salt = r.bits();
            const auto sites = hardCoreSites(r, n, 2, 0.55 / std::sqrt(double(n)), V3{ 1.0, 1.0, 1.0 });
            for (int s = 0; s < n; ++s)
                for (int ox = -1; ox <= 1; ++ox)
                    for (int oy = -1; oy <= 1; ++oy) {
                        m_img.push_back({ sites[s][0] + ox, sites[s][1] + oy, 0.0 });
                        m_imgSite.push_back(s);
                    }
            m_wmin = 0.75;
            break;
        }
        case Kind::Cuttlebone: {
            m_chambers = 1 + static_cast<int>(std::lround(2.0 * kn.a));
            m_wavy = 0.4 * kn.b;
            const double k0 = 1.6 + 1.4 * kn.c;
            m_g = waves2D(r, k0 - 0.5, k0 + 0.6, 6);
            m_h = waves2D(r, 2.0 * k0 - 0.6, 2.0 * k0 + 0.8, 10);
            m_wmin = 1.0;
            break;
        }
        case Kind::Turing: {
            // Gray-Scott, u + 2 v -> 3 v, explicit Euler on a periodic G^3 grid with the
            // 6-neighbour Laplacian normalized to (mean of neighbours - centre); dt = 0.8
            // keeps the scheme off its stability limit (dt = 1).
            const double f = 0.037 + 0.009 * kn.a, kk = 0.060 + 0.003 * kn.a;
            const int G = m_G = static_cast<int>(std::lround(32.0 + 16.0 * kn.b));
            const size_t n = size_t(G) * G * G;
            std::vector<float> u(n, 1.0f), v(n, 0.0f), nu(n), nv(n);
            auto at = [G](int i, int j, int l) { return (size_t(((i % G) + G) % G) * G + size_t(((j % G) + G) % G)) * G + size_t(((l % G) + G) % G); };
            for (int p = 0; p < 12; ++p) {                 // seed patches
                const int ci = static_cast<int>(r.u() * G), cj = static_cast<int>(r.u() * G), cl = static_cast<int>(r.u() * G);
                for (int i = -2; i <= 2; ++i)
                    for (int j = -2; j <= 2; ++j)
                        for (int l = -2; l <= 2; ++l) { u[at(ci + i, cj + j, cl + l)] = 0.5f; v[at(ci + i, cj + j, cl + l)] = 0.25f; }
            }
            for (float& x : v) x += 0.01f * static_cast<float>(r.u());
            const float dt = 0.8f, ff = float(f), fk = float(f + kk);
            for (int step = 0; step < 10000; ++step) {
                #pragma omp parallel for schedule(static)
                for (int i = 0; i < G; ++i)
                    for (int j = 0; j < G; ++j)
                        for (int l = 0; l < G; ++l) {
                            const size_t c = (size_t(i) * G + j) * G + l;
                            const size_t nb[6] = { at(i + 1, j, l), at(i - 1, j, l), at(i, j + 1, l),
                                                   at(i, j - 1, l), at(i, j, l + 1), at(i, j, l - 1) };
                            float su = 0.0f, sv = 0.0f;
                            for (size_t q : nb) { su += u[q]; sv += v[q]; }
                            const float uvv = u[c] * v[c] * v[c];
                            nu[c] = u[c] + dt * ((su / 6.0f - u[c]) - uvv + ff * (1.0f - u[c]));
                            nv[c] = v[c] + dt * (0.5f * (sv / 6.0f - v[c]) + uvv - fk * v[c]);
                        }
                u.swap(nu); v.swap(nv);
            }
            double mean = 0.0, var = 0.0;
            for (float x : v) mean += x;
            mean /= double(n);
            for (float x : v) var += (x - mean) * (x - mean);
            const double sd = std::sqrt(var / double(n));
            m_v.resize(n);
            for (size_t i = 0; i < n; ++i) m_v[i] = static_cast<float>((v[i] - mean) / std::max(sd, 1e-12));
            m_wmin = 0.0;
            break;
        }
        case Kind::HierGyroid: {
            m_scale = 2 + static_cast<int>(std::lround(2.0 * kn.a));
            m_outer = 0.08 + 0.07 * kn.b;
            m_wmin = 1.0;
            break;
        }
        default:
            break;
    }
}

double Field::at(double x, double y, double z) const
{
    switch (m_kind) {
        case Kind::Radiolarian: return radiolarian(x, y, z);
        case Kind::Bone:        return bone(x, y, z);
        case Kind::Wood:        return wood(x, y, z);
        case Kind::Cuttlebone:  return cuttlebone(x, y, z);
        case Kind::Turing:      return turing(x, y, z);
        case Kind::HierGyroid:  return hierGyroid(x, y, z);
        default:                return kInf;
    }
}

// Shells: distance to the sphere, +inf inside a pore. Pores of the outer shell
// sit on the 6 <100> and 12 <110> directions (17 and 12 degrees x pore size),
// of the inner and corner shells on the <100> directions only (24 degrees);
// the spines leave along <111>, between the pores.
double Field::radiolarian(double x, double y, double z) const
{
    double best = kInf;
    auto shell = [&](double qx, double qy, double qz, double R, bool rich) {
        const double r = std::sqrt(qx * qx + qy * qy + qz * qz) + 1e-12;
        const double ax = std::abs(qx) / r, ay = std::abs(qy) / r, az = std::abs(qz) / r;
        const double cosAxis = std::max({ ax, ay, az });
        const double deg = M_PI / 180.0;
        if (rich) {
            if (cosAxis > std::cos(17.0 * m_pore * deg)) return;
            const double c110 = std::max({ ax + ay, ay + az, az + ax }) / std::sqrt(2.0);
            if (c110 > std::cos(12.0 * m_pore * deg)) return;
        } else if (cosAxis > std::cos(24.0 * m_pore * deg)) {
            return;
        }
        best = std::min(best, std::abs(r - R));
    };
    const double qx = x - 0.5, qy = y - 0.5, qz = z - 0.5;
    shell(qx, qy, qz, m_R[0], true);
    shell(qx, qy, qz, m_R[1], false);
    shell(wrapc(x), wrapc(y), wrapc(z), m_R[2], false);
    const double s3 = 1.0 / std::sqrt(3.0);
    for (int sg = 0; sg < 8; ++sg) {
        const double u[3] = { (sg & 1 ? -s3 : s3), (sg & 2 ? -s3 : s3), (sg & 4 ? -s3 : s3) };
        const double t = std::clamp(qx * u[0] + qy * u[1] + qz * u[2], m_R[1], m_spineEnd);
        const double dx = qx - t * u[0], dy = qy - t * u[1], dz = qz - t * u[2];
        best = std::min(best, std::sqrt(dx * dx + dy * dy + dz * dz) / m_spineW);
    }
    return best;
}

// Voronoi in the stretched metric: the three nearest site images give the
// distance to a face ((d2 - d1) / 2) and to an edge ((d3 - d1) / 2). Every
// edge is a rod of its own (seeded) thickness, a seeded share of the faces a plate.
double Field::bone(double x, double y, double z) const
{
    const double p[3] = { x, y, z / m_stretch };
    double d[3] = { kInf, kInf, kInf };
    int    s[3] = { -1, -1, -1 };
    for (size_t i = 0; i < m_img.size(); ++i) {
        const double dx = p[0] - m_img[i][0], dy = p[1] - m_img[i][1], dz = p[2] - m_img[i][2];
        const double d2 = dx * dx + dy * dy + dz * dz;
        if (d2 >= d[2]) continue;
        int j = 2;
        while (j > 0 && d2 < d[j - 1]) { d[j] = d[j - 1]; s[j] = s[j - 1]; --j; }
        d[j] = d2; s[j] = m_imgSite[i];
    }
    for (double& v : d) v = std::sqrt(v);
    int e[3] = { s[0], s[1], s[2] };
    std::sort(e, e + 3);
    const double rod = 0.75 + 0.5 * hash01(m_salt, e[0], e[1], e[2]);
    double F = 0.5 * (d[2] - d[0]) / rod;
    const int fa = std::min(s[0], s[1]), fb = std::max(s[0], s[1]);
    if (m_plates > 0.0 && hash01(m_salt ^ 0x5bd1e995ull, fa, fb) < m_plates)
        F = std::min(F, 0.5 * (d[1] - d[0]) / 0.6);
    return F;
}

// Tracheids: the 2D Voronoi wall between the two nearest site images, thicker
// in the latewood band about x = 1/2; three pits per wall at seeded heights.
// A pit keeps a pit radius clear of the corners where three walls meet
// ((d3 - d1) / 2 is the distance to such a corner), so it can only open a
// window in its own wall and never cut a piece of wall loose.
double Field::wood(double x, double y, double z) const
{
    double d[3] = { kInf, kInf, kInf };
    int    k[3] = { -1, -1, -1 };
    for (size_t i = 0; i < m_img.size(); ++i) {
        const double dx = x - m_img[i][0], dy = y - m_img[i][1];
        const double d2 = dx * dx + dy * dy;
        if (d2 >= d[2]) continue;
        int j = 2;
        while (j > 0 && d2 < d[j - 1]) { d[j] = d[j - 1]; k[j] = k[j - 1]; --j; }
        d[j] = d2; k[j] = static_cast<int>(i);
    }
    for (double& v : d) v = std::sqrt(v);
    const double late = 0.5 - 0.5 * std::cos(kTau * x);
    const double w = 0.75 + m_late * late * late * late;
    const double wall = 0.5 * (d[1] - d[0]) / w;
    if (0.5 * (d[2] - d[0]) < 2.0 * m_pit) return wall;
    const size_t i0 = static_cast<size_t>(k[0]), i1 = static_cast<size_t>(k[1]);
    const double mx = 0.5 * (m_img[i0][0] + m_img[i1][0]), my = 0.5 * (m_img[i0][1] + m_img[i1][1]);
    const int a = std::min(m_imgSite[i0], m_imgSite[i1]), b = std::max(m_imgSite[i0], m_imgSite[i1]);
    for (int p = 0; p < 3; ++p) {
        const double dz = wrapc(z - hash01(m_salt, a, b, p));
        if ((x - mx) * (x - mx) + (y - my) * (y - my) + dz * dz < m_pit * m_pit) return kInf;
    }
    return wall;
}

// Walls: the zero set of a 2D labyrinth field g plus a corrugation h whose
// amplitude grows from a chamber's floor to just under its ceiling, as a
// distance |W| / |grad W|; septa every 1 / chambers along Z, 1.4 times thicker.
double Field::cuttlebone(double x, double y, double z) const
{
    double gg[2], gh[2];
    const double G = sum2D(m_g, x, y, gg), Hh = sum2D(m_h, x, y, gh);
    const double zc = frac(z * m_chambers);
    const double A = m_wavy * std::sin(M_PI * zc) * zc;
    const double W = G + A * Hh;
    const double gx = gg[0] + A * gh[0], gy = gg[1] + A * gh[1];
    const double walls = std::abs(W) / std::max(std::hypot(gx, gy), 1e-9);
    const double septa = std::abs(z * m_chambers - std::round(z * m_chambers)) / m_chambers;
    return std::min(walls, septa / 1.4);
}

// Periodic Catmull-Rom interpolation of the activator (node i at (i + 1/2) / G),
// negated so the solid {F <= c} is where the activator is high.
double Field::turing(double x, double y, double z) const
{
    const int G = m_G;
    if (m_v.empty()) return 0.0;
    auto cr = [](double p0, double p1, double p2, double p3, double t) {
        return p1 + 0.5 * t * (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + t * (3.0 * (p1 - p2) + p3 - p0)));
    };
    const double fx = x * G - 0.5, fy = y * G - 0.5, fz = z * G - 0.5;
    const int ix = static_cast<int>(std::floor(fx)), iy = static_cast<int>(std::floor(fy)), iz = static_cast<int>(std::floor(fz));
    const double tx = fx - ix, ty = fy - iy, tz = fz - iz;
    auto w = [G](int i) { return ((i % G) + G) % G; };
    double py[4];
    for (int a = 0; a < 4; ++a) {
        double pz[4];
        for (int b = 0; b < 4; ++b) {
            double px[4];
            for (int c = 0; c < 4; ++c)
                px[c] = m_v[(size_t(w(ix - 1 + c)) * G + size_t(w(iy - 1 + a))) * G + size_t(w(iz - 1 + b))];
            pz[b] = cr(px[0], px[1], px[2], px[3], tx);
        }
        py[a] = cr(pz[0], pz[1], pz[2], pz[3], tz);
    }
    return -cr(py[0], py[1], py[2], py[3], ty);
}

double Field::hierGyroid(double x, double y, double z) const
{
    double g[3];
    const double f1 = gyroid(kTau * x, kTau * y, kTau * z, g);
    const double big = std::abs(f1) / std::max(kTau * std::sqrt(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]), 1e-12);
    if (big > m_outer) return kInf;
    const double s = kTau * m_scale;
    const double f2 = gyroid(s * x, s * y, s * z, g);
    return std::abs(f2) / std::max(s * std::sqrt(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]), 1e-12);
}

std::vector<Wave> tpmsWaves(Surface s)
{
    auto C = [](int kx, int ky, int kz) { return cosWave(kx, ky, kz, 1.0, 0.0); };
    auto S = [](int kx, int ky, int kz) { return sinWave(kx, ky, kz, 1.0); };
    auto scaled = [](WaveList w, double f) { for (Wave& x : w) x.amp *= f; return w; };
    switch (s) {
        case Surface::Neovius:
            // 3 (cos x + cos y + cos z) + 4 cos x cos y cos z
            return canonical(plus(plus(plus(scaled(C(1, 0, 0), 3.0), scaled(C(0, 1, 0), 3.0)), scaled(C(0, 0, 1), 3.0)),
                                  scaled(times(times(C(1, 0, 0), C(0, 1, 0)), C(0, 0, 1)), 4.0)));
        case Surface::FischerKochS:
            // cos 2x sin y cos z + cos x cos 2y sin z + sin x cos y cos 2z
            return canonical(plus(plus(times(times(C(2, 0, 0), S(0, 1, 0)), C(0, 0, 1)),
                                       times(times(C(1, 0, 0), C(0, 2, 0)), S(0, 0, 1))),
                                  times(times(S(1, 0, 0), C(0, 1, 0)), C(0, 0, 2))));
        case Surface::Lidinoid: {
            // (sin 2x cos y sin z + sin 2y cos z sin x + sin 2z cos x sin y) / 2
            //   - (cos 2x cos 2y + cos 2y cos 2z + cos 2z cos 2x) / 2 + 0.15
            const WaveList a = plus(plus(times(times(S(2, 0, 0), C(0, 1, 0)), S(0, 0, 1)),
                                         times(times(S(0, 2, 0), C(0, 0, 1)), S(1, 0, 0))),
                                    times(times(S(0, 0, 2), C(1, 0, 0)), S(0, 1, 0)));
            const WaveList b = plus(plus(times(C(2, 0, 0), C(0, 2, 0)), times(C(0, 2, 0), C(0, 0, 2))),
                                    times(C(0, 0, 2), C(2, 0, 0)));
            return canonical(plus(plus(scaled(a, 0.5), scaled(b, -0.5)), cosWave(0, 0, 0, 0.15, 0.0)));
        }
    }
    return {};
}

} // namespace cellforms

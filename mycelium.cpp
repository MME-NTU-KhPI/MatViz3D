#include "mycelium.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>

namespace mycelium {

namespace {

using V3 = std::array<double,3>;

inline V3 operator+(V3 a, const V3& b) { for (int i = 0; i < 3; ++i) a[i] += b[i]; return a; }
inline V3 operator-(V3 a, const V3& b) { for (int i = 0; i < 3; ++i) a[i] -= b[i]; return a; }
inline V3 operator*(V3 a, double s)    { for (double& x : a) x *= s; return a; }
inline double dot(const V3& a, const V3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline double norm(const V3& a) { return std::sqrt(dot(a, a)); }
inline V3 unitv(const V3& a)
{
    const double n = norm(a);
    return n > 1e-12 ? a * (1.0 / n) : V3{ 0.0, 0.0, 1.0 };
}
inline double wrap1(double x)
{
    const double r = x - std::floor(x);
    return r >= 1.0 ? 0.0 : r;
}
inline V3 wrap3(const V3& p) { return { wrap1(p[0]), wrap1(p[1]), wrap1(p[2]) }; }
/// Minimum-image displacement.
inline V3 mic(V3 d) { for (double& x : d) x -= std::round(x); return d; }
inline double mic1(double d) { return d - std::round(d); }

const V3 kEz = { 0.0, 0.0, 1.0 };

/// Draws from the algorithm's generator. Normals by Box-Muller from the
/// uniform draws, not std::normal_distribution, whose algorithm differs
/// between standard libraries.
struct Rand {
    std::mt19937& g;
    std::uniform_real_distribution<double> U{ 0.0, 1.0 };
    double u() { return U(g); }
    double uniform(double a, double b) { return a + (b - a) * u(); }
    int    index(int n) { return std::min(n - 1, static_cast<int>(u() * n)); }
    double normal()
    {
        const double u1 = std::max(1e-300, u()), u2 = u();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
    }
    V3 normal3() { const double x = normal(), y = normal(), z = normal(); return { x, y, z }; }
    V3 dir()     { return unitv(normal3()); }
    V3 point()   { const double x = u(), y = u(), z = u(); return { x, y, z }; }
};

/// Direction at angle `ang` (radians) from u, random azimuth.
V3 turn(const V3& u, double ang, Rand& r)
{
    V3 a = r.dir();
    a = unitv(a - u * dot(a, u));
    return unitv(u * std::cos(ang) + a * std::sin(ang));
}

// ── substrate ───────────────────────────────────────────────────────────────

/// Nutrient on a periodic G^3 grid (node i at (i + 1/2) / G): hyphae take it up
/// around every new node, it diffuses, the tips read it and its gradient.
class Substrate {
public:
    Substrate(int G, const std::function<double(double)>& level) : m_G(G), m_S(static_cast<size_t>(G) * G * G)
    {
        for (int k = 0; k < G; ++k) {
            const double s = level((k + 0.5) / G);
            for (int i = 0; i < G; ++i)
                for (int j = 0; j < G; ++j) m_S[idx(i, j, k)] = s;
        }
        m_r = static_cast<int>(std::ceil(3.0 * kSigma * G));
    }

    double at(const V3& p) const
    {
        double g[3]; int i0[3]; double f[3];
        for (int a = 0; a < 3; ++a) {
            g[a] = wrap1(p[a]) * m_G - 0.5;
            i0[a] = static_cast<int>(std::floor(g[a]));
            f[a] = g[a] - i0[a];
        }
        double v = 0.0;
        for (int c = 0; c < 8; ++c) {
            const int cx = c & 1, cy = (c >> 1) & 1, cz = (c >> 2) & 1;
            const double w = (cx ? f[0] : 1 - f[0]) * (cy ? f[1] : 1 - f[1]) * (cz ? f[2] : 1 - f[2]);
            v += w * m_S[idx(i0[0] + cx, i0[1] + cy, i0[2] + cz)];
        }
        return v;
    }

    V3 grad(const V3& p) const
    {
        const double h = 1.0 / m_G;
        V3 g{};
        for (int a = 0; a < 3; ++a) {
            V3 e{}; e[a] = h;
            g[a] = (at(p + e) - at(p - e)) / (2.0 * h);
        }
        return g;
    }

    void take(const V3& p)
    {
        const V3 q = wrap3(p);
        int ic[3];
        for (int a = 0; a < 3; ++a) ic[a] = static_cast<int>(std::floor(q[a] * m_G));
        const double inv2s2 = 1.0 / (2.0 * kSigma * kSigma);
        for (int dx = -m_r; dx <= m_r; ++dx)
            for (int dy = -m_r; dy <= m_r; ++dy)
                for (int dz = -m_r; dz <= m_r; ++dz) {
                    const int c[3] = { ic[0] + dx, ic[1] + dy, ic[2] + dz };
                    double d2 = 0.0;
                    for (int a = 0; a < 3; ++a) { const double d = (c[a] + 0.5) / m_G - q[a]; d2 += d * d; }
                    m_S[idx(c[0], c[1], c[2])] *= 1.0 - kUptake * std::exp(-d2 * inv2s2);
                }
    }

    void diffuse()
    {
        std::vector<double> L(m_S.size());
        for (int s = 0; s < kSub; ++s) {
            for (int i = 0; i < m_G; ++i)
                for (int j = 0; j < m_G; ++j)
                    for (int k = 0; k < m_G; ++k)
                        L[idx(i, j, k)] = m_S[idx(i + 1, j, k)] + m_S[idx(i - 1, j, k)] + m_S[idx(i, j + 1, k)]
                                        + m_S[idx(i, j - 1, k)] + m_S[idx(i, j, k + 1)] + m_S[idx(i, j, k - 1)]
                                        - 6.0 * m_S[idx(i, j, k)];
            for (size_t n = 0; n < m_S.size(); ++n) m_S[n] += kD * L[n];
        }
    }

private:
    static constexpr double kUptake = 0.35;   ///< share taken at a node's own position
    static constexpr double kSigma  = 0.04;   ///< uptake radius, cell edges
    static constexpr double kD      = 0.12;   ///< diffusion per sub-step (stable below 1/6)
    static constexpr int    kSub    = 2;

    size_t idx(int i, int j, int k) const
    {
        auto w = [this](int x) { return ((x % m_G) + m_G) % m_G; };
        return (static_cast<size_t>(w(i)) * m_G + w(j)) * m_G + w(k);
    }

    int m_G;
    int m_r = 1;
    std::vector<double> m_S;
};

// ── the colony ──────────────────────────────────────────────────────────────

enum Kind { Hypha = 0, Skeletal = 1, Binding = 2, Layer = 3, Anchor = 4 };

struct Tip {
    V3     p{}, u{};
    int    h = 0, order = 0, kind = Hypha;
    double arc = 0.0, since = 0.0, due = -1.0;
    int    origin = -1;              ///< binding: the skeletal hypha it started on
    double sign = 0.0, z0 = 0.0;     ///< anchor: growth direction along Z, starting height
};

struct Options {
    double wiggle = 0.25, unit = 0.25, angle = 60.0, fuse = 0.05;
    double avoid = 0.3, avoidR = 0.1, chemo = 0.0, stopBelow = 0.08;
    double life = 0.0;               ///< 0 = hyphae grow until they stop or fuse
    int    maxTips = 32, maxOrder = 2;
};

class Colony {
public:
    explicit Colony(Rand& r) : rnd(r) {}

    std::vector<V3>     X;          ///< node positions in [0, 1)^3
    std::vector<int>    hyp;        ///< hypha of every node (-1: a spore)
    std::vector<double> arc;        ///< arc length of the node along its hypha
    std::vector<int>    pos;        ///< index of the node in its hypha's chain
    std::vector<std::vector<int>> chains;
    std::vector<int>    order, kind;   ///< per hypha
    std::vector<int>    origin;        ///< per hypha: the skeletal hypha a binding one grew from (-1 otherwise)
    std::vector<int>    spores;
    std::vector<Tip>    tips;
    int    fusions = 0, branches = 0, germ = 0;
    double length = 0.0;
    Rand&  rnd;

    // Per-mode hooks.
    std::function<void(int kind, Options&)>        tweak;        ///< options per kind
    std::function<V3(Tip&, const V3& p)>           bias;         ///< direction bias (may convert the tip)
    std::function<bool(const Tip&, int node)>      canFuse;
    std::function<int(const Tip&)>                 branchKind;
    std::function<void(Tip&)>                      onBranch;     ///< adjust a new branch tip
    std::function<double(int kind)>                life;         ///< length cap per hypha kind

    int nodeKind(int j) const { return hyp[j] >= 0 ? kind[hyp[j]] : -1; }

    int addNode(const V3& p, int h, double a)
    {
        X.push_back(wrap3(p));
        hyp.push_back(h);
        arc.push_back(a);
        pos.push_back(h >= 0 ? static_cast<int>(chains[h].size()) : 0);
        return static_cast<int>(X.size()) - 1;
    }

    int newHypha(int node, int ord, int knd)
    {
        chains.push_back({ node });
        order.push_back(ord);
        kind.push_back(knd);
        origin.push_back(-1);
        return static_cast<int>(chains.size()) - 1;
    }

    Tip newTip(int node, const V3& u, int ord, int knd, const V3& p)
    {
        Tip t;
        t.h = newHypha(node, ord, knd);
        t.p = p; t.u = u; t.order = ord; t.kind = knd;
        return t;
    }

    void spore(const V3& p, const V3& u, int knd, bool both = true)
    {
        const int n = addNode(p, -1, 0.0);
        spores.push_back(n);
        tips.push_back(newTip(n, u, 0, knd, X[n]));
        if (both) tips.push_back(newTip(n, u * -1.0, 0, knd, X[n]));
        germ += both ? 2 : 1;
    }

    /// All tips stopped: a lateral branch from a node that still has substrate.
    bool revive(const Substrate* sub, const Options& o)
    {
        if (X.size() < 3) return false;
        for (int t = 0; t < 60; ++t) {
            const int j = rnd.index(static_cast<int>(X.size()));
            const int h = hyp[j];
            if (h < 0 || (sub && sub->at(X[j]) < 2.0 * o.stopBelow)) continue;
            const std::vector<int>& ch = chains[h];
            const int k = pos[j];
            if (k <= 0 || k >= static_cast<int>(ch.size()) - 1 || ch[k] != j) continue;
            const V3 axis = unitv(mic(X[ch[k + 1]] - X[ch[k - 1]]));
            Tip nt = newTip(j, turn(axis, o.angle * M_PI / 180.0, rnd), std::min(order[h] + 1, o.maxOrder), kind[h], X[j]);
            if (kind[h] == Binding) { nt.origin = h; origin[nt.h] = h; }
            if (onBranch) onBranch(nt);
            tips.push_back(nt);
            ++branches;
            return true;
        }
        return false;
    }

    void grow(double total, Substrate* sub, const Options& base)
    {
        const double step = kStep;
        const double target = length + total;
        while (length < target) {
            if (tips.empty() && !revive(sub, base)) break;
            std::vector<Tip> alive;
            alive.reserve(2 * tips.size());
            for (Tip t : tips) {
                Options o = base;
                if (tweak) tweak(t.kind, o);
                const V3 p0 = t.p, q0 = wrap3(p0);
                V3 push{};
                if (o.avoid > 0.0) {
                    V3 m{}; int cnt = 0;
                    const double r2 = o.avoidR * o.avoidR;
                    for (size_t j = 0; j < X.size(); ++j) {
                        if (hyp[j] == t.h && arc[j] > t.arc - 1.5 * o.avoidR) continue;
                        const V3 d = mic(X[j] - q0);
                        if (dot(d, d) < r2) { m = m - d; ++cnt; }
                    }
                    if (cnt && norm(m) > 1e-9 * cnt) push = unitv(m) * o.avoid;
                }
                V3 g{};
                if (sub && o.chemo > 0.0) {
                    const V3 gr = sub->grad(p0);
                    if (norm(gr) > 1e-9) g = unitv(gr) * o.chemo;
                }
                const V3 b = bias ? bias(t, p0) : V3{};
                if (tweak) { o = base; tweak(t.kind, o); }   // the bias may have changed the tip's kind
                const V3 u = unitv(t.u + rnd.normal3() * o.wiggle + push + g + b);
                const V3 p = p0 + u * step;
                t.arc += step;
                t.since += step;
                length += step;

                // Fusion with another hypha.
                if (o.fuse > 0.0 && t.arc > 2.5 * o.fuse) {
                    const V3 q = wrap3(p);
                    int best = -1;
                    double bd = o.fuse * o.fuse;
                    for (size_t j = 0; j < X.size(); ++j) {
                        if (hyp[j] == t.h && arc[j] >= t.arc - 3.0 * o.fuse) continue;
                        const V3 d = mic(X[j] - q);
                        const double dd = dot(d, d);
                        if (dd < bd && (!canFuse || canFuse(t, static_cast<int>(j)))) { bd = dd; best = static_cast<int>(j); }
                    }
                    if (best >= 0) {
                        const int n = addNode(p, t.h, t.arc);
                        chains[t.h].push_back(n);
                        chains[t.h].push_back(best);
                        ++fusions;
                        continue;
                    }
                }

                const double s = sub ? sub->at(p + u * (3.0 * step)) : 1.0;   // what the tip grows into
                const int n = addNode(p, t.h, t.arc);
                chains[t.h].push_back(n);
                if (sub) sub->take(p);
                t.p = p;
                t.u = u;
                const double cap = life ? life(t.kind) : o.life;
                if (s < o.stopBelow || (cap > 0.0 && t.arc >= cap)) continue;   // a free end
                if (t.due < 0.0) t.due = o.unit * rnd.uniform(0.6, 1.4);
                if (t.since >= t.due / std::max(s, 0.05) && static_cast<int>(alive.size()) + 1 < o.maxTips) {
                    const int knd = branchKind ? branchKind(t) : t.kind;
                    const int ord = std::min(t.order + 1, o.maxOrder);
                    Tip nt = newTip(n, turn(u, o.angle * M_PI / 180.0, rnd), ord, knd, p);
                    nt.origin = t.origin;
                    origin[nt.h] = t.origin;
                    nt.sign = t.sign;
                    nt.z0 = t.z0;
                    if (onBranch) onBranch(nt);
                    ++branches;
                    t.since = 0.0;
                    t.due = -1.0;
                    alive.push_back(t);
                    alive.push_back(nt);
                } else {
                    alive.push_back(t);
                }
            }
            tips.swap(alive);
            if (sub) sub->diffuse();
        }
    }
};

// ── modes ───────────────────────────────────────────────────────────────────

void growSubstrate(Colony& col, const Params& p, bool layered)
{
    const double poor = std::clamp(p.poor, 0.0, 1.0);
    Substrate sub(32, [&](double z) {
        if (!layered) return 1.0;
        const double s = std::sin(M_PI * z);
        return poor + (1.0 - poor) * s * s;
    });
    const int n = p.spores > 0 ? p.spores : 4;
    for (int i = 0; i < n; ++i) {
        V3 q = col.rnd.point();
        if (layered) q[2] = wrap1(0.5 + 0.08 * col.rnd.normal());
        col.spore(q, col.rnd.dir(), Hypha);
    }
    Options o;
    o.chemo = 0.35; o.wiggle = p.wiggle; o.unit = p.unit; o.angle = p.angle; o.fuse = p.fuse;
    col.grow(p.length, &sub, o);
}

void growPolypore(Colony& col, const Params& p)
{
    const int nSkel = p.spores > 0 ? p.spores : 10;
    const double share = std::clamp(p.binding, 0.0, 0.95);
    const double skelLen = (1.0 - share) * p.length / nSkel;   // per skeletal hypha (both halves)
    const double align = std::clamp(p.align, 0.0, 1.0);
    for (int i = 0; i < nSkel; ++i) {
        const V3 q = col.rnd.point();   // drawn before the direction: argument order is unspecified
        V3 u = col.rnd.dir();
        if (align > 0.0) u = unitv(u * (1.0 - align) + kEz * align);
        col.spore(q, u, Skeletal);
    }
    {
        Options o;
        o.wiggle = 0.03; o.unit = 1e9; o.fuse = 0.0; o.avoid = 0.0; o.life = skelLen / 2.0;
        col.grow(nSkel * skelLen, nullptr, o);
    }
    col.tips.clear();
    const double bindLen = share * p.length;
    if (bindLen <= 0.0) return;

    std::vector<int> skel;
    for (size_t j = 0; j < col.X.size(); ++j)
        if (col.hyp[j] >= 0) skel.push_back(static_cast<int>(j));
    if (skel.empty()) return;
    // Aligned fibres need ties between them, so more binding hyphae start (two
    // more per fibre at full alignment) and spend their length on bridging.
    const int nBind = std::max(1, static_cast<int>(std::lround(14.0 * bindLen / 9.0 + 2.0 * align * nSkel)));
    for (int i = 0; i < nBind; ++i) {
        const int j = skel[static_cast<size_t>(col.rnd.index(static_cast<int>(skel.size())))];
        Tip t = col.newTip(j, col.rnd.dir(), 1, Binding, col.X[j]);
        t.origin = col.hyp[j];
        col.origin[t.h] = t.origin;
        col.tips.push_back(t);
        ++col.germ;
    }
    // A binding hypha reaches for the nearest skeletal hypha other than its own
    // and fuses with anything but the hypha it started on. Aligned skeletal
    // hyphae stand ~1/sqrt(n) apart side by side, beyond the 0.16 a random
    // tangle needs, so the reach grows with the alignment to tie them.
    const double attractR = 0.16 + align * std::max(0.0, 0.8 / std::sqrt(double(nSkel)) - 0.16);
    constexpr double attract = 0.6;
    col.bias = [&col, attractR](Tip& t, const V3& p0) -> V3 {
        if (t.kind != Binding) return {};
        const V3 q = wrap3(p0);
        double bd = attractR * attractR;
        V3 best{}; bool found = false;
        for (size_t j = 0; j < col.X.size(); ++j) {
            if (col.nodeKind(static_cast<int>(j)) != Skeletal || col.hyp[j] == t.origin) continue;
            const V3 d = mic(col.X[j] - q);
            const double dd = dot(d, d);
            if (dd < bd) { bd = dd; best = d; found = true; }
        }
        return found ? unitv(best) * attract : V3{};
    };
    // Aligned skeletal hyphae never cross, so a binding hypha has to reach
    // another one: it does not fuse into the tuft of its own fibre, and lives
    // long enough to cross the gap.
    col.canFuse = [&col, align](const Tip& t, int j) {
        const int h = col.hyp[j];
        if (h == t.origin) return false;
        return !(align > 0.0 && h >= 0 && col.kind[h] == Binding && col.origin[h] == t.origin);
    };
    const double life = 0.45 + 0.35 * align;
    col.life = [life](int) { return life; };
    // Binding hyphae are more tortuous, branch more often and wider than the
    // substrate hyphae of the same settings (defaults: 0.45, 0.09, 70 deg, 0.045).
    Options o;
    o.wiggle = 1.8 * p.wiggle * (1.0 - 0.5 * align); o.unit = 0.36 * p.unit * (1.0 + align);
    o.angle = std::min(90.0, p.angle * 7.0 / 6.0);
    o.fuse = 0.9 * p.fuse; o.avoid = 0.15; o.avoidR = 0.06; o.maxTips = 40;
    col.grow(bindLen, nullptr, o);
}

void growMat(Colony& col, const Params& p)
{
    static constexpr double layers[2] = { 0.25, 0.75 };
    constexpr double flat = 0.9, hold = 6.0;
    const int n = p.spores > 0 ? p.spores : 3;
    for (double z : layers)
        for (int i = 0; i < n; ++i) {
            V3 q = col.rnd.point(); q[2] = z;
            V3 u = col.rnd.dir(); u[2] = 0.0;
            col.spore(q, unitv(u), Layer);
        }
    auto nearestLayer = [](double z) {
        double best = 1e9;
        for (double L : layers) { const double d = mic1(z - L); if (std::abs(d) < std::abs(best)) best = d; }
        return best;
    };
    col.bias = [&col, nearestLayer](Tip& t, const V3& p0) -> V3 {
        const double z = wrap1(p0[2]);
        const double dz = nearestLayer(z);
        if (t.kind == Anchor && std::abs(dz) < 0.03 && std::abs(mic1(z - t.z0)) > 0.3) {
            // Reached the other layer: the anchor ends, a layer hypha spreads on from its tip.
            t.h = col.newHypha(col.chains[t.h].back(), 1, Layer);
            t.kind = Layer; t.order = 1; t.arc = 0.0;
        }
        if (t.kind == Layer) return kEz * (-flat * t.u[2] - hold * dz);
        return kEz * (0.8 * t.sign);
    };
    const double share = std::clamp(p.anchors, 0.0, 1.0);
    col.branchKind = [&col, share](const Tip& t) {
        if (t.kind == Layer && col.rnd.u() < share) return static_cast<int>(Anchor);
        return static_cast<int>(Layer);
    };
    col.onBranch = [&col](Tip& t) {
        if (t.kind != Anchor) return;
        t.sign = col.rnd.u() < 0.5 ? 1.0 : -1.0;
        t.u = unitv(t.u + kEz * (1.5 * t.sign));
        t.order = 0;
        col.order[t.h] = 0;
        t.z0 = wrap1(col.X[col.chains[t.h].front()][2]);
    };
    col.canFuse = [&col](const Tip& t, int j) {
        if (t.kind == Layer) return true;
        return col.nodeKind(j) == Layer && std::abs(mic1(wrap1(t.p[2]) - t.z0)) > 0.3;
    };
    col.tweak = [](int kind, Options& o) {
        if (kind == Anchor) { o.wiggle = 0.08; o.unit = 1e9; }
    };
    Options o;   // a little straighter and denser than substrate hyphae (defaults 0.22, 0.22)
    o.wiggle = 0.88 * p.wiggle; o.unit = 0.88 * p.unit; o.angle = p.angle; o.fuse = p.fuse;
    col.grow(p.length, nullptr, o);
}

/// Share of the length on the 2-core of the node graph (dangling trees removed).
double backboneShare(const Colony& col, const std::vector<std::array<int,2>>& edges, const std::vector<double>& len)
{
    std::vector<int> deg(col.X.size(), 0);
    std::vector<std::vector<int>> inc(col.X.size());
    for (size_t e = 0; e < edges.size(); ++e) {
        ++deg[edges[e][0]]; ++deg[edges[e][1]];
        inc[edges[e][0]].push_back(static_cast<int>(e));
        inc[edges[e][1]].push_back(static_cast<int>(e));
    }
    std::vector<char> gone(edges.size(), 0);
    std::vector<int> stack;
    for (size_t v = 0; v < deg.size(); ++v) if (deg[v] == 1) stack.push_back(static_cast<int>(v));
    while (!stack.empty()) {
        const int v = stack.back(); stack.pop_back();
        if (deg[v] != 1) continue;
        for (int e : inc[v]) {
            if (gone[e]) continue;
            gone[e] = 1;
            const int w = edges[e][0] == v ? edges[e][1] : edges[e][0];
            --deg[v]; --deg[w];
            if (deg[w] == 1) stack.push_back(w);
        }
    }
    double kept = 0.0, total = 0.0;
    for (size_t e = 0; e < edges.size(); ++e) { total += len[e]; if (!gone[e]) kept += len[e]; }
    return total > 0.0 ? kept / total : 0.0;
}

} // namespace

bool parseMode(const char* text, Mode& out)
{
    struct { const char* key; Mode m; } t[] = {
        { "substrate", Mode::Substrate }, { "polypore", Mode::Polypore }, { "mat", Mode::Mat }, { "layered", Mode::Layered } };
    for (const auto& e : t)
        if (std::strcmp(text, e.key) == 0) { out = e.m; return true; }
    return false;
}

const char* modeKey(Mode m)
{
    switch (m) {
        case Mode::Polypore: return "polypore";
        case Mode::Mat:      return "mat";
        case Mode::Layered:  return "layered";
        default:             return "substrate";
    }
}

std::vector<Segment> grow(const Params& p, std::mt19937& rng, Stats* stats)
{
    Rand r{ rng };
    Colony col(r);
    switch (p.mode) {
        case Mode::Polypore: growPolypore(col, p); break;
        case Mode::Mat:      growMat(col, p); break;
        case Mode::Layered:  growSubstrate(col, p, true); break;
        default:             growSubstrate(col, p, false); break;
    }

    const double taper = std::clamp(p.taper, 0.05, 1.0);
    std::vector<Segment> out;
    std::vector<std::array<int,2>> edges;
    std::vector<double> len;
    for (size_t h = 0; h < col.chains.size(); ++h) {
        const std::vector<int>& ch = col.chains[h];
        const float w = static_cast<float>(std::pow(taper, col.order[h]));
        for (size_t k = 0; k + 1 < ch.size(); ++k) {
            const V3& A = col.X[ch[k]];
            const V3 d = mic(col.X[ch[k + 1]] - A);
            if (dot(d, d) < 1e-18) continue;
            Segment s;
            for (int i = 0; i < 3; ++i) {
                s.a[i] = static_cast<float>(A[i]);
                s.b[i] = static_cast<float>(static_cast<double>(s.a[i]) + d[i]);
            }
            s.w = w;
            out.push_back(s);
            edges.push_back({ ch[k], ch[k + 1] });
            len.push_back(norm(d));
        }
    }

    if (stats) {
        *stats = measure(out);
        stats->grown    = true;
        stats->hyphae   = static_cast<int>(col.chains.size());
        stats->fusions  = col.fusions;
        stats->branches = col.branches;
        std::vector<int> deg(col.X.size(), 0);
        for (const auto& e : edges) { ++deg[e[0]]; ++deg[e[1]]; }
        int ends = 0;
        for (size_t v = 0; v < deg.size(); ++v)
            if (deg[v] == 1 && std::find(col.spores.begin(), col.spores.end(), static_cast<int>(v)) == col.spores.end()) ++ends;
        stats->freeEnds   = ends;
        stats->growthUnit = stats->length / std::max(1, col.germ + col.branches);
        stats->backbone   = backboneShare(col, edges, len);
    }
    return out;
}

Stats measure(const std::vector<Segment>& segments)
{
    Stats s;
    double A[3] = { 0.0, 0.0, 0.0 };
    for (const Segment& e : segments) {
        double d[3], L2 = 0.0;
        for (int i = 0; i < 3; ++i) { d[i] = double(e.b[i]) - double(e.a[i]); L2 += d[i] * d[i]; }
        const double L = std::sqrt(L2);
        s.length += L;
        if (L > 0.0)
            for (int i = 0; i < 3; ++i) A[i] += d[i] * d[i] / L;
    }
    for (int i = 0; i < 3; ++i) s.fabric[i] = s.length > 0.0 ? A[i] / s.length : 0.0;
    return s;
}

} // namespace mycelium

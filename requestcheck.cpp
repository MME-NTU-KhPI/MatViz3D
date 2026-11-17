#include "requestcheck.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>

namespace reqcheck {

namespace {

// 653, 65.3, 6.53, 0.65: as the tab prints a value.
std::string fmt(double x)
{
    char text[32];
    const double a = std::abs(x);
    std::snprintf(text, sizeof text, a >= 100 ? "%.0f" : a >= 10 ? "%.1f" : "%.2f", x);
    return text;
}

bool isModulus(const std::string& key)
{
    static const std::set<std::string> moduli = { "Ex", "Ey", "Ez", "Gxy", "Gyz", "Gxz", "Emean", "Gmean", "K" };
    return moduli.count(key) > 0;
}

std::string withUnit(const std::string& key, double value)
{
    return fmt(value) + (isModulus(key) ? " MPa" : "");
}

std::string join(const std::vector<std::string>& parts, const std::string& glue)
{
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) out += (i ? glue : "") + parts[i];
    return out;
}

// The material's moduli in MPa: E along the layers, across them, shear in the layer plane and across, Reuss bulk modulus.
struct Solid {
    double E = 0, Ez = 0, G = 0, Gz = 0, K = 0;
};
Solid solidOf(const Material& m)
{
    Solid s;
    s.E = m.eGpa * 1000.0;
    s.Ez = m.aniso * s.E;
    s.G = s.E / (2.0 * (1.0 + m.nu));
    s.Gz = m.aniso * s.G;
    // Parameters::lawStiffnessPa: S11 = S22 = 1/E, S33 = 1/(r E), S12 = S13 = S23 = -nu/E.
    const double sum = 2.0 / s.E + 1.0 / s.Ez - 6.0 * m.nu / s.E;
    s.K = sum > 0.0 ? 1.0 / sum : 0.0;
    return s;
}

// A modulus asked beyond a limit.
struct Hit {
    std::string key;
    double value = 0, limit = 0;
};

// "E x, E y, E z 653 MPa (limit 600 MPa), G xy 300 MPa (limit 217 MPa)": keys with the same value and limit share one entry.
std::string listHits(const std::vector<Hit>& hits)
{
    std::vector<std::string> parts;
    for (size_t i = 0; i < hits.size();) {
        size_t j = i;
        std::vector<std::string> names;
        while (j < hits.size() && std::abs(hits[j].value - hits[i].value) <= 1e-9 * hits[i].value
               && std::abs(hits[j].limit - hits[i].limit) <= 1e-9 * hits[i].limit)
            names.push_back(label(hits[j++].key));
        parts.push_back(join(names, ", ") + " " + withUnit(hits[i].key, hits[i].value) + " (limit " + withUnit(hits[i].key, hits[i].limit) + ")");
        i = j;
    }
    return join(parts, ", ");
}

// The density at which the Hashin-Shtrikman bound of `which` (0 E, 1 G, 2 K) reaches `value`; above 1 when even the solid does not.
double hsDensity(int which, double value, const Material& m)
{
    auto bound = [&](double rho) {
        double E = 0, G = 0, K = 0;
        hashinShtrikman(m.eGpa * 1000.0, m.nu, rho, &E, &G, &K);
        return which == 0 ? E : which == 1 ? G : K;
    };
    if (bound(1.0) < value) return 2.0;
    double lo = 0.0, hi = 1.0;
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (lo + hi);
        (bound(mid) < value ? lo : hi) = mid;
    }
    return hi;
}

// ── The moduli together: the Hashin-Shtrikman trace bounds of a porous cell ──

// What the joint bound reads: the moduli and Poisson's ratios asked along the axes, NaN where not asked.
struct AxisRequest {
    double E[3];    // E x, E y, E z
    double G[3];    // G xy, G yz, G xz
    double nu[3];   // nu xy, nu xz, nu yz
};

const double kNotAsked = std::numeric_limits<double>::quiet_NaN();

// Nelder-Mead from x, which ends at the best point found; f is +inf outside its domain.
template <class F>
double nelderMead(F&& f, std::vector<double>& x, double step, int maxEval)
{
    const int n = int(x.size());
    std::vector<std::vector<double>> p(n + 1, x);
    std::vector<double> v(n + 1);
    for (int i = 0; i < n; ++i) p[i + 1][i] += step * std::max(std::abs(x[i]), 1.0);
    for (int i = 0; i <= n; ++i) v[i] = f(p[i]);
    int evals = n + 1;
    std::vector<double> c(n), xr(n), xe(n), xc(n);
    std::vector<int> order(n + 1);
    while (evals < maxEval) {
        for (int i = 0; i <= n; ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](int a, int b) { return v[a] < v[b]; });
        std::vector<std::vector<double>> ps(n + 1);
        std::vector<double> vs(n + 1);
        for (int i = 0; i <= n; ++i) { ps[i] = p[order[i]]; vs[i] = v[order[i]]; }
        p.swap(ps);
        v.swap(vs);
        if (std::isfinite(v[n]) && v[n] - v[0] <= 1e-13 * (1.0 + std::abs(v[0]))) break;
        std::fill(c.begin(), c.end(), 0.0);
        for (int i = 0; i < n; ++i)
            for (int k = 0; k < n; ++k) c[k] += p[i][k] / n;
        for (int k = 0; k < n; ++k) xr[k] = c[k] + (c[k] - p[n][k]);
        const double fr = f(xr);
        ++evals;
        if (fr < v[0]) {
            for (int k = 0; k < n; ++k) xe[k] = c[k] + 2.0 * (c[k] - p[n][k]);
            const double fe = f(xe);
            ++evals;
            if (fe < fr) { p[n] = xe; v[n] = fe; } else { p[n] = xr; v[n] = fr; }
        } else if (fr < v[n - 1]) {
            p[n] = xr;
            v[n] = fr;
        } else {
            const bool outside = fr < v[n];
            for (int k = 0; k < n; ++k) xc[k] = c[k] + 0.5 * ((outside ? xr[k] : p[n][k]) - c[k]);
            const double fc = f(xc);
            ++evals;
            if (fc < std::min(fr, v[n])) {
                p[n] = xc;
                v[n] = fc;
            } else {
                for (int i = 1; i <= n; ++i) {
                    for (int k = 0; k < n; ++k) p[i][k] = p[0][k] + 0.5 * (p[i][k] - p[0][k]);
                    v[i] = f(p[i]);
                }
                evals += n;
            }
        }
    }
    const int best = int(std::min_element(v.begin(), v.end()) - v.begin());
    x = p[best];
    return v[best];
}

bool inverse3(const double a[3][3], double out[3][3])
{
    const double c00 = a[1][1] * a[2][2] - a[1][2] * a[2][1], c01 = a[1][2] * a[2][0] - a[1][0] * a[2][2], c02 = a[1][0] * a[2][1] - a[1][1] * a[2][0];
    const double det = a[0][0] * c00 + a[0][1] * c01 + a[0][2] * c02;
    if (!(std::abs(det) > 0.0) || !std::isfinite(det)) return false;
    const double inv = 1.0 / det;
    out[0][0] = c00 * inv;
    out[1][0] = c01 * inv;
    out[2][0] = c02 * inv;
    out[0][1] = (a[0][2] * a[2][1] - a[0][1] * a[2][2]) * inv;
    out[1][1] = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) * inv;
    out[2][1] = (a[0][1] * a[2][0] - a[0][0] * a[2][1]) * inv;
    out[0][2] = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) * inv;
    out[1][2] = (a[0][2] * a[1][0] - a[0][0] * a[1][2]) * inv;
    out[2][2] = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) * inv;
    return true;
}

// Positive definite (Sylvester's criterion) for a symmetric 3 x 3.
bool positive3(const double a[3][3])
{
    const double m2 = a[0][0] * a[1][1] - a[0][1] * a[1][0];
    const double det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
                       + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    return a[0][0] > 0.0 && m2 > 0.0 && det > 0.0;
}

/**
 * The trace bounds (Milton & Kohn 1988) of a cell of an isotropic solid (K_s, G_s) and void at solid fraction rho:
 * with A = C_s - C in Mandel form (Λh, Λs the hydrostatic and deviatoric projections),
 *     Tr(Λh A⁻¹) <= 1 / (3 (K_s - k))     and     Tr(Λs A⁻¹) <= 5 / (2 (G_s - g)),
 * k and g the Hashin-Shtrikman bounds. For an isotropic cell they are those bounds; for any other shape they hold as
 * they stand (checked on the 2 037 solved cells of dataset v1 with an isotropic solid: none above, the closest at
 * 0.998 of the bulk bound - closed foams - and 0.990 of the shear bound - plates). A solid weaker across the printed
 * layers (r <= 1) has a compliance at least that of the isotropic solid with E along them, so the cell is softer in
 * every load and the bounds of that solid hold for it.
 *
 * The request fixes some compliances: S_ii = 1 / E_i, S_ij = -nu_ij S_ii, the shear ones 1 / G. Its axis values do not
 * change under the mirror images of the cell and both traces are convex in the compliance, so when some cell has the
 * asked values an orthotropic one has (the mean over the mirror images): the free entries are the normal compliances
 * the request leaves open, and a shear modulus not asked adds least at zero, 1 / (2 G_s). Some cell is allowed when
 * min over those entries of max(bulk trace / bound, shear trace / bound) <= 1; since the two are convex, that is
 * max over t in [0, 1] of min over the entries of t bulk + (1 - t) shear (a smooth problem for each t).
 */
class TraceBound {
public:
    TraceBound(const Material& m, double rho)
    {
        es = m.eGpa * 1000.0;
        ks = es / (3.0 * (1.0 - 2.0 * m.nu));
        gs = es / (2.0 * (1.0 + m.nu));
        lambda = ks - 2.0 * gs / 3.0;
        double k = 0, g = 0;
        hashinShtrikman(es, m.nu, rho, nullptr, &g, &k);
        bulkScale = ks - k;
        shearScale = 2.0 * (gs - g) / 5.0;
        startE = std::max(0.3 * rho, 0.01) * es;
    }

    // The two traces over their bounds for the free entries x (normal compliances x E_s); false outside the domain.
    bool ratios(const AxisRequest& r, const double* x, double* bulk, double* shear) const
    {
        double s[3][3];
        int k = 0;
        for (int i = 0; i < 3; ++i) s[i][i] = std::isfinite(r.E[i]) ? 1.0 / r.E[i] : x[k++] / es;
        const int pa[3] = { 0, 0, 1 }, pb[3] = { 1, 2, 2 };   // nu xy, xz, yz: S12 = -nu xy S11, S13 = -nu xz S11, S23 = -nu yz S22
        for (int p = 0; p < 3; ++p) {
            const double v = std::isfinite(r.nu[p]) ? -r.nu[p] * s[pa[p]][pa[p]] : x[k++] / es;
            s[pa[p]][pb[p]] = s[pb[p]][pa[p]] = v;
        }
        double c[3][3], a[3][3], b[3][3];
        if (!positive3(s) || !inverse3(s, c)) return false;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) a[i][j] = lambda + (i == j ? 2.0 * gs : 0.0) - c[i][j];
        if (!positive3(a) || !inverse3(a, b)) return false;
        double sum = 0.0, trace = 0.0;
        for (int i = 0; i < 3; ++i) {
            trace += b[i][i];
            for (int j = 0; j < 3; ++j) sum += b[i][j];
        }
        double shearPart = 0.0;
        for (int i = 0; i < 3; ++i) {
            const double g = std::isfinite(r.G[i]) ? r.G[i] : 0.0;
            if (!(g < gs)) return false;
            shearPart += 1.0 / (2.0 * (gs - g));
        }
        *bulk = sum * bulkScale;                                   // Tr(Λh A⁻¹) = (1/3) m·B·m, times 3 (K_s - k)
        *shear = (trace - sum / 3.0 + shearPart) * shearScale;
        return true;
    }

    // Can some cell of this material at this density have the asked values? Undecided cases count as yes.
    bool fits(const AxisRequest& r) const
    {
        // The optimizer can only overstate the minimum, so a conflict must be clear of its error (~1e-8 after the
        // restarts). Near the limit the score is flat - G 69 -> 72.7 MPa moves it by 0.002 in the case of the tests -
        // so a looser margin would move the reported limits.
        const double tol = 1e-4;
        int n = 0;
        for (int i = 0; i < 3; ++i) n += !std::isfinite(r.E[i]) + !std::isfinite(r.nu[i]);
        double bulk = 0, shear = 0;
        if (n == 0) return ratios(r, nullptr, &bulk, &shear) && std::max(bulk, shear) <= 1.0 + tol;

        // A start inside the domain: open E at a third of the Voigt level, open Poisson's ratios tried at a few values.
        std::vector<double> x;
        bool inside = false;
        for (double nu0 : { 0.2, 0.0, 0.35, -0.2, 0.45 }) {
            double d[3];
            for (int i = 0; i < 3; ++i) d[i] = std::isfinite(r.E[i]) ? es / r.E[i] : es / startE;
            x.clear();
            for (int i = 0; i < 3; ++i)
                if (!std::isfinite(r.E[i])) x.push_back(d[i]);
            const int pa[3] = { 0, 0, 1 }, pb[3] = { 1, 2, 2 };
            for (int p = 0; p < 3; ++p)
                if (!std::isfinite(r.nu[p])) x.push_back(-nu0 * std::sqrt(d[pa[p]] * d[pb[p]]));
            if (ratios(r, x.data(), &bulk, &shear)) { inside = true; break; }
        }
        if (!inside) return true;
        if (std::max(bulk, shear) <= 1.0) return true;

        bool found = false;
        auto phi = [&](double t) {
            auto f = [&](const std::vector<double>& y) {
                double b = 0, s = 0;
                if (!ratios(r, y.data(), &b, &s)) return std::numeric_limits<double>::infinity();
                if (std::max(b, s) <= 1.0) found = true;
                return t * b + (1.0 - t) * s;
            };
            double v = 0.0;
            for (double step : { 0.3, 0.05, 0.01 }) v = nelderMead(f, x, step, 600 * n);   // two restarts from the best point
            return v;
        };
        auto verdict = [&](double v, bool* decided) {
            *decided = found || v > 1.0 + tol;
            return found;
        };
        bool decided = false;
        bool answer = verdict(phi(0.0), &decided);
        if (decided) return answer;
        answer = verdict(phi(1.0), &decided);
        if (decided) return answer;
        const double golden = 0.5 * (std::sqrt(5.0) - 1.0);
        double lo = 0.0, hi = 1.0;
        double t1 = hi - golden * (hi - lo), t2 = lo + golden * (hi - lo);
        double f1 = phi(t1);
        answer = verdict(f1, &decided);
        if (decided) return answer;
        double f2 = phi(t2);
        answer = verdict(f2, &decided);
        if (decided) return answer;
        for (int it = 0; it < 20; ++it) {   // phi is concave in t: its largest value is the min-max
            if (f1 > f2) { hi = t2; t2 = t1; f2 = f1; t1 = hi - golden * (hi - lo); f1 = phi(t1); answer = verdict(f1, &decided); }
            else         { lo = t1; t1 = t2; f1 = f2; t2 = lo + golden * (hi - lo); f2 = phi(t2); answer = verdict(f2, &decided); }
            if (decided) return answer;
        }
        return true;
    }

private:
    double es = 0, ks = 0, gs = 0, lambda = 0, bulkScale = 0, shearScale = 0, startE = 0;
};

// The smallest density at which `r` fits, from `rho` (where it does not) up; above 1 when none does.
double traceDensity(const AxisRequest& r, const Material& m, double rho)
{
    if (!TraceBound(m, 0.999).fits(r)) return 2.0;
    double lo = rho, hi = 0.999;
    for (int i = 0; i < 14; ++i) {
        const double mid = 0.5 * (lo + hi);
        (TraceBound(m, mid).fits(r) ? hi : lo) = mid;
    }
    return hi;
}

// The largest factor on the asked E (which = 0) or G (1) with which `r` fits; `r` does not fit at 1. 0: none.
double traceScale(const AxisRequest& r, const TraceBound& bound, int which)
{
    auto scaled = [&](double f) {
        AxisRequest q = r;
        for (int i = 0; i < 3; ++i) (which == 0 ? q.E[i] : q.G[i]) *= f;
        return q;
    };
    const double floor = which == 0 ? 1e-3 : 0.0;
    if (!bound.fits(scaled(floor))) return 0.0;
    double lo = floor, hi = 1.0;
    for (int i = 0; i < 14; ++i) {
        const double mid = 0.5 * (lo + hi);
        (bound.fits(scaled(mid)) ? lo : hi) = mid;
    }
    return lo;
}

// "E x, E y, E z 610 MPa", "E x 500 MPa, E y 400 MPa, ν xy 0.30": keys with the same value share one entry.
std::string listValues(const std::vector<std::string>& keys, const std::map<std::string, double>& targets)
{
    std::vector<std::string> parts;
    for (size_t i = 0; i < keys.size();) {
        size_t j = i;
        std::vector<std::string> names;
        const double v = targets.at(keys[i]);
        while (j < keys.size() && isModulus(keys[j]) == isModulus(keys[i]) && std::abs(targets.at(keys[j]) - v) <= 1e-9 * std::abs(v))
            names.push_back(label(keys[j++]));
        parts.push_back(join(names, ", ") + " " + withUnit(keys[i], v));
        i = j;
    }
    return join(parts, ", ");
}

} // namespace

std::string label(const std::string& key)
{
    static const std::map<std::string, std::string> names = {
        { "Ex", "E x" }, { "Ey", "E y" }, { "Ez", "E z" }, { "Gxy", "G xy" }, { "Gyz", "G yz" }, { "Gxz", "G xz" },
        { "nuXy", "ν xy" }, { "nuXz", "ν xz" }, { "nuYz", "ν yz" }, { "Emean", "E mean" }, { "Gmean", "G mean" },
        { "EzEx", "E z / E x" }, { "EyEx", "E y / E x" }, { "EzEy", "E z / E y" }, { "GxzGxy", "G xz / G xy" },
        { "GyzGxy", "G yz / G xy" }, { "GyzGxz", "G yz / G xz" }, { "EmaxEmin", "E max / min" }, { "GmaxGmin", "G max / min" },
        { "AU", "Aᵁ" }, { "zener", "Zener" }, { "K", "K" }, { "nuMin", "ν min" }, { "density", "density" } };
    const auto it = names.find(key);
    return it == names.end() ? key : it->second;
}

double voigtPerDensity(const std::string& key, const Material& material)
{
    const Solid s = solidOf(material);
    if (key == "Ex" || key == "Ey") return s.E;
    if (key == "Ez") return s.Ez;
    if (key == "Gxy") return s.G;
    if (key == "Gyz" || key == "Gxz") return s.Gz;
    if (key == "Emean") return std::cbrt(s.E * s.E * s.Ez);       // geometric mean of the three axes
    if (key == "Gmean") return std::cbrt(s.G * s.Gz * s.Gz);
    if (key == "K") return s.K;
    return 0.0;
}

void hashinShtrikman(double eMPa, double nu, double rho, double* E, double* G, double* K)
{
    // Upper bounds of a porous isotropic cell (the void has no stiffness), solid fraction rho.
    const double Ks = eMPa / (3.0 * (1.0 - 2.0 * nu)), Gs = eMPa / (2.0 * (1.0 + nu));
    const double k = 4.0 * Gs * Ks * rho / (4.0 * Gs + 3.0 * Ks * (1.0 - rho));
    const double g = Gs * rho * (9.0 * Ks + 8.0 * Gs) / (9.0 * Ks + 8.0 * Gs + 6.0 * (1.0 - rho) * (Ks + 2.0 * Gs));
    if (K) *K = k;
    if (G) *G = g;
    if (E) *E = (3.0 * k + g) > 0.0 ? 9.0 * k * g / (3.0 * k + g) : 0.0;
}

std::vector<Issue> check(const std::map<std::string, double>& targets, const Material& material, const Form& form)
{
    std::vector<Issue> out;
    auto has = [&](const std::string& key) { return targets.count(key) > 0 && std::isfinite(targets.at(key)); };
    auto at = [&](const std::string& key) { return targets.at(key); };
    auto add = [&](std::vector<std::string> keys, const std::string& text) { out.push_back({ std::move(keys), text }); };
    // Two numbers that should be equal differ by more than `tol` (relative).
    auto differ = [](double a, double b, double tol) { return std::abs(std::log(a / b)) > std::log(1.0 + tol); };

    const bool rhoGiven = has("density");
    const double rho = rhoGiven ? at("density") : 0.0;
    const std::vector<std::string> moduli = { "Ex", "Ey", "Ez", "Gxy", "Gyz", "Gxz", "Emean", "Gmean", "K" };
    const std::vector<std::string> eAxes = { "Ex", "Ey", "Ez" }, gAxes = { "Gxy", "Gyz", "Gxz" }, nuAxes = { "nuXy", "nuXz", "nuYz" };

    // ── Isotropy: the whole stiffness the same in every direction ──
    // A^U = 0 only for an isotropic stiffness; E max / min = 1 alone allows anisotropic shear, so with G max / min = 1 too.
    const bool isoAU = has("AU") && at("AU") <= 0.05;
    const bool isoDir = has("EmaxEmin") && at("EmaxEmin") <= 1.05 && has("GmaxGmin") && at("GmaxGmin") <= 1.05;
    const bool iso = isoAU || isoDir;
    const std::string isoWhat = isoAU ? label("AU") + " " + fmt(at("AU"))
                                      : isoDir ? label("EmaxEmin") + " " + fmt(at("EmaxEmin")) + ", " + label("GmaxGmin") + " " + fmt(at("GmaxGmin")) : "";
    const std::vector<std::string> isoKeys = isoAU ? std::vector<std::string>{ "AU" } : std::vector<std::string>{ "EmaxEmin", "GmaxGmin" };

    // ── "Same in all directions" against anisotropy targets ──
    // The form sends one E (G) for the three axes; a ratio of the axes other than 1, or a ratio over all directions
    // above 1, asks for a cell whose modulus changes with direction. Only for E (G) when it was given.
    std::set<std::string> sameReported;   // the ratio targets named here; the per-axis messages below would repeat them
    if (form.sameInAllDirections) {
        const struct { const char* modulus; std::vector<std::string> axes, ratios; const char* overAll; } sides[] = {
            { "E", eAxes, { "EzEx", "EyEx", "EzEy" }, "EmaxEmin" }, { "G", gAxes, { "GxzGxy", "GyzGxy", "GyzGxz" }, "GmaxGmin" } };
        for (const auto& s : sides) {
            if (!std::all_of(s.axes.begin(), s.axes.end(), has)) continue;
            std::vector<std::string> off, keys;
            for (const std::string& k : s.ratios)
                if (has(k) && at(k) > 0.0 && differ(at(k), 1.0, 0.03)) { off.push_back(label(k) + " " + fmt(at(k))); keys.push_back(k); }
            if (has(s.overAll) && at(s.overAll) > 1.05) { off.push_back(label(s.overAll) + " " + fmt(at(s.overAll))); keys.push_back(s.overAll); }
            if (off.empty()) continue;
            sameReported.insert(keys.begin(), keys.end());
            keys.insert(keys.end(), s.axes.begin(), s.axes.end());
            add(keys, std::string("\"Same in all directions\" gives ") + s.modulus + " one value for every direction (" + label(s.axes[0]) + " = "
                          + label(s.axes[1]) + " = " + label(s.axes[2]) + " = " + withUnit(s.axes[0], at(s.axes[0])) + "), but " + join(off, ", ")
                          + " asks for " + s.modulus + " that changes with direction. Untick \"same in all directions\" to set the axes one by one, "
                          + "or set " + (off.size() > 1 ? "these targets" : "this target") + " to 1.");
        }
    }

    // ── Bounds of any shape (Voigt) and of an isotropic cell (Hashin-Shtrikman) ──
    std::set<std::string> boundedByHs;
    if (iso && rhoGiven && material.aniso <= 1.0 + 1e-9) {
        double bE = 0, bG = 0, bK = 0;
        hashinShtrikman(material.eGpa * 1000.0, material.nu, rho, &bE, &bG, &bK);
        std::vector<Hit> hits;
        double need = 0.0;
        for (const std::string& key : moduli) {
            if (!has(key)) continue;
            const int which = (key[0] == 'E') ? 0 : (key[0] == 'G') ? 1 : 2;
            const double limit = which == 0 ? bE : which == 1 ? bG : bK;
            if (at(key) > limit * 1.05) {   // asked "isotropic" up to 5 %: a little above the bound of an exactly isotropic cell
                hits.push_back({ key, at(key), limit });
                need = std::max(need, hsDensity(which, at(key), material));
            }
        }
        if (!hits.empty()) {
            std::vector<std::string> keys;
            for (const Hit& h : hits) { keys.push_back(h.key); boundedByHs.insert(h.key); }
            keys.insert(keys.end(), isoKeys.begin(), isoKeys.end());
            keys.push_back("density");
            add(keys, "Beyond any cell that is the same in every direction (" + isoWhat + ") at density " + fmt(rho) + ": "
                          + listHits(hits) + ". The limit is the Hashin–Shtrikman bound of an isotropic porous solid; "
                          + (need <= 1.0 ? "an isotropic cell needs density ≥ " + fmt(need) + " for these moduli."
                                         : "no density reaches them in an isotropic cell."));
        }
    }
    bool singleBound = !boundedByHs.empty();   // some modulus is beyond its own limit: the joint bound below has nothing to add
    {
        std::vector<Hit> hits;
        double need = 0.0;
        for (const std::string& key : moduli) {
            const double c = voigtPerDensity(key, material);
            if (!has(key) || !(c > 0.0) || !(at(key) > 0.0)) continue;
            need = std::max(need, at(key) / c);
            const double limit = (rhoGiven ? rho : 1.0) * c;
            if (at(key) > limit * (1.0 + 1e-9) && !boundedByHs.count(key)) hits.push_back({ key, at(key), limit });
        }
        if (!hits.empty()) {
            singleBound = true;
            std::vector<std::string> keys;
            for (const Hit& h : hits) keys.push_back(h.key);
            if (rhoGiven) {
                keys.push_back("density");
                add(keys, "Beyond any shape at density " + fmt(rho) + ": " + listHits(hits) + ". No cell of this material is stiffer "
                              "than density × the solid's modulus along the load (all of its material lined up with the load, the Voigt bound); "
                              + (need <= 1.0 ? "these moduli need density ≥ " + fmt(need) + "." : "these moduli are beyond the solid itself."));
            } else {
                add(keys, "Stiffer than the solid itself: " + listHits(hits) + ". No density reaches it.");
            }
        }
    }

    // ── Poisson's ratios with the moduli: the compliance of the asked numbers must be positive ──
    const bool allE = has("Ex") && has("Ey") && has("Ez");
    const bool allNu = has("nuXy") && has("nuXz") && has("nuYz");
    bool stable = true;
    {
        // nu_ij: stretched along i, the share it narrows along j; |nu_ij| < sqrt(E_i / E_j) for every pair.
        const struct { const char* nu; const char* ei; const char* ej; } pairs[] = { { "nuXy", "Ex", "Ey" }, { "nuXz", "Ex", "Ez" }, { "nuYz", "Ey", "Ez" } };
        std::vector<std::string> bad;
        for (const auto& p : pairs) {
            if (!has(p.nu) || !has(p.ei) || !has(p.ej)) continue;
            const double limit = std::sqrt(at(p.ei) / at(p.ej));
            if (std::abs(at(p.nu)) >= limit)
                bad.push_back(label(p.nu) + " " + fmt(at(p.nu)) + " (|ν| must stay below √(" + label(p.ei) + " / " + label(p.ej) + ") = " + fmt(limit) + ")");
        }
        if (!bad.empty()) {
            stable = false;
            add({ "nuXy", "nuXz", "nuYz", "Ex", "Ey", "Ez" }, "No elastic material has these moduli and Poisson's ratios: " + join(bad, ", ")
                                                               + "; otherwise some load would give negative energy.");
        } else if (allE && allNu) {
            const double s11 = 1.0 / at("Ex"), s22 = 1.0 / at("Ey"), s33 = 1.0 / at("Ez");
            const double s12 = -at("nuXy") / at("Ex"), s13 = -at("nuXz") / at("Ex"), s23 = -at("nuYz") / at("Ey");
            const double det = s11 * (s22 * s33 - s23 * s23) - s12 * (s12 * s33 - s23 * s13) + s13 * (s12 * s23 - s22 * s13);
            if (!(det > 0.0)) {
                stable = false;
                const bool equal = !differ(at("Ex"), at("Ey"), 0.001) && !differ(at("Ex"), at("Ez"), 0.001)
                                   && std::abs(at("nuXy") - at("nuXz")) < 1e-6 && std::abs(at("nuXy") - at("nuYz")) < 1e-6;
                add({ "nuXy", "nuXz", "nuYz", "Ex", "Ey", "Ez" },
                    "No elastic material has E x / y / z " + fmt(at("Ex")) + " / " + fmt(at("Ey")) + " / " + fmt(at("Ez")) + " MPa with ν xy / xz / yz "
                        + fmt(at("nuXy")) + " / " + fmt(at("nuXz")) + " / " + fmt(at("nuYz")) + ": some load would give negative energy"
                        + (equal ? " (with equal E and equal Poisson's ratios they must stay below 0.5)." : "."));
            } else if (has("K")) {
                // The normal compliances fix the Reuss bulk modulus.
                const double k = 1.0 / (s11 + s22 + s33 + 2.0 * (s12 + s13 + s23));
                if (differ(at("K"), k, 0.05))
                    add({ "K", "Ex", "Ey", "Ez", "nuXy", "nuXz", "nuYz" },
                        "The asked E and ν fix the bulk modulus: K = 1 / (sum of the normal compliances) = " + fmt(k) + " MPa; asked K "
                            + fmt(at("K")) + " MPa.");
            }
        }
    }

    // ── The moduli together: each under its own limit, yet no cell has them all (Hashin-Shtrikman trace bounds) ──
    // Reads the values along the axes (E, G, nu); E mean / G mean and K are left to the checks above. A cell asked to be
    // isotropic has its own bound above, and a solid stiffer across the layers (r > 1) is not covered by these bounds.
    if (rhoGiven && rho > 0.0 && rho < 1.0 && !singleBound && stable && !iso && material.aniso <= 1.0 + 1e-9) {
        AxisRequest r;
        std::vector<std::string> eKeys, gKeys, nuKeys;
        for (int i = 0; i < 3; ++i) {
            r.E[i] = has(eAxes[i]) && at(eAxes[i]) > 0.0 ? at(eAxes[i]) : kNotAsked;
            r.G[i] = has(gAxes[i]) && at(gAxes[i]) > 0.0 ? at(gAxes[i]) : kNotAsked;
            r.nu[i] = has(nuAxes[i]) ? at(nuAxes[i]) : kNotAsked;
            if (std::isfinite(r.E[i])) eKeys.push_back(eAxes[i]);
            if (std::isfinite(r.G[i])) gKeys.push_back(gAxes[i]);
            if (std::isfinite(r.nu[i])) nuKeys.push_back(nuAxes[i]);
        }
        const TraceBound bound(material, rho);
        if ((!eKeys.empty() || !gKeys.empty()) && !bound.fits(r)) {
            AxisRequest onlyE = r, onlyG = r;
            for (int i = 0; i < 3; ++i) {
                onlyE.G[i] = kNotAsked;
                onlyG.E[i] = onlyG.nu[i] = kNotAsked;
            }
            std::vector<std::string> eSide = eKeys;
            eSide.insert(eSide.end(), nuKeys.begin(), nuKeys.end());
            std::vector<std::string> gNames, eNames;
            for (const std::string& k : gKeys) gNames.push_back(label(k));
            for (const std::string& k : eKeys) eNames.push_back(label(k));
            // "G xy, G yz, G xz can reach at most 69.3 MPa" or, for unequal asked values, "... at most 40 % of the asked values".
            auto reach = [&](const std::vector<std::string>& keys, const std::vector<std::string>& names, double factor) {
                const double v = at(keys[0]);
                const bool equal = std::all_of(keys.begin(), keys.end(), [&](const std::string& k) { return std::abs(at(k) - v) <= 1e-9 * v; });
                return join(names, ", ") + " can reach at most "
                       + (equal ? fmt(factor * v) + " MPa" : fmt(100.0 * factor) + " % of the asked values");
            };
            std::vector<std::string> parts;
            if (!eKeys.empty() && !gKeys.empty()) {
                const bool eFits = bound.fits(onlyE), gFits = bound.fits(onlyG);
                if (eFits) {
                    const double f = traceScale(r, bound, 1);
                    parts.push_back("With " + listValues(eSide, targets) + ", " + reach(gKeys, gNames, f) + ".");
                } else {
                    parts.push_back(listValues(eSide, targets) + " alone need density ≥ " + fmt(traceDensity(onlyE, material, rho)) + ".");
                }
                if (gFits) {
                    const double f = traceScale(r, bound, 0);
                    parts.push_back("With " + listValues(gKeys, targets) + ", " + (f > 0.0 ? reach(eKeys, eNames, f) : "no E along the axes is possible") + ".");
                } else {
                    parts.push_back(listValues(gKeys, targets) + " alone need density ≥ " + fmt(traceDensity(onlyG, material, rho)) + ".");
                }
            }
            const double need = traceDensity(r, material, rho);
            std::vector<std::string> all = eSide;
            all.insert(all.end(), gKeys.begin(), gKeys.end());
            std::vector<std::string> keys = all;
            keys.push_back("density");
            const std::string why = (!eKeys.empty() && !gKeys.empty())
                                        ? "material lined up with the axes gives E, material across them at 45° gives G, and both draw on the same solid"
                                        : "they draw on the same solid";
            add(keys, "At density " + fmt(rho) + " " + listValues(all, targets) + " do not fit in one cell, though each is under its own limit: " + why
                          + " (the Hashin–Shtrikman trace bounds of a porous cell). " + (parts.empty() ? "" : join(parts, " ") + " ")
                          + (need <= 1.0 ? "Together they need density ≥ " + fmt(need) + "." : "No density reaches them together."));
        }
    }

    // ── Ratio targets against the values along the axes ──
    {
        const struct { const char* ratio; const char* top; const char* bottom; } pairs[] = {
            { "EzEx", "Ez", "Ex" }, { "EyEx", "Ey", "Ex" }, { "EzEy", "Ez", "Ey" },
            { "GxzGxy", "Gxz", "Gxy" }, { "GyzGxy", "Gyz", "Gxy" }, { "GyzGxz", "Gyz", "Gxz" } };
        for (const auto& p : pairs) {
            if (!has(p.ratio) || !has(p.top) || !has(p.bottom) || sameReported.count(p.ratio)) continue;
            const double r = at(p.top) / at(p.bottom);
            if (differ(at(p.ratio), r, 0.03))
                add({ p.ratio, p.top, p.bottom }, label(p.ratio) + " " + fmt(at(p.ratio)) + " disagrees with the asked " + label(p.top) + " "
                                                      + fmt(at(p.top)) + " MPa / " + label(p.bottom) + " " + fmt(at(p.bottom)) + " MPa = " + fmt(r) + ".");
        }
        const struct { const char* whole; const char* a; const char* b; } chains[] = { { "EzEy", "EzEx", "EyEx" }, { "GyzGxz", "GyzGxy", "GxzGxy" } };
        for (const auto& c : chains) {
            if (!has(c.whole) || !has(c.a) || !has(c.b)) continue;
            const double r = at(c.a) / at(c.b);
            if (differ(at(c.whole), r, 0.03))
                add({ c.whole, c.a, c.b }, label(c.whole) + " " + fmt(at(c.whole)) + " disagrees with " + label(c.a) + " " + fmt(at(c.a)) + " / "
                                               + label(c.b) + " " + fmt(at(c.b)) + " = " + fmt(r) + ".");
        }
        const struct { const char* mean; std::vector<std::string> axes; } means[] = { { "Emean", eAxes }, { "Gmean", gAxes } };
        for (const auto& m : means) {
            if (!has(m.mean) || !std::all_of(m.axes.begin(), m.axes.end(), has)) continue;
            const double g = std::cbrt(at(m.axes[0]) * at(m.axes[1]) * at(m.axes[2]));
            if (differ(at(m.mean), g, 0.03))
                add({ m.mean, m.axes[0], m.axes[1], m.axes[2] }, label(m.mean) + " " + fmt(at(m.mean)) + " MPa disagrees with the geometric mean of the asked "
                                                                     + label(m.axes[0]) + ", " + label(m.axes[1]) + ", " + label(m.axes[2]) + ": " + fmt(g) + " MPa.");
        }
    }

    // ── Ratios over all directions against those along the axes ──
    {
        // The largest axis ratio the request implies, with what implies it.
        auto implied = [&](const std::vector<std::string>& ratios, const std::vector<std::string>& axes, std::string* from) {
            double best = 1.0;
            for (const std::string& k : ratios)
                if (has(k) && at(k) > 0.0 && std::max(at(k), 1.0 / at(k)) > best) {
                    best = std::max(at(k), 1.0 / at(k));
                    *from = label(k) + " " + fmt(at(k));
                }
            std::vector<double> v;
            std::vector<std::string> names;
            for (const std::string& k : axes)
                if (has(k)) { v.push_back(at(k)); names.push_back(label(k)); }
            if (v.size() >= 2) {
                const double r = *std::max_element(v.begin(), v.end()) / *std::min_element(v.begin(), v.end());
                if (r > best) { best = r; *from = "the asked " + join(names, ", ") + " (largest / smallest " + fmt(r) + ")"; }
            }
            return best;
        };
        const struct { const char* all; std::vector<std::string> ratios, axes; } rules[] = {
            { "EmaxEmin", { "EzEx", "EyEx", "EzEy" }, eAxes }, { "GmaxGmin", { "GxzGxy", "GyzGxy", "GyzGxz" }, gAxes } };
        for (const auto& r : rules) {
            if (!has(r.all)) continue;
            std::string from;
            const double need = implied(r.ratios, r.axes, &from);
            if (at(r.all) < need / 1.02) {
                std::vector<std::string> keys = { r.all };
                keys.insert(keys.end(), r.ratios.begin(), r.ratios.end());
                keys.insert(keys.end(), r.axes.begin(), r.axes.end());
                add(keys, label(r.all) + " " + fmt(at(r.all)) + " is the largest ratio over all directions, so it cannot be below " + from + ".");
            }
        }
        if (has("nuMin")) {
            std::string which;
            double smallest = 1e9;
            for (const std::string& k : nuAxes)
                if (has(k) && at(k) < smallest) { smallest = at(k); which = k; }
            if (!which.empty() && at("nuMin") > smallest + 0.005)
                add({ "nuMin", which }, label("nuMin") + " " + fmt(at("nuMin")) + " is the smallest Poisson's ratio over all directions, so it cannot be above "
                                            + label(which) + " " + fmt(smallest) + ".");
        }
    }

    // ── A cell asked to be isotropic: every ratio 1, G = E / (2 (1 + ν)), K = E / (3 (1 − 2 ν)) ──
    if (isoAU) {
        std::vector<std::string> off;
        std::vector<std::string> keys = isoKeys;
        for (const char* k : { "EzEx", "EyEx", "EzEy", "GxzGxy", "GyzGxy", "GyzGxz", "EmaxEmin", "GmaxGmin", "zener" })
            if (has(k) && differ(at(k), 1.0, 0.05)) { off.push_back(label(k) + " " + fmt(at(k))); keys.push_back(k); }
        if (!off.empty())
            add(keys, isoWhat + " asks for a cell that is the same in every direction; " + join(off, ", ") + " for one that is not.");
    }
    if (iso) {
        auto level = [&](const std::vector<std::string>& axes, const std::string& mean, double* value) {
            std::vector<double> v;
            for (const std::string& k : axes)
                if (has(k)) v.push_back(at(k));
            if (has(mean)) v.push_back(at(mean));
            if (v.empty()) return false;
            double logSum = 0.0;
            for (double x : v) logSum += std::log(x);
            *value = std::exp(logSum / double(v.size()));
            return true;
        };
        double E = 0, G = 0, nu = 0;
        int nNu = 0;
        for (const std::string& k : nuAxes)
            if (has(k)) { nu += at(k); ++nNu; }
        if (nNu) nu /= nNu;
        const bool hasE = level(eAxes, "Emean", &E), hasG = level(gAxes, "Gmean", &G);
        if (hasE && hasG && nNu && stable) {
            const double g = E / (2.0 * (1.0 + nu));
            if (differ(G, g, 0.10)) {
                std::vector<std::string> keys = isoKeys;
                for (const std::string& k : { "Ex", "Ey", "Ez", "Emean", "Gxy", "Gyz", "Gxz", "Gmean", "nuXy", "nuXz", "nuYz" })
                    if (has(k)) keys.push_back(k);
                add(keys, "A cell that is the same in every direction has G = E / (2 (1 + ν)) = " + fmt(g) + " MPa for the asked E " + fmt(E)
                              + " MPa and ν " + fmt(nu) + "; asked G " + fmt(G) + " MPa.");
            }
        }
        if (hasE && nNu && has("K") && stable && nu < 0.5) {
            const double k = E / (3.0 * (1.0 - 2.0 * nu));
            if (differ(at("K"), k, 0.10)) {
                std::vector<std::string> keys = isoKeys;
                keys.push_back("K");
                add(keys, "A cell that is the same in every direction has K = E / (3 (1 − 2 ν)) = " + fmt(k) + " MPa for the asked E " + fmt(E)
                              + " MPa and ν " + fmt(nu) + "; asked K " + fmt(at("K")) + " MPa.");
            }
        }
    }

    // ── A cell with three equal axes (cubic): the Zener ratio and E max / min follow from E, G, ν ──
    {
        const bool allG = has("Gxy") && has("Gyz") && has("Gxz");
        const bool cubic = allE && allG && allNu && stable && !differ(at("Ex"), at("Ey"), 0.02) && !differ(at("Ex"), at("Ez"), 0.02)
                           && !differ(at("Gxy"), at("Gyz"), 0.02) && !differ(at("Gxy"), at("Gxz"), 0.02)
                           && std::abs(at("nuXy") - at("nuXz")) < 0.01 && std::abs(at("nuXy") - at("nuYz")) < 0.01;
        if (cubic) {
            const double E = std::cbrt(at("Ex") * at("Ey") * at("Ez")), G = std::cbrt(at("Gxy") * at("Gyz") * at("Gxz"));
            const double nu = (at("nuXy") + at("nuXz") + at("nuYz")) / 3.0;
            const double zener = 2.0 * G * (1.0 + nu) / E;
            const std::vector<std::string> base = { "Ex", "Ey", "Ez", "Gxy", "Gyz", "Gxz", "nuXy", "nuXz", "nuYz" };
            if (has("zener") && differ(at("zener"), zener, 0.05)) {
                std::vector<std::string> keys = base;
                keys.push_back("zener");
                add(keys, "With equal axes the Zener ratio is 2 G (1 + ν) / E = " + fmt(zener) + " for the asked E " + fmt(E) + " MPa, G " + fmt(G)
                              + " MPa, ν " + fmt(nu) + "; asked " + fmt(at("zener")) + ".");
            }
            // Cubic: 1 / E(n) = S11 − 2 (S11 − S12 − S44 / 2)(n1² n2² + n2² n3² + n3² n1²); along [111] that sum is 1/3.
            const double s11 = 1.0 / E, s12 = -nu / E, s44 = 1.0 / G;
            const double e111 = 1.0 / (s11 - 2.0 / 3.0 * (s11 - s12 - 0.5 * s44));
            const double eRatio = e111 > 0.0 ? std::max(E, e111) / std::min(E, e111) : 0.0;
            if (has("EmaxEmin") && eRatio > 0.0 && differ(at("EmaxEmin"), eRatio, 0.05) && !sameReported.count("EmaxEmin")) {
                std::vector<std::string> keys = base;
                keys.push_back("EmaxEmin");
                add(keys, "With equal axes the asked E, G, ν make E " + fmt(E) + " MPa along the axes and " + fmt(e111) + " MPa along the cube diagonal: "
                              "E max / min = " + fmt(eRatio) + "; asked " + fmt(at("EmaxEmin")) + ".");
            }
            const double gRatio = std::max(zener, 1.0 / zener);
            if (has("GmaxGmin") && differ(at("GmaxGmin"), gRatio, 0.05) && !sameReported.count("GmaxGmin")) {
                std::vector<std::string> keys = base;
                keys.push_back("GmaxGmin");
                add(keys, "With equal axes the asked E, G, ν make G max / min = " + fmt(gRatio) + " (the Zener ratio " + fmt(zener) + " or its inverse); asked "
                              + fmt(at("GmaxGmin")) + ".");
            }
        }
    }
    return out;
}

} // namespace reqcheck

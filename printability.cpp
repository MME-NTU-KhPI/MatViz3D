#include "printability.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace {

// Index helpers: x-fastest layout, idx = (z*N + y)*N + x.
inline std::size_t at(int x, int y, int z, int N) {
    return (static_cast<std::size_t>(z) * N + y) * N + x;
}
inline int wrap(int i, int N) { return (i % N + N) % N; }

// Connected components of the voxels where mask[idx] != 0 (6-connectivity).
// Periodic: a component is "anchored" when it wraps around the torus, i.e.
// it reaches one of its own images across a face -- detected with unwrapped
// coordinates during the flood fill (the classic wrapping-cluster test). Both
// labyrinths of a sheet gyroid wrap; a sealed bubble or a floating blob does
// not. Non-periodic: anchored = touches any face.
struct Components {
    int count = 0;
    std::vector<std::size_t> size;      // per component
    std::vector<char>        anchored;  // per component
    std::size_t anchoredVoxels = 0;
    std::size_t largest = 0;
};

Components components(const std::vector<unsigned char>& mask, int N, bool periodic)
{
    Components c;
    const std::size_t M = mask.size();
    std::vector<int> label(M, -1);
    std::vector<int> ux(M), uy(M), uz(M);   // unwrapped coordinates
    std::vector<std::size_t> stack;
    const int dx[6] = { 1, -1, 0, 0, 0, 0 }, dy[6] = { 0, 0, 1, -1, 0, 0 }, dz[6] = { 0, 0, 0, 0, 1, -1 };

    for (std::size_t seed = 0; seed < M; ++seed) {
        if (!mask[seed] || label[seed] >= 0) continue;
        const int id = c.count++;
        c.size.push_back(0);
        c.anchored.push_back(0);
        label[seed] = id;
        ux[seed] = int(seed % N); uy[seed] = int((seed / N) % N); uz[seed] = int(seed / (std::size_t(N) * N));
        stack.clear(); stack.push_back(seed);
        bool anchored = false;
        while (!stack.empty()) {
            const std::size_t i = stack.back(); stack.pop_back();
            ++c.size[id];
            const int x = int(i % N), y = int((i / N) % N), z = int(i / (std::size_t(N) * N));
            if (!periodic && (x == 0 || y == 0 || z == 0 || x == N - 1 || y == N - 1 || z == N - 1)) anchored = true;
            for (int k = 0; k < 6; ++k) {
                int nx = x + dx[k], ny = y + dy[k], nz = z + dz[k];
                if (periodic) { nx = wrap(nx, N); ny = wrap(ny, N); nz = wrap(nz, N); }
                else if (nx < 0 || ny < 0 || nz < 0 || nx >= N || ny >= N || nz >= N) continue;
                const std::size_t j = at(nx, ny, nz, N);
                if (!mask[j]) continue;
                const int ex = ux[i] + dx[k], ey = uy[i] + dy[k], ez = uz[i] + dz[k];   // expected unwrapped
                if (label[j] < 0) {
                    label[j] = id; ux[j] = ex; uy[j] = ey; uz[j] = ez;
                    stack.push_back(j);
                } else if (periodic && (ux[j] != ex || uy[j] != ey || uz[j] != ez)) {
                    anchored = true;   // met our own image: the component wraps
                }
            }
        }
        c.anchored[id] = anchored;
        if (anchored) c.anchoredVoxels += c.size[id];
        if (c.size[id] > c.largest) c.largest = c.size[id];
    }
    return c;
}

// 1-D squared Euclidean distance transform (Felzenszwalb & Huttenlocher).
// f holds 0 at feature positions and +inf elsewhere; only finite entries can
// be nearest features, so only they become parabolas.
void edt1d(const std::vector<double>& f, std::vector<double>& d)
{
    const int n = static_cast<int>(f.size());
    const double INF = std::numeric_limits<double>::infinity();
    static thread_local std::vector<int> v;
    static thread_local std::vector<double> z;
    v.assign(n, 0); z.assign(n + 1, 0.0);
    d.assign(n, INF);

    int k = -1;
    for (int q = 0; q < n; ++q) {
        if (std::isinf(f[q])) continue;
        if (k < 0) { k = 0; v[0] = q; z[0] = -INF; z[1] = INF; continue; }
        double s;
        for (;;) {
            const int vk = v[k];
            s = ((f[q] + double(q) * q) - (f[vk] + double(vk) * vk)) / (2.0 * (q - vk));
            if (s <= z[k] && k > 0) { --k; continue; }
            break;
        }
        ++k; v[k] = q; z[k] = s; z[k + 1] = INF;
    }
    if (k < 0) return;   // no features on this line

    k = 0;
    for (int q = 0; q < n; ++q) {
        while (z[k + 1] < q) ++k;
        const int vk = v[k];
        d[q] = double(q - vk) * (q - vk) + f[vk];
    }
}

// Squared Euclidean distance from every voxel to the nearest voxel with
// feature[idx] != 0. Periodic along each axis when requested (each line is
// tripled and the middle third kept, which is exact on a torus).
void edt3d(const std::vector<unsigned char>& feature, int N, bool periodic, std::vector<double>& dist2)
{
    const double INF = std::numeric_limits<double>::infinity();
    const std::size_t M = feature.size();
    dist2.assign(M, INF);
    for (std::size_t i = 0; i < M; ++i) if (feature[i]) dist2[i] = 0.0;
    if (std::none_of(feature.begin(), feature.end(), [](unsigned char c) { return c != 0; })) return;

    const int L = periodic ? 3 * N : N;
    std::vector<double> line(L), out;
    auto pass = [&](int axis) {
        for (int a = 0; a < N; ++a)
            for (int b = 0; b < N; ++b) {
                auto idx = [&](int i) {
                    if (axis == 0) return at(i, a, b, N);
                    if (axis == 1) return at(a, i, b, N);
                    return at(a, b, i, N);
                };
                for (int i = 0; i < N; ++i) {
                    const double v = dist2[idx(i)];
                    if (periodic) { line[i] = v; line[i + N] = v; line[i + 2 * N] = v; }
                    else line[i] = v;
                }
                edt1d(line, out);
                for (int i = 0; i < N; ++i) dist2[idx(i)] = periodic ? out[i + N] : out[i];
            }
    };
    pass(0); pass(1); pass(2);
}

} // namespace

const char* PrintabilityReport::directionName(int d)
{
    static const char* names[6] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };
    return (d >= 0 && d < 6) ? names[d] : "?";
}

PrintabilityReport checkPrintability(int32_t*** voxels, int N, const PrintabilityThresholds& t)
{
    PrintabilityReport r;
    if (!voxels || N < 2) { r.verdict = "no structure"; return r; }
    const std::size_t M = static_cast<std::size_t>(N) * N * N;

    std::vector<unsigned char> solid(M, 0), voidm(M, 0);
    std::size_t nsolid = 0;
    for (int z = 0; z < N; ++z)
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                const bool s = voxels[x][y][z] != 0;
                solid[at(x, y, z, N)] = s; voidm[at(x, y, z, N)] = !s;
                if (s) ++nsolid;
            }
    r.computed = true;
    r.solid_fraction = double(nsolid) / double(M);
    if (nsolid == 0) { r.verdict = "empty structure"; return r; }

    // 1. Islands and closed pores. Periodic: anchored = wraps the torus;
    // non-periodic: anchored = touches a face. A solid component that is not
    // anchored floats; a void component that is not anchored is sealed.
    {
        const Components cs = components(solid, N, t.periodic);
        r.components = cs.count;
        std::size_t anchoredSolid = cs.anchoredVoxels;
        if (!t.periodic || anchoredSolid == 0) anchoredSolid = std::max(anchoredSolid, cs.largest);
        r.island_fraction = 1.0 - double(anchoredSolid) / double(nsolid);
        if (nsolid < M) {
            const Components cv = components(voidm, N, t.periodic);
            r.void_components = cv.count;
            std::size_t openVoid = cv.anchoredVoxels;
            if (!t.periodic && openVoid == 0) openVoid = cv.largest;
            r.closed_pore_fraction = double(M - nsolid - openVoid) / double(M);
        }
    }

    // 2. Thin walls: opening with a ball of radius min_wall/2.
    const double rad = 0.5 * t.min_wall_mm / std::max(t.voxel_mm, 1e-9);
    r.opening_radius_vox = rad;
    r.thin_check_resolved = rad >= 1.0;
    if (nsolid < M) {
        std::vector<double> dSolid;                  // distance to nearest void
        edt3d(voidm, N, t.periodic, dSolid);
        double sumd = 0.0;
        for (std::size_t i = 0; i < M; ++i) if (solid[i]) sumd += std::sqrt(dSolid[i]);
        r.mean_thickness_vox = 4.0 * sumd / double(nsolid);
        std::vector<unsigned char> eroded(M, 0);
        const double rad2 = rad * rad;
        std::size_t ne = 0;
        for (std::size_t i = 0; i < M; ++i)
            if (solid[i] && dSolid[i] > rad2) { eroded[i] = 1; ++ne; }
        if (ne == 0) {
            r.thin_fraction = 1.0;
        } else {
            std::vector<double> dEroded;             // distance to the eroded set
            edt3d(eroded, N, t.periodic, dEroded);
            std::size_t kept = 0;
            for (std::size_t i = 0; i < M; ++i)
                if (solid[i] && dEroded[i] <= rad2) ++kept;
            r.thin_fraction = 1.0 - double(kept) / double(nsolid);
        }
    }

    // 3. Overhangs for the six build directions.
    const int win = std::max(0, static_cast<int>(std::lround(std::tan(t.max_overhang_deg * 3.14159265358979323846 / 180.0))));
    for (int d = 0; d < 6; ++d) {
        const int axis = d / 2;            // 0 = X, 1 = Y, 2 = Z
        const int sign = (d % 2 == 0) ? +1 : -1;   // build direction along +axis or -axis
        std::size_t unsupported = 0;
        for (int z = 0; z < N; ++z)
            for (int y = 0; y < N; ++y)
                for (int x = 0; x < N; ++x) {
                    if (!solid[at(x, y, z, N)]) continue;
                    int c[3] = { x, y, z };
                    const int layer = sign > 0 ? c[axis] : (N - 1 - c[axis]);
                    if (layer == 0) continue;   // on the bed
                    int below[3] = { x, y, z };
                    below[axis] = c[axis] - sign;
                    bool supported = false;
                    const int u = (axis == 0) ? 1 : 0, v = (axis == 2) ? 1 : 2;   // the two in-plane axes
                    for (int du = -win; du <= win && !supported; ++du)
                        for (int dv = -win; dv <= win && !supported; ++dv) {
                            int q[3] = { below[0], below[1], below[2] };
                            q[u] += du; q[v] += dv;
                            if (t.periodic) { q[u] = wrap(q[u], N); q[v] = wrap(q[v], N); }
                            else if (q[u] < 0 || q[u] >= N || q[v] < 0 || q[v] >= N) continue;
                            if (solid[at(q[0], q[1], q[2], N)]) supported = true;
                        }
                    if (!supported) ++unsupported;
                }
        r.unsupported[d] = double(unsupported) / double(nsolid);
    }
    r.best_direction = 0;
    for (int d = 1; d < 6; ++d) if (r.unsupported[d] < r.unsupported[r.best_direction]) r.best_direction = d;
    r.unsupported_best = r.unsupported[r.best_direction];

    // 4. Verdict.
    std::string why;
    if (r.island_fraction > t.max_island_fraction)
        why += "islands " + std::to_string(r.island_fraction * 100.0).substr(0, 5) + " % of the solid; ";
    if (r.thin_fraction > t.max_thin_fraction)
        why += "walls thinner than " + std::to_string(t.min_wall_mm).substr(0, 4) + " mm in " + std::to_string(r.thin_fraction * 100.0).substr(0, 5) + " % of the solid; ";
    if (r.unsupported_best > t.max_unsupported_fraction)
        why += "unsupported overhangs " + std::to_string(r.unsupported_best * 100.0).substr(0, 5) + " % even along " + std::string(PrintabilityReport::directionName(r.best_direction)) + "; ";
    r.ok = why.empty();
    r.verdict = r.ok ? "printable" : why.substr(0, why.size() - 2);
    return r;
}

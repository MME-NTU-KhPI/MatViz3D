// ============================================================================
//  tensorstreamlinebuilder.cpp  -  Hyperstreamline tube geometry.
//
//  Declarations live in tensorglyphbuilder.h, alongside the glyph builder, so
//  the two share GlyphColorMode and the colour helpers rather than growing two
//  copies of them. The implementations are separate because the integrator has
//  nothing in common with the glyph tessellator.
//
//  See the header for the three correctness requirements this file exists to
//  satisfy: interpolate components (never eigenvectors), re-fix the eigenvector
//  sign at every Runge-Kutta stage, and take the cross-section axes from a
//  transported frame rather than from the minor eigenvectors.
// ============================================================================
#include "tensorglyphbuilder.h"

#include <algorithm>
#include <cmath>

namespace {

/// One integration station: where the curve is, which way it goes, and the
/// cross-section it carries there.
struct Station
{
    mvt::Vec3 p;              ///< grid coordinates (integer == voxel centre)
    mvt::Vec3 t;              ///< unit tangent, sign-continuous along the curve
    mvt::Vec3 minor;          ///< larger-magnitude minor eigenvector
    double    r1 = 0.0;       ///< semi-axis belonging with `minor`  (world units)
    double    r2 = 0.0;       ///< semi-axis belonging with the other (world units)
    float     colorT = 0.5f;
    float     equivalent = 0.0f;
};

/// Field evaluated at one point, with the major direction already oriented.
struct LocalFrame
{
    mvt::Vec3 dir;
    mvt::Eig3 eig;
    double    m[3];           ///< eigenvalue magnitudes, descending
    int       order[3];
    double    cl = 0.0;       ///< Westin linearity
};

/// Sample, decompose, and orient the major direction to agree with `ref`.
/// False when the point is outside the grid, surrounded by void, or the tensor
/// is numerically zero.
///
/// The sign fix lives HERE rather than in the stepping loop, which is what makes
/// it happen at every Runge-Kutta stage. eigenSym3 returns an arbitrary sign, so
/// without it roughly half the k2/k3/k4 evaluations point backwards and the
/// integrator stalls in place instead of advancing.
///
/// The major direction is the magnitude-dominant eigenvector, not the
/// algebraically largest: stress is indefinite, and in a compression-dominated
/// region the algebraically largest principal value can be near zero, which
/// would make the curve follow whichever direction is least significant. This
/// also matches how the glyph builder picks a glyph's long axis, so the two
/// overlays agree.
bool localFrameAt(const TensorFieldSnapshot& snap, const StreamlineParams& p,
                  const mvt::Vec3& x, const mvt::Vec3& ref, LocalFrame& out)
{
    mvt::Sym3 T;
    if (!snap.sampleTensor(x, p.source, T)) return false;
    if (p.deviatoric) T = mvt::deviatoric(T);

    out.eig = mvt::eigenSym3(T);
    mvt::sortedMagnitudes(out.eig, out.m, out.order);
    if (out.m[0] < 1e-300) return false;

    double cp = 0.0, cs = 0.0;
    mvt::westin(out.m[0], out.m[1], out.m[2], out.cl, cp, cs);

    mvt::Vec3 d = out.eig.e[out.order[0]];
    if (mvt::dot(d, ref) < 0.0) d = mvt::scaled(d, -1.0);
    out.dir = d;
    return true;
}

mvt::Vec3 step(const mvt::Vec3& x, double h, const mvt::Vec3& d)
{
    return { x[0] + h * d[0], x[1] + h * d[1], x[2] + h * d[2] };
}

/// Integrate one half-curve from `seed` in the general sense of `dir0`,
/// appending stations in integration order.
void integrateHalf(const TensorFieldSnapshot& snap, const StreamlineParams& p,
                   const mvt::Vec3& seed, const mvt::Vec3& dir0, float lamRef,
                   std::vector<Station>& out)
{
    const double h = p.stepVoxels;
    mvt::Vec3 x   = seed;
    mvt::Vec3 ref = dir0;

    for (int n = 0; n < p.maxSteps; ++n) {
        LocalFrame f1;
        if (!localFrameAt(snap, p, x, ref, f1)) break;
        // Below this the major direction is not determined by the tensor, so
        // continuing would draw an orientation the data does not contain.
        if (f1.cl < p.minLinearity) break;

        LocalFrame f2, f3, f4;
        if (!localFrameAt(snap, p, step(x, 0.5 * h, f1.dir), f1.dir, f2)) break;
        if (!localFrameAt(snap, p, step(x, 0.5 * h, f2.dir), f2.dir, f3)) break;
        if (!localFrameAt(snap, p, step(x,       h, f3.dir), f3.dir, f4)) break;

        const mvt::Vec3 t = mvt::normalized({
            f1.dir[0] + 2.0 * f2.dir[0] + 2.0 * f3.dir[0] + f4.dir[0],
            f1.dir[1] + 2.0 * f2.dir[1] + 2.0 * f3.dir[1] + f4.dir[1],
            f1.dir[2] + 2.0 * f2.dir[2] + 2.0 * f3.dir[2] + f4.dir[2] });

        // A turn this sharp within one step means the field is degenerate here,
        // not that the curve genuinely bends that way.
        if (n > 0 && mvt::dot(t, ref) < p.minTurnDot) break;

        Station st;
        st.p     = x;
        st.t     = t;
        st.minor = f1.eig.e[f1.order[1]];

        const auto semiAxis = [&](double mag) {
            const double frac = std::clamp(mag / lamRef,
                                           static_cast<double>(p.minRadiusFrac), 1.0);
            return frac * p.tubeRadius * p.cubeSize;
        };
        st.r1 = semiAxis(f1.m[1]);
        st.r2 = semiAxis(f1.m[2]);

        st.equivalent = static_cast<float>(tensorEquivalent(f1.eig, p.source));
        switch (p.colorMode) {
        case GlyphColorMode::SignedPrincipal: {
            // Signed value of the dominant principal direction, mapped
            // symmetrically about zero so a diverging palette puts the
            // unstressed state at its midpoint.
            const double dom = f1.eig.lambda[f1.order[0]];
            st.colorT = static_cast<float>(std::clamp(0.5 + 0.5 * dom / lamRef, 0.0, 1.0));
            break;
        }
        case GlyphColorMode::SignPattern: {
            int neg = 0;
            for (int k = 0; k < 3; ++k) if (f1.eig.lambda[k] < 0.0) ++neg;
            st.colorT = static_cast<float>(neg);      // index, not a ratio
            break;
        }
        case GlyphColorMode::Equivalent:
            st.colorT = 0.0f;                         // normalized in a later pass
            break;
        case GlyphColorMode::FieldComponent: {
            const int xi = static_cast<int>(std::lround(x[0]));
            const int yi = static_cast<int>(std::lround(x[1]));
            const int zi = static_cast<int>(std::lround(x[2]));
            st.colorT = 0.5f;
            if (snap.inBounds(xi, yi, zi)) {
                const size_t idx = static_cast<size_t>(snap.denseIndex(xi, yi, zi));
                if (idx < snap.scalar.size()) st.colorT = snap.scalar[idx];
            }
            break;
        }
        }

        out.push_back(st);

        x   = step(x, h, t);
        ref = t;
    }
}

/// One step of the double-reflection rotation-minimizing frame (Wang, Juettler,
/// Zheng, Liu 2008): transport `u0` from (p0,t0) to (p1,t1) with no twist about
/// the tangent. Two reflections, no trigonometry, and exact.
mvt::Vec3 rmfTransport(const mvt::Vec3& p0, const mvt::Vec3& t0, const mvt::Vec3& u0,
                       const mvt::Vec3& p1, const mvt::Vec3& t1)
{
    const mvt::Vec3 v1{ p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
    const double c1 = mvt::dot(v1, v1);
    if (c1 < 1e-300) return u0;

    const double a = 2.0 * mvt::dot(v1, t0) / c1;
    const double b = 2.0 * mvt::dot(v1, u0) / c1;
    const mvt::Vec3 tL{ t0[0] - a*v1[0], t0[1] - a*v1[1], t0[2] - a*v1[2] };
    const mvt::Vec3 uL{ u0[0] - b*v1[0], u0[1] - b*v1[1], u0[2] - b*v1[2] };

    const mvt::Vec3 v2{ t1[0] - tL[0], t1[1] - tL[1], t1[2] - tL[2] };
    const double c2 = mvt::dot(v2, v2);
    if (c2 < 1e-300) return uL;

    const double d = 2.0 * mvt::dot(v2, uL) / c2;
    return mvt::normalized({ uL[0] - d*v2[0], uL[1] - d*v2[1], uL[2] - d*v2[2] });
}

} // namespace

int autoSeedStride(int numCubes, int budget)
{
    if (numCubes <= 0 || budget <= 0) return 1;
    const double s = numCubes / std::cbrt(static_cast<double>(budget));
    return std::max(1, static_cast<int>(std::ceil(s)));
}

StreamlineMesh buildStreamlineMesh(const TensorFieldSnapshot& snap,
                                   const StreamlineParams& p)
{
    StreamlineMesh mesh;

    if (snap.N <= 0 || !snap.has(p.source)) {
        mesh.sourceMissing = true;
        return mesh;
    }
    const float lamRef = snap.lambdaRef(p.source);
    if (!(lamRef > 0.0f)) return mesh;              // uniformly zero field

    const int N      = snap.N;
    const int stride = (p.seedStride > 0) ? p.seedStride : autoSeedStride(N, p.maxLines);
    mesh.strideUsed  = stride;

    // ---- integrate ---------------------------------------------------------
    std::vector<std::vector<Station>> lines;
    const int offset = stride / 2;                  // start inside the block

    for (int i = offset; i < N && int(lines.size()) < p.maxLines; i += stride) {
        for (int j = offset; j < N && int(lines.size()) < p.maxLines; j += stride) {
            for (int k = offset; k < N && int(lines.size()) < p.maxLines; k += stride) {
                if (!snap.occupied(k, i, j)) continue;

                const mvt::Vec3 seed{ double(k), double(i), double(j) };
                LocalFrame f0;
                if (!localFrameAt(snap, p, seed, mvt::Vec3{ 1, 0, 0 }, f0)) continue;
                if (f0.cl < p.minLinearity) continue;

                // Both ways from the seed, then spliced, so the result is one
                // curve through the seed rather than two curves meeting at it.
                std::vector<Station> back, fwd;
                integrateHalf(snap, p, seed, mvt::scaled(f0.dir, -1.0), lamRef, back);
                integrateHalf(snap, p, seed, f0.dir, lamRef, fwd);

                std::vector<Station> line;
                line.reserve(back.size() + fwd.size());
                for (size_t q = back.size(); q-- > 0; ) {
                    Station s = back[q];
                    // The backward half was integrated against the curve, so its
                    // tangents must be reversed to make one consistently
                    // oriented polyline -- otherwise the transported frame sees
                    // a 180 degree flip at the splice.
                    s.t = mvt::scaled(s.t, -1.0);
                    line.push_back(s);
                }
                line.insert(line.end(), fwd.begin(), fwd.end());

                if (line.size() >= 3) lines.push_back(std::move(line));
            }
        }
    }

    if (lines.empty()) return mesh;

    // ---- normalize the equivalent-value colour scale ------------------------
    if (p.colorMode == GlyphColorMode::Equivalent) {
        std::vector<float> eq;
        for (const auto& ln : lines)
            for (const auto& s : ln) eq.push_back(s.equivalent);
        const size_t q = static_cast<size_t>(0.99 * (eq.size() - 1));
        std::nth_element(eq.begin(), eq.begin() + q, eq.end());
        const float ref = (eq[q] > 0.0f) ? eq[q] : 1.0f;
        for (auto& ln : lines)
            for (auto& s : ln) s.colorT = std::clamp(s.equivalent / ref, 0.0f, 1.0f);
    }

    // ---- sweep the tubes ---------------------------------------------------
    const int   cols  = p.radialSegments + 1;        // seam duplicated, as for glyphs
    const float halfN = static_cast<float>(N / 2);   // integer division, per calculateScene()
    const auto  cmap  = matviz_cmap::createColorMap(64, p.palette);

    // Grid -> world is a uniform scale plus a translation, so unit direction
    // vectors carry over unchanged and only the radii need the cubeSize factor.
    const auto toWorld = [&](const mvt::Vec3& g) {
        return std::array<float, 3>{
            static_cast<float>((g[0] - halfN + 0.5) * p.cubeSize),
            static_cast<float>((g[1] - halfN + 0.5) * p.cubeSize),
            static_cast<float>((g[2] - halfN + 0.5) * p.cubeSize) };
    };

    for (const auto& ln : lines) {
        const uint32_t lineBase = static_cast<uint32_t>(mesh.verts.size());

        // Any vector perpendicular to the first tangent will do: every later
        // station is transported from its predecessor, so only the relative
        // twist matters.
        mvt::Vec3 u = mvt::normalized(mvt::cross(
            (std::abs(ln[0].t[2]) > 0.9) ? mvt::Vec3{ 1, 0, 0 } : mvt::Vec3{ 0, 0, 1 },
            ln[0].t));

        for (size_t s = 0; s < ln.size(); ++s) {
            if (s > 0) u = rmfTransport(ln[s-1].p, ln[s-1].t, u, ln[s].p, ln[s].t);

            // Re-orthogonalize against the tangent: the transport step is exact
            // but rounding accumulates over thousands of stations.
            const double du = mvt::dot(u, ln[s].t);
            u = mvt::normalized({ u[0] - du*ln[s].t[0],
                                  u[1] - du*ln[s].t[1],
                                  u[2] - du*ln[s].t[2] });
            const mvt::Vec3 w = mvt::cross(ln[s].t, u);

            // Pair the two radii with the transported axes by whichever the
            // larger minor eigenvector lies closer to. The eigenvalues thus set
            // the cross-section shape while the axes stay transported, so a
            // near-degenerate minor pair cannot twist the tube.
            const double au = std::abs(mvt::dot(ln[s].minor, u));
            const double aw = std::abs(mvt::dot(ln[s].minor, w));
            const double ru = (au >= aw) ? ln[s].r1 : ln[s].r2;
            const double rw = (au >= aw) ? ln[s].r2 : ln[s].r1;

            std::array<GLubyte, 4> col;
            if (p.colorMode == GlyphColorMode::SignPattern) {
                const int neg = std::clamp(static_cast<int>(ln[s].colorT), 0, 3);
                col = tensorSignPatternColors()[static_cast<size_t>(neg)];
            } else {
                col = matviz_cmap::scalarToColor(ln[s].colorT, cmap);
            }

            const auto c = toWorld(ln[s].p);
            for (int q = 0; q < cols; ++q) {
                const double psi = 2.0 * mvt::kPi * q / p.radialSegments;
                const double cq = std::cos(psi), sq = std::sin(psi);

                GlVertex v{};
                v.x = c[0] + static_cast<GLfloat>(ru*cq*u[0] + rw*sq*w[0]);
                v.y = c[1] + static_cast<GLfloat>(ru*cq*u[1] + rw*sq*w[1]);
                v.z = c[2] + static_cast<GLfloat>(ru*cq*u[2] + rw*sq*w[2]);

                // Ellipse gradient, NOT the radial vector. They coincide only on
                // a circular section; on a strongly elliptical one the radial
                // vector shades visibly wrong exactly where the anisotropy that
                // produced the ellipse is highest.
                const double gu = (ru > 1e-12) ? cq / ru : 0.0;
                const double gw = (rw > 1e-12) ? sq / rw : 0.0;
                const mvt::Vec3 nrm = mvt::normalized({ gu*u[0] + gw*w[0],
                                                        gu*u[1] + gw*w[1],
                                                        gu*u[2] + gw*w[2] });

                v.r = col[0]; v.g = col[1]; v.b = col[2]; v.a = col[3];
                glVertexSetNormal(v, static_cast<float>(nrm[0]),
                                     static_cast<float>(nrm[1]),
                                     static_cast<float>(nrm[2]));
                mesh.verts.push_back(v);
            }
        }

        // Quad strip between consecutive rings.
        for (size_t s = 0; s + 1 < ln.size(); ++s) {
            const uint32_t a = lineBase + static_cast<uint32_t>(s * cols);
            const uint32_t b = a + static_cast<uint32_t>(cols);
            for (int q = 0; q < p.radialSegments; ++q) {
                const uint32_t v00 = a + static_cast<uint32_t>(q);
                const uint32_t v01 = v00 + 1;
                const uint32_t v10 = b + static_cast<uint32_t>(q);
                const uint32_t v11 = v10 + 1;
                mesh.indices.insert(mesh.indices.end(), { v00, v10, v11, v00, v11, v01 });
            }
        }

        // Flat caps. Face culling is off for the overlay pass, so an open tube
        // would show its own inner wall at the ends rather than reading as a cut.
        for (int end = 0; end < 2; ++end) {
            const size_t s   = (end == 0) ? 0 : ln.size() - 1;
            const auto   c   = toWorld(ln[s].p);
            const mvt::Vec3 nrm = (end == 0) ? mvt::scaled(ln[s].t, -1.0) : ln[s].t;

            const uint32_t ring   = lineBase + static_cast<uint32_t>(s * cols);
            const uint32_t centre = static_cast<uint32_t>(mesh.verts.size());

            GlVertex v{};
            v.x = c[0]; v.y = c[1]; v.z = c[2];
            const GlVertex& src = mesh.verts[ring];
            v.r = src.r; v.g = src.g; v.b = src.b; v.a = src.a;
            glVertexSetNormal(v, static_cast<float>(nrm[0]),
                                 static_cast<float>(nrm[1]),
                                 static_cast<float>(nrm[2]));
            mesh.verts.push_back(v);

            for (int q = 0; q < p.radialSegments; ++q) {
                const uint32_t r0 = ring + static_cast<uint32_t>(q);
                const uint32_t r1 = r0 + 1;
                if (end == 0) mesh.indices.insert(mesh.indices.end(), { centre, r1, r0 });
                else          mesh.indices.insert(mesh.indices.end(), { centre, r0, r1 });
            }
        }

        mesh.stationCount += static_cast<int>(ln.size());
    }

    mesh.lineCount = static_cast<int>(lines.size());
    return mesh;
}

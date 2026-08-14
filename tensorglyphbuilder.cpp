#include "tensorglyphbuilder.h"

#include <algorithm>
#include <cmath>

namespace {

/// One selected voxel, decomposed.  Collected in a first pass so that colour
/// scales which depend on the whole selection (the equivalent-stress
/// percentile) can be computed before any geometry is emitted.
struct GlyphRecord
{
    float  wx, wy, wz;      ///< world centre
    double a[3];            ///< half-axes, largest first (magnitude order)
    mvt::Vec3 axis[3];      ///< world direction of each half-axis
    mvt::SqShape shape;
    float  colorT;          ///< 0..1 into the palette
    float  equivalent;      ///< von Mises / equivalent, before normalization
};

/// Von Mises stress, or equivalent strain, of a symmetric tensor whose shear
/// components are TENSOR shear.  Both reduce to the same expression in terms of
/// principal values up to the leading constant, so this works off eigenvalues
/// and stays source-agnostic.
double equivalentOf(const mvt::Eig3& e, TensorSource src)
{
    const double a = e.lambda[0] - e.lambda[1];
    const double b = e.lambda[1] - e.lambda[2];
    const double c = e.lambda[2] - e.lambda[0];
    const double j2 = 0.5 * (a * a + b * b + c * c);
    // sqrt(J2-form) for stress; strain uses the 2/3 factor of equivalent strain.
    return (src == TensorSource::Stress) ? std::sqrt(j2)
                                         : std::sqrt(4.0 / 9.0 * j2);
}

/// Categorical colours for the sign-pattern mode: how many principal values are
/// negative.  3 = fully compressive, 0 = fully tensile, 1/2 = mixed states.
const std::array<std::array<GLubyte, 4>, 4>& signPatternColors()
{
    static const std::array<std::array<GLubyte, 4>, 4> c = {{
        {{ 214,  64,  52, 255 }},   // 0 negative  -- triaxial tension
        {{ 236, 162,  58, 255 }},   // 1 negative
        {{  92, 176, 128, 255 }},   // 2 negative
        {{  52, 108, 214, 255 }},   // 3 negative  -- triaxial compression
    }};
    return c;
}

} // namespace

int autoGlyphStride(int numCubes, int budget)
{
    if (numCubes <= 0) return 1;
    if (budget   <= 0) return 1;
    // budget ~ (N/stride)^3  =>  stride ~ N / cbrt(budget)
    const double s = numCubes / std::cbrt(static_cast<double>(budget));
    return std::max(1, static_cast<int>(std::ceil(s)));
}

GlyphMesh buildGlyphMesh(const TensorFieldSnapshot& snap, const GlyphParams& p)
{
    GlyphMesh mesh;

    if (snap.N <= 0 || !snap.has(p.source)) {
        mesh.sourceMissing = true;
        return mesh;
    }

    const int   N       = snap.N;
    const float lamRef  = snap.lambdaRef(p.source);
    if (!(lamRef > 0.0f)) return mesh;          // uniformly zero field

    const int stride = (p.stride > 0) ? p.stride : autoGlyphStride(N, p.maxGlyphs);
    mesh.strideUsed = stride;

    // ---- pass 1: select and decompose -------------------------------------
    std::vector<GlyphRecord> recs;
    recs.reserve(static_cast<size_t>(p.maxGlyphs));

    const float half = static_cast<float>(N / 2);   // integer division, as in calculateScene()

    for (int i = 0; i < N; i += stride) {           // y
        if (p.sliceAxis == 1 && i != p.sliceIndex) continue;
        for (int j = 0; j < N; j += stride) {       // z
            if (p.sliceAxis == 2 && j != p.sliceIndex) continue;
            for (int k = 0; k < N; k += stride) {   // x
                if (p.sliceAxis == 0 && k != p.sliceIndex) continue;

                mvt::Sym3 T;
                if (!snap.voxelTensor(k, i, j, p.source, T)) continue;
                if (p.deviatoric) T = mvt::deviatoric(T);

                const mvt::Eig3 E = mvt::eigenSym3(T);

                double m[3];
                int    order[3];
                mvt::sortedMagnitudes(E, m, order);

                // A tensor that is zero to within rounding has no orientation
                // to draw; emitting a glyph anyway produces a degenerate
                // speck whose exponents come out of a 0/0.
                if (m[0] < 1e-12 * lamRef) continue;

                GlyphRecord r{};

                double cl, cp, cs;
                mvt::westin(m[0], m[1], m[2], cl, cp, cs);
                r.shape = mvt::kindlmann(cl, cp, p.gamma);

                for (int t = 0; t < 3; ++t) {
                    const double frac = std::clamp(m[t] / lamRef,
                                                   static_cast<double>(p.minAxisFrac), 1.0);
                    r.a[t]    = frac * p.maxHalfAxis * p.cubeSize;
                    // Orientation from the algebraically sorted eigenvectors,
                    // permuted into magnitude order so shape and orientation
                    // refer to the same axis.
                    r.axis[t] = E.e[order[t]];
                }

                const size_t idx = static_cast<size_t>(snap.denseIndex(k, i, j));

                // World placement: identical mapping to calculateScene(), plus
                // half a cube to land on the voxel centre rather than its
                // lower corner.
                float ox = 0.0f, oy = 0.0f, oz = 0.0f;
                const int32_t gid = snap.grainId[idx];
                if (!p.grainOffset.empty() && gid > 0 &&
                    static_cast<size_t>(gid - 1) < p.grainOffset.size())
                {
                    const auto& g = p.grainOffset[static_cast<size_t>(gid - 1)];
                    ox = g[0]; oy = g[1]; oz = g[2];
                }
                if (p.showDeformed && snap.hasDisp && idx < snap.disp.size()) {
                    ox += snap.disp[idx][0] * p.deformedScale;
                    oy += snap.disp[idx][1] * p.deformedScale;
                    oz += snap.disp[idx][2] * p.deformedScale;
                }

                r.wx = -(half - k) * p.cubeSize + 0.5f * p.cubeSize + ox;
                r.wy = -(half - i) * p.cubeSize + 0.5f * p.cubeSize + oy;
                r.wz = -(half - j) * p.cubeSize + 0.5f * p.cubeSize + oz;

                // ---- colour ---------------------------------------------
                r.equivalent = static_cast<float>(equivalentOf(E, p.source));
                switch (p.colorMode) {
                case GlyphColorMode::SignedPrincipal: {
                    // The algebraically largest or smallest eigenvalue,
                    // whichever dominates in magnitude, mapped symmetrically
                    // about zero so the diverging palette's midpoint is the
                    // unstressed state.
                    const double dom = (std::abs(E.lambda[0]) >= std::abs(E.lambda[2]))
                                         ? E.lambda[0] : E.lambda[2];
                    r.colorT = static_cast<float>(
                        std::clamp(0.5 + 0.5 * dom / lamRef, 0.0, 1.0));
                    break;
                }
                case GlyphColorMode::SignPattern: {
                    int neg = 0;
                    for (int t = 0; t < 3; ++t) if (E.lambda[t] < 0.0) ++neg;
                    r.colorT = static_cast<float>(neg);   // index, not a ratio
                    break;
                }
                case GlyphColorMode::Equivalent:
                    r.colorT = 0.0f;                      // filled in by pass 1.5
                    break;
                case GlyphColorMode::FieldComponent:
                    r.colorT = (idx < snap.scalar.size()) ? snap.scalar[idx] : 0.5f;
                    break;
                }

                recs.push_back(r);
            }
        }
    }

    if (recs.empty()) return mesh;

    // ---- pass 1.5: normalize the equivalent-value colour scale -------------
    // Done over the selection rather than against lambdaRef so the palette
    // spans the range actually on screen. Percentile, not maximum, for the same
    // outlier reason lambdaRef uses one.
    if (p.colorMode == GlyphColorMode::Equivalent) {
        std::vector<float> eq;
        eq.reserve(recs.size());
        for (const auto& r : recs) eq.push_back(r.equivalent);
        const size_t q = static_cast<size_t>(0.99 * (eq.size() - 1));
        std::nth_element(eq.begin(), eq.begin() + q, eq.end());
        const float ref = (eq[q] > 0.0f) ? eq[q] : 1.0f;
        for (auto& r : recs) r.colorT = std::clamp(r.equivalent / ref, 0.0f, 1.0f);
    }

    // ---- pass 2: emit geometry --------------------------------------------
    const int rows = p.thetaSteps + 1;      // theta = 0 .. pi
    const int cols = p.phiSteps + 1;        // phi   = 0 .. 2pi, seam duplicated
    const int vertsPerGlyph = rows * cols;
    const int idxPerGlyph   = p.thetaSteps * p.phiSteps * 6;

    const auto cmap = matviz_cmap::createColorMap(64, p.palette);

    mesh.verts.reserve(static_cast<size_t>(recs.size()) * vertsPerGlyph);
    mesh.indices.reserve(static_cast<size_t>(recs.size()) * idxPerGlyph);

    for (const auto& r : recs) {
        const uint32_t base = static_cast<uint32_t>(mesh.verts.size());

        std::array<GLubyte, 4> col;
        if (p.colorMode == GlyphColorMode::SignPattern) {
            const int n = std::clamp(static_cast<int>(r.colorT), 0, 3);
            col = signPatternColors()[static_cast<size_t>(n)];
        } else {
            col = matviz_cmap::scalarToColor(r.colorT, cmap);
        }

        for (int rr = 0; rr < rows; ++rr) {
            const double theta = mvt::kPi * rr / p.thetaSteps;
            for (int cc = 0; cc < cols; ++cc) {
                const double phi = 2.0 * mvt::kPi * cc / p.phiSteps;

                const mvt::Vec3 lp = mvt::superquadricPoint (r.shape, theta, phi);
                const mvt::Vec3 ln = mvt::superquadricNormal(r.shape, theta, phi);

                // Local -> world.  Local axis t is the eigenvector of magnitude
                // rank t, scaled by that half-axis.
                double px = 0, py = 0, pz = 0;
                for (int t = 0; t < 3; ++t) {
                    const double s = lp[t] * r.a[t];
                    px += s * r.axis[t][0];
                    py += s * r.axis[t][1];
                    pz += s * r.axis[t][2];
                }

                // Normals transform by the INVERSE TRANSPOSE of the scaling,
                // i.e. divide by each half-axis instead of multiplying.  Using
                // the forward scale here would tilt every normal on an
                // anisotropic glyph -- most visibly on the flat faces, which is
                // exactly where the shape is carrying the information.
                double nx = 0, ny = 0, nz = 0;
                for (int t = 0; t < 3; ++t) {
                    const double s = ln[t] / std::max(r.a[t], 1e-12);
                    nx += s * r.axis[t][0];
                    ny += s * r.axis[t][1];
                    nz += s * r.axis[t][2];
                }
                const mvt::Vec3 wn = mvt::normalized({ nx, ny, nz });

                GlVertex v{};
                v.x = r.wx + static_cast<GLfloat>(px);
                v.y = r.wy + static_cast<GLfloat>(py);
                v.z = r.wz + static_cast<GLfloat>(pz);
                v.r = col[0]; v.g = col[1]; v.b = col[2]; v.a = col[3];
                glVertexSetNormal(v, static_cast<float>(wn[0]),
                                     static_cast<float>(wn[1]),
                                     static_cast<float>(wn[2]));
                mesh.verts.push_back(v);
            }
        }

        for (int rr = 0; rr < p.thetaSteps; ++rr) {
            for (int cc = 0; cc < p.phiSteps; ++cc) {
                const uint32_t v00 = base + static_cast<uint32_t>(rr * cols + cc);
                const uint32_t v01 = v00 + 1;
                const uint32_t v10 = v00 + static_cast<uint32_t>(cols);
                const uint32_t v11 = v10 + 1;
                mesh.indices.insert(mesh.indices.end(), { v00, v10, v11, v00, v11, v01 });
            }
        }
    }

    mesh.glyphCount = static_cast<int>(recs.size());
    return mesh;
}

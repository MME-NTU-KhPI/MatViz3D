// ============================================================================
//  tensorglyphbuilder.h  -  Tensor field geometry: superquadric glyphs and
//                           hyperstreamline tubes.
//
//  Turns a TensorFieldSnapshot into one indexed triangle mesh in the app's
//  standard GlVertex layout, ready to hand straight to a VBO.  Pure geometry:
//  no Qt, no GL calls, no singletons -- so it is safe to call from a
//  QtConcurrent worker, which is the whole reason it is a free function over an
//  immutable snapshot rather than a method on the widget.
//
//  The glyph shape follows Kindlmann (2004): eigenvalue ratios pick a point on
//  the Westin linear/planar/spherical barycentric triangle, which picks the two
//  superquadric exponents, so that the *shape* of the glyph is readable at a
//  glance and rotationally symmetric exactly where the tensor is.  The maths
//  lives in tensormath.hpp; this file is about which voxels get a glyph, how
//  big it is, where it goes, and what colour it takes.
//
//  Sign handling is the part that differs from the diffusion-MRI literature
//  superquadric glyphs come from.  Diffusion tensors are positive definite;
//  stress is not.  Shape is therefore taken from eigenvalue MAGNITUDES (so the
//  Westin denominator stays positive and no half-axis goes negative), while
//  ORIENTATION comes from the algebraically sorted eigenvectors permuted into
//  the same magnitude order.  Sign is carried by colour instead of geometry --
//  a glyph cannot show you the difference between tension and compression, and
//  pretending otherwise by letting an axis go negative just mirrors the shape.
// ============================================================================
#ifndef TENSORGLYPHBUILDER_H
#define TENSORGLYPHBUILDER_H

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "colormap.hpp"
#include "glvertex.hpp"
#include "tensorfieldsnapshot.h"

// ---------------------------------------------------------------------------
//  Shared colour helpers, used by both the glyph and the streamline builder.
// ---------------------------------------------------------------------------
/// Von Mises stress, or equivalent strain, from eigenvalues.  Both reduce to the
/// same expression in principal values up to the leading constant, so this works
/// off the spectrum and stays source-agnostic.
inline double tensorEquivalent(const mvt::Eig3& e, TensorSource src)
{
    const double a = e.lambda[0] - e.lambda[1];
    const double b = e.lambda[1] - e.lambda[2];
    const double c = e.lambda[2] - e.lambda[0];
    const double j2 = 0.5 * (a * a + b * b + c * c);
    return (src == TensorSource::Stress) ? std::sqrt(j2) : std::sqrt(4.0 / 9.0 * j2);
}

/// Categorical colours keyed by how many principal values are negative:
/// 0 = triaxial tension ... 3 = triaxial compression.
inline const std::array<std::array<GLubyte, 4>, 4>& tensorSignPatternColors()
{
    static const std::array<std::array<GLubyte, 4>, 4> c = {{
        {{ 214,  64,  52, 255 }},   // 0 negative -- triaxial tension
        {{ 236, 162,  58, 255 }},   // 1 negative
        {{  92, 176, 128, 255 }},   // 2 negative
        {{  52, 108, 214, 255 }},   // 3 negative -- triaxial compression
    }};
    return c;
}

/// What the glyph colour encodes.  Ordinals are part of the QML API.
enum class GlyphColorMode {
    SignedPrincipal = 0,  ///< signed dominant principal value, diverging palette
    SignPattern     = 1,  ///< categorical by number of negative eigenvalues
    Equivalent      = 2,  ///< von Mises stress / equivalent strain
    FieldComponent  = 3,  ///< the scalar the voxels themselves are coloured by
};

struct GlyphParams
{
    TensorSource source     = TensorSource::Stress;
    bool         deviatoric = false;   ///< strip the hydrostatic part first

    /// Sample every stride'th voxel along each axis.  <= 0 means "choose one
    /// automatically so the glyph count lands near maxGlyphs".
    int stride    = 0;
    int maxGlyphs = 20000;

    /// Optional single-slice restriction: axis 0=x, 1=y, 2=z; -1 disables.
    int sliceAxis  = -1;
    int sliceIndex = 0;

    float maxHalfAxis = 0.45f;  ///< half-extent of the largest axis, voxel units
    float minAxisFrac = 0.05f;  ///< floor on a half-axis, as a fraction of the above
    float gamma       = 3.0f;   ///< Kindlmann sharpness exponent

    int thetaSteps = 8;   ///< latitude segments (thetaSteps+1 rings)
    int phiSteps   = 16;  ///< longitude segments (phiSteps+1 columns, seam duplicated)

    GlyphColorMode       colorMode = GlyphColorMode::SignedPrincipal;
    matviz_cmap::Palette palette   = matviz_cmap::Palette::CoolWarm;

    // ---- placement, mirroring OpenGLWidgetQML::calculateScene() -------------
    int   numCubes = 0;
    float cubeSize = 1.0f;

    /// Exploded-view displacement per grain, indexed by (grainId - 1).  Empty
    /// means no explode.  Supplied by the caller because it is derived from the
    /// grain colour table, which lives in the widget.
    std::vector<std::array<float, 3>> grainOffset;

    bool  showDeformed  = false;
    float deformedScale = 1.0f;
};

struct GlyphMesh
{
    std::vector<GlVertex> verts;
    std::vector<uint32_t> indices;
    int glyphCount    = 0;
    int strideUsed    = 1;
    bool sourceMissing = false;   ///< snapshot had no data for the requested source
};

/// Stride that samples about `budget` voxels out of an N^3 grid.
int autoGlyphStride(int numCubes, int budget);

GlyphMesh buildGlyphMesh(const TensorFieldSnapshot& snap, const GlyphParams& p);

// ============================================================================
//  Hyperstreamlines
// ============================================================================
//  A tube swept along an integral curve of the major principal direction, whose
//  elliptical cross-section carries the two minor eigenvalues -- so one object
//  shows all three principal magnitudes and the full orientation, continuously,
//  where a field of glyphs shows them only at sample points.
//
//  Three things here are correctness requirements rather than choices:
//
//  1. The six tensor COMPONENTS are interpolated and the result is
//     eigendecomposed -- never the other way round. Interpolating eigenvectors
//     is not a well-posed operation: they are defined only up to sign, and
//     within a degenerate subspace only up to an arbitrary rotation.
//     (TensorFieldSnapshot::sampleTensor does the interpolation.)
//
//  2. The eigenvector sign is re-fixed at EVERY Runge-Kutta stage against the
//     direction carried into that stage. eigenSym3 returns an arbitrary sign, so
//     without this k2 points backwards about half the time and the integrator
//     stalls instead of advancing.
//
//  3. The cross-section axes come from a rotation-minimizing frame (parallel
//     transport), not from the minor eigenvectors directly. Taking the
//     eigenvectors makes the tube snap through 90 degrees from one step to the
//     next wherever the two minor eigenvalues are close, because their labelling
//     inside a near-degenerate subspace is arbitrary. The eigenvalues still set
//     the two radii; only the axes are transported.
// ============================================================================

struct StreamlineParams
{
    TensorSource source     = TensorSource::Stress;
    bool         deviatoric = false;

    /// Seeds are placed on a lattice every seedStride voxels. <= 0 picks one
    /// automatically so the line count lands near maxLines.
    int seedStride = 0;
    int maxLines   = 200;

    double stepVoxels    = 0.25;  ///< RK4 step, in voxels
    int    maxSteps      = 2000;  ///< per direction, so a curve is 2x this at most
    double minLinearity  = 0.15;  ///< stop where Westin cl falls below this
    double minTurnDot    = 0.2;   ///< stop on a turn sharper than acos(this)

    float tubeRadius    = 0.35f;  ///< largest cross-section semi-axis, voxel units
    float minRadiusFrac = 0.15f;  ///< floor on a semi-axis, fraction of the above
    int   radialSegments = 10;    ///< around the tube

    GlyphColorMode       colorMode = GlyphColorMode::SignedPrincipal;
    matviz_cmap::Palette palette   = matviz_cmap::Palette::CoolWarm;

    int   numCubes = 0;
    float cubeSize = 1.0f;
};

struct StreamlineMesh
{
    std::vector<GlVertex> verts;
    std::vector<uint32_t> indices;
    int  lineCount     = 0;
    int  stationCount  = 0;   ///< total integration stations across all lines
    int  strideUsed    = 1;
    bool sourceMissing = false;
};

/// Seed lattice stride that yields about `budget` seeds in an N^3 grid.
int autoSeedStride(int numCubes, int budget);

StreamlineMesh buildStreamlineMesh(const TensorFieldSnapshot& snap,
                                   const StreamlineParams& p);

#endif // TENSORGLYPHBUILDER_H

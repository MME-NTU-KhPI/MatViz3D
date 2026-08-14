// ============================================================================
//  tensorglyphbuilder.h  -  Superquadric tensor glyph geometry.
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
#include <cstdint>
#include <vector>

#include "colormap.hpp"
#include "glvertex.hpp"
#include "tensorfieldsnapshot.h"

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

#endif // TENSORGLYPHBUILDER_H

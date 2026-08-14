// ============================================================================
//  stiffnesssurfacebuilder.h  -  Directional elastic property surfaces.
//
//  Turns a 6x6 elastic matrix into a closed theta-phi radial surface mesh whose
//  radius is |q(n)| for a chosen directional quantity q, ready to hand to a
//  renderer.  No Qt objects and no GL calls: geometry only.
//
//  Input matrices arrive in one of two bases (see Basis).  Everything is
//  normalized to GPa internally so the UI never has to switch units.
// ============================================================================
#pragma once

#include "colormap.hpp"
#include "glvertex.hpp"
#include "tensormath.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace mvsurf {

/// Which 6x6 convention the incoming matrix uses.
enum class Basis {
    VoigtGPa    = 0,  ///< material_properties rows: Voigt order, GPa
    PipelinePa  = 1   ///< StiffnessMatrixResult::C: pipeline order, Pa
};

enum class Quantity {
    YoungsE = 0,
    ShearGmin,
    ShearGmax,
    ShearGmean,
    PoissonMin,
    PoissonMax,
    PoissonMean,
    LinearCompressibility,
    C_component,
    S_component
};

/// Everything derived once from the matrix, reused across parameter changes.
struct ElasticState {
    bool valid = false;

    mvt::Mat6 Cm{};       ///< Mandel stiffness  [GPa]
    mvt::Mat6 Sm{};       ///< Mandel compliance [1/GPa]
    mvt::C4   C4s{};      ///< 4th-order stiffness  [GPa]
    mvt::C4   S4s{};      ///< 4th-order compliance [1/GPa]

    bool   cubic   = false;   ///< true if the matrix has cubic symmetry
    double c11 = 0, c12 = 0, c44 = 0;
    double zener   = 1.0;     ///< meaningful only when cubic
    double bulkVRH = 0.0;     ///< Voigt-Reuss-Hill bulk modulus [GPa]
};

/// How the free twist about the plotted direction is resolved for a component
/// surface.
///
/// A component naming two or more axes is a function of the direction AND of the
/// rotation about it -- one direction does not fix a frame. No continuous choice
/// of frame exists over the whole sphere (hairy ball), so ANY fixed twist puts a
/// defect in the surface. Measured on cubic copper, a fixed twist violates the
/// crystal's own 24-fold symmetry by as much as the entire range of the
/// quantity: the surface is then mostly artefact, showing a funnel where the
/// frame degenerates.
///
/// Averaging or extremizing over the twist removes that entirely, because the
/// result depends on the direction alone. Mean is exact to rounding (a uniform
/// grid integrates the trigonometric polynomial exactly, and the average
/// commutes with the crystal symmetry); Min and Max are correct to the grid
/// step, and mirror the existing G-min/G-max and nu-min/nu-max quantities.
enum class TwistMode {
    Mean  = 0,  ///< average over the twist -- exactly symmetry-respecting
    Min   = 1,
    Max   = 2,
    Fixed = 3   ///< the user's spinDeg; frame-dependent, for inspection only
};

struct Params {
    Quantity quantity = Quantity::YoungsE;
    int      ci = 0, cj = 0;           ///< Voigt indices for *_component
    double   spinDeg = 0.0;            ///< twist about n, used only by TwistMode::Fixed
    TwistMode twist  = TwistMode::Mean;
    int      nTheta = 48, nPhi = 96;
    int      nAzimuth = 36;            ///< perpendicular samples for G and nu
    double   worldRadius = 1.0;        ///< radius the peak |q| maps to
    int      colorLevels = 64;
    matviz_cmap::Palette palette = matviz_cmap::Palette::Viridis;
};

struct Mesh {
    std::vector<GlVertex> verts;
    std::vector<uint32_t> indices;

    bool   valid = false;
    double minValue = 0.0;        ///< min over the sampled directions
    double maxValue = 0.0;
    double anisotropyRatio = 1.0; ///< max/min of |q|; 1 for an isotropic surface
    const char* unit = "";
};

/**
 * @brief Derive the reusable elastic state from a 6x6 matrix.
 * @param M    the matrix, row-major
 * @param basis how to interpret it
 * @return false if the matrix is singular (a blank database row is all zeros),
 *         in which case callers must show an error rather than draw NaNs.
 */
bool buildElasticState(const double M[6][6], Basis basis, ElasticState& out);

/// Build the radial surface mesh. Returns an invalid Mesh if state is invalid.
Mesh buildSurface(const ElasticState& state, const Params& params);

/// Human-readable name of a quantity, for UI labels.
const char* quantityName(Quantity q);

/// Unit string of a quantity, given that everything is normalized to GPa.
const char* quantityUnit(Quantity q);

} // namespace mvsurf

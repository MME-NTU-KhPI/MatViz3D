#ifndef CELLFORMS_H
#define CELLFORMS_H

#include <array>
#include <cstdint>
#include <random>
#include <vector>

#include "mycelium.h"

/**
 * @brief Bio-inspired and architected cell forms for the Lattice algorithm.
 *
 * Pure C++ (no Qt), so a test can build a form without the algorithm. Every
 * form is shaped by three knobs a, b, c in [0, 1] (the CLI keys `form_a`,
 * `form_b`, `form_c`), each mapped linearly onto the physical range of one
 * shape parameter of that form (`knobLabel()` names it), so a dataset samples
 * every form from the unit cube; the random forms draw the rest from the seed.
 *
 * Two kinds of form:
 *
 *   - segment forms: a table of straight pieces with thickness weights
 *     (mycelium::Segment, at most 0.25 cell edges long), F = distance to the
 *     nearest piece / its weight -- the Lattice evaluates them through the
 *     mycelium's periodic bucket grid;
 *       glass sponge  -- the square grid of Euplectella's wall with two
 *                        parallel diagonals each way in every other square
 *                        (Fernandes et al. 2021); a = diagonal offset,
 *                        b = diagonal / grid thickness, c > 1/2 braces the
 *                        horizontal planes too;
 *       chiral twist  -- a ring on every face and 12 bars between the rings
 *                        of adjacent faces, repeated by the 24 rotations of the
 *                        cube (Frenzel, Kadic, Wegener 2017): compression twists
 *                        it; a = ring radius, b = where a bar leaves its ring,
 *                        c = bar thickness;
 *       woven         -- plain-weave layers of warp (X) and weft (Y) yarns,
 *                        stacked so the crimps touch; a: 3 or 4 layers, b = crimp;
 *       bouligand     -- the helicoidal plywood of the mantis-shrimp club:
 *                        eight plies of parallel fibres along lattice directions
 *                        turning through 180 degrees, every fibre a flat ribbon
 *                        so the plies touch; a = fibre spacing;
 *       diamond       -- ball-and-stick diamond lattice; a = atom size;
 *       re-entrant 3D -- vertical ribs at the BCC columns joined by bars that
 *                        fall from a rib's top to the next rib's foot (auxetic);
 *                        a = rib length (depth of the re-entrance),
 *                        b = bar thickness, c = node balls;
 *   - field forms: a field evaluated per point (`Field`);
 *       radiolarian   -- perforated nested shells at the cell centre and corner
 *                        joined by eight <111> spines; a = shell radius,
 *                        b = pore size, c = spine thickness;
 *       bone          -- trabecular bone: the edges (rods) and a share of the
 *                        faces (perforation-free plates) of a random Voronoi
 *                        partition, in a metric stretched along Z;
 *                        a = number of cells, b = stretch, c = plate share;
 *       wood          -- softwood: tracheids as a random 2D Voronoi honeycomb
 *                        extruded along Z, walls thicker in the latewood band of a
 *                        growth ring across X, three round pits in every wall;
 *                        a = tracheids, b = latewood contrast, c = pit size;
 *       cuttlebone    -- horizontal septa joined by labyrinth walls that are
 *                        straight at a chamber's floor and wavy under its
 *                        ceiling; a = chambers, b = waviness, c = labyrinth scale;
 *       turing        -- a Gray-Scott reaction-diffusion pattern grown on a
 *                        periodic G^3 grid from seeded patches (labyrinth / worm
 *                        regime), read by periodic Catmull-Rom interpolation;
 *                        a signed level set (solid where the activator is high);
 *                        a = feed / kill along the regime line, b = pattern scale;
 *       hierarchical gyroid -- a thick gyroid sheet whose walls are a fine
 *                        gyroid sheet; a = scale ratio (2, 3, 4), b = outer wall.
 *
 * The three new triply periodic minimal surfaces (Neovius, Fischer-Koch S,
 * Lidinoid) are level sets like the gyroid and live in the Lattice; this file
 * supplies their exact Fourier expansions (`tpmsWaves()`).
 */
namespace cellforms {

enum class Kind {
    GlassSponge, ChiralTwist, Woven, Bouligand, Diamond, Reentrant3D,   // segment forms
    Radiolarian, Bone, Wood, Cuttlebone, Turing, HierGyroid             // field forms
};

struct Knobs { double a = 0.5, b = 0.5, c = 0.5; };

bool isSegmentKind(Kind k);
/// The shape parameter knob 0 / 1 / 2 sets for this form, or nullptr when it is unused.
const char* knobLabel(Kind k, int knob);

/// Segment forms: the pieces of one unit cell (weights 1 = the thickest bars).
std::vector<mycelium::Segment> segments(Kind k, const Knobs& kn);

/// Field forms: built once, then evaluated per point of the unit cell.
class Field {
public:
    /// Draws what the form needs from `rng` (bone, wood, cuttlebone, turing).
    void build(Kind k, const Knobs& kn, std::mt19937& rng);
    /// The field at a point of the unit cell, coordinates in [0, 1): a distance
    /// (cell edges) divided by a thickness weight, +inf where the form is cut
    /// away, or -- for turing -- the signed, unit-variance activator level.
    double at(double x, double y, double z) const;
    /// Smallest thickness weight: the thinnest wall is 2 c w thick (0 for turing).
    double minWeight() const { return m_wmin; }
    bool   signedLevel() const { return m_kind == Kind::Turing; }
    void   clear();

private:
    double radiolarian(double x, double y, double z) const;
    double bone(double x, double y, double z) const;
    double wood(double x, double y, double z) const;
    double cuttlebone(double x, double y, double z) const;
    double turing(double x, double y, double z) const;
    double hierGyroid(double x, double y, double z) const;

    // The shape parameters of the forms. build() sets every parameter of the form it
    // builds from the knobs a, b, c in [0, 1]; the formula and the range it gives are
    // written at each one. The numbers in the initializers are only the state before
    // build(): placeholders inside those ranges, never read for a built form.
    // Lengths are in cell edges (the unit cell is 1 x 1 x 1); a thickness weight
    // scales the wall or bar: it is 2 c w thick, c the level the density is solved for.
    Kind   m_kind = Kind::Radiolarian;
    Knobs  m_kn;
    double m_wmin = 1.0;                         ///< smallest thickness weight of the built form (minWeight())
    std::uint64_t m_salt = 0;                    ///< seeded hash salt (rod weights, plates, pits)

    // radiolarian
    /// Radii of the three shells: [0] outer shell about the cell centre, 0.24 + 0.10 a
    /// (0.24 ... 0.34); [1] inner shell about the centre, 0.5 of the outer;
    /// [2] shell about the cell corner, 0.55 of the outer.
    double m_R[3] = { 0.3, 0.15, 0.17 };
    /// Pore size factor, 0.7 + 0.6 b (0.7 ... 1.3): multiplies the half-angles of the
    /// pores (17 and 12 degrees on the outer shell, 24 degrees on the other two).
    double m_pore = 1.0;
    /// Thickness weight of the spines, 0.8 + 0.8 c (0.8 ... 1.6); the shells have 1.
    double m_spineW = 1.0;
    /// Where a spine ends, measured from the cell centre along <111>: half the cube
    /// diagonal (sqrt(3) / 2) minus 0.9 of the corner shell's radius, so the spine
    /// ends just inside the corner shell.
    double m_spineEnd = 0.7;

    // bone / wood: sites and their periodic images (scaled coordinates)
    std::vector<std::array<double,3>> m_img;
    std::vector<int> m_imgSite;
    /// bone: stretch of the Voronoi cells along Z, 1 + b (1 = equiaxed ... 2).
    double m_stretch = 1.0;
    /// bone: share of the Voronoi faces filled by a plate, 0.4 c (0 = rods only ... 0.4).
    double m_plates = 0.0;
    /// wood: extra thickness weight of the walls in the latewood band, 0.2 + 0.4 b
    /// (0.2 ... 0.6); the weight runs from 0.75 (earlywood) to 0.75 + this (x = 1/2).
    double m_late = 0.6;
    /// wood: radius of a pit, 0.03 + 0.04 c (0.03 ... 0.07).
    double m_pit = 0.05;

    // cuttlebone
    /// 2D waves {kx, ky, amplitude, phase} of the labyrinth field g (up to 6 waves about
    /// |k| = 1.6 + 1.4 c per cell edge) and of the corrugation h (up to 10 about twice that).
    std::vector<std::array<double,4>> m_g, m_h;
    /// Chambers per cell along Z, 1 + round(2 a) (1, 2 or 3).
    int    m_chambers = 2;
    /// Weight of the corrugation h against g, 0.4 b (0 = straight walls ... 0.4):
    /// the walls are the zero set of g + m_wavy sin(pi z) z h, z the height inside
    /// a chamber (0 at its floor, 1 at its ceiling).
    double m_wavy = 0.3;

    // turing
    /// Points per cell edge of the reaction-diffusion grid, round(32 + 16 b) (32 ... 48).
    /// The pattern has a fixed wavelength in grid points, so a larger grid is a finer pattern.
    int    m_G = 40;
    std::vector<float> m_v;                      ///< activator on the G^3 grid, zero mean, unit variance

    // hierarchical gyroid
    /// Periods of the fine gyroid per period of the coarse one, 2 + round(2 a) (2, 3 or 4).
    int    m_scale = 3;
    /// Half thickness of the coarse gyroid sheet, 0.08 + 0.07 b (0.08 ... 0.15);
    /// outside it the form is cut away.
    double m_outer = 0.11;
};

/// One cosine wave of a level set: k (per cell edge), amplitude, phase.
struct Wave { double k[3]; double amp; double phase; };
enum class Surface { Neovius, FischerKochS, Lidinoid };
/// Exact Fourier expansion of the level-set approximation (a zero vector is a constant term).
std::vector<Wave> tpmsWaves(Surface s);

} // namespace cellforms

#endif // CELLFORMS_H

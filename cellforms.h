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

    Kind   m_kind = Kind::Radiolarian;
    Knobs  m_kn;
    double m_wmin = 1.0;
    std::uint64_t m_salt = 0;                    ///< seeded hash salt (rod weights, plates, pits)
    // radiolarian
    double m_R[3] = { 0.3, 0.15, 0.17 };         ///< outer, inner, corner shell radius
    double m_pore = 1.0, m_spineW = 1.0, m_spineEnd = 0.7;
    // bone / wood: sites and their periodic images (scaled coordinates)
    std::vector<std::array<double,3>> m_img;
    std::vector<int> m_imgSite;
    double m_stretch = 1.0, m_plates = 0.0;
    double m_late = 0.6, m_pit = 0.05;
    // cuttlebone: 2D waves kx ky amplitude phase, labyrinth g and corrugation h
    std::vector<std::array<double,4>> m_g, m_h;
    int    m_chambers = 2;
    double m_wavy = 0.3;
    // turing
    int    m_G = 40;
    std::vector<float> m_v;
    // hierarchical gyroid
    int    m_scale = 3;
    double m_outer = 0.11;
};

/// One cosine wave of a level set: k (per cell edge), amplitude, phase.
struct Wave { double k[3]; double amp; double phase; };
enum class Surface { Neovius, FischerKochS, Lidinoid };
/// Exact Fourier expansion of the level-set approximation (a zero vector is a constant term).
std::vector<Wave> tpmsWaves(Surface s);

} // namespace cellforms

#endif // CELLFORMS_H

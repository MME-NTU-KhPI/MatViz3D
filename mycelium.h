#ifndef MYCELIUM_H
#define MYCELIUM_H

#include <array>
#include <random>
#include <vector>

/**
 * @brief Growth of a fungal mycelium in the periodic unit cell, for the
 *        Lattice family `mycelium`.
 *
 * Pure C++ (no Qt), so a test can grow a colony without the algorithm.
 *
 * The colony is a graph: nodes every `kStep` cell edges along every hypha
 * (wrapped into [0, 1)^3), a hypha a chain of nodes that starts on its
 * parent's node (a branch) or at a spore, and may end on a node of another
 * hypha (a fusion, anastomosis). What drives the growth:
 *
 *   - substrate: a nutrient field on a periodic 32^3 grid. Every new node takes
 *     it up around itself, it diffuses, tips turn up its gradient, branch in
 *     proportion to it and stop where it has run out -- so the hyphae forage
 *     into the parts of the cell no other hypha has reached yet;
 *   - branching: a branch every `unit` of hypha length on rich substrate (the
 *     hyphal growth unit: total length over the number of tips stays roughly
 *     constant in a growing colony), proportionally less often on poor one,
 *     at `angle` to its parent;
 *   - avoidance: tips turn away from the hyphae around them;
 *   - fusion: a tip that comes within `fuse` of another hypha joins it;
 *   - hierarchy: every branch order is `taper` times thinner (orders 0, 1, 2).
 *
 * Modes add a direction bias and fusion rules on top:
 *   substrate  -- `spores` spores (default 4), each germinating both ways;
 *   polypore   -- the dimitic tissue of a bracket fungus: `spores` long,
 *                 straight, unbranched skeletal hyphae (default 10), then
 *                 thin branched binding hyphae (share `binding` of the
 *                 length) that start on them, reach for the nearest other
 *                 skeletal hypha and fuse to it;
 *   mat        -- two horizontal layers (z = 1/4, 3/4) of `spores` spores
 *                 each (default 3); a share `anchors` of the branches turns
 *                 into thick vertical anchors that grow to the other layer
 *                 and spread on in it;
 *   layered    -- substrate growth on a substrate that is rich in the mid
 *                 plane z = 1/2 and poor (`poor`) at z = 0: a graded cell.
 *
 * The result is a table of segments with thickness weights; the solid is
 * {min_e dist(x, e) / w_e <= c}. Coordinates are rounded to float, so the
 * published table rebuilds the cell bit for bit.
 */
namespace mycelium {

enum class Mode { Substrate, Polypore, Mat, Layered };

/// "substrate" | "polypore" | "mat" | "layered"; false for anything else.
bool parseMode(const char* text, Mode& out);
const char* modeKey(Mode m);

struct Params {
    Mode   mode    = Mode::Substrate;
    double length  = 22.0;   ///< total hypha length per cell, cell edges
    int    spores  = 0;      ///< 0 = the mode's default (see above)
    double unit    = 0.25;   ///< hyphal growth unit, cell edges
    double angle   = 60.0;   ///< branch angle, degrees
    double wiggle  = 0.25;   ///< persistent random walk of the tips
    double fuse    = 0.05;   ///< fusion distance, cell edges (0 = no fusions, a tree)
    double taper   = 0.75;   ///< thickness of a branch over its parent's
    double binding = 0.36;   ///< polypore: share of the length in binding hyphae
    double anchors = 0.25;   ///< mat: share of the branches that become anchors
    double poor    = 0.25;   ///< layered: substrate level at z = 0 (1 in the mid plane)
};

/// One straight piece of hypha: a in [0, 1)^3, b = a + its vector (not
/// wrapped), w the thickness weight (1 = the thickest hyphae).
struct Segment {
    float a[3] = { 0.f, 0.f, 0.f };
    float b[3] = { 0.f, 0.f, 0.f };
    float w = 1.f;
};

struct Stats {
    bool   grown      = false;   ///< false: read from a file (only length / fabric known)
    double length     = 0.0;     ///< total hypha length, cell edges
    int    hyphae     = 0;
    int    fusions    = 0;
    int    branches   = 0;
    int    freeEnds   = 0;       ///< tips that stopped without fusing
    double growthUnit = 0.0;     ///< length / number of tips that ever grew
    double backbone   = 0.0;     ///< share of the length on closed loops (2-core): dangling ends carry no load
    double fabric[3]  = { 0.0, 0.0, 0.0 };   ///< length-weighted <u_x^2>, <u_y^2>, <u_z^2>
};

/// Grows a colony from the generator; fills `stats` when given.
std::vector<Segment> grow(const Params& p, std::mt19937& rng, Stats* stats = nullptr);

/// Length and fabric tensor of a segment table (a file has no growth history).
Stats measure(const std::vector<Segment>& segments);

/// Grid step of the nodes along a hypha, cell edges.
constexpr double kStep = 0.02;

} // namespace mycelium

#endif // MYCELIUM_H

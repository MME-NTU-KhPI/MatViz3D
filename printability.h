#ifndef PRINTABILITY_H
#define PRINTABILITY_H

#include <cstdint>
#include <string>

/**
 * @brief Can this voxel structure be printed by FDM? Thresholds and verdict.
 *
 * The check works on any grain-id grid (id 0 = void, anything else = solid)
 * and is independent of Qt, so it is unit-testable on its own. Four
 * measures, all fractions of the solid volume unless stated:
 *
 *   - islands: 6-connected solid components that do not wrap the periodic
 *     cube (reach their own image across a face); a floating piece has
 *     nothing to be printed on. The verdict fails on any island larger than
 *     `max_island_vox` voxels, so only voxelization specks are tolerated.
 *     The same measure on the cube taken as a plain box is reported too:
 *     that is what hangs loose at the cut side faces of a finite part;
 *   - thin walls: solid removed by a morphological opening with a ball of
 *     radius min_wall/2, i.e. material that no ball of the minimum wall
 *     diameter fits into. Uses an exact Euclidean distance transform, so a
 *     wall is "thin" by its true local thickness, not by a voxel count;
 *   - overhangs: for each of the six build directions, solid voxels on a
 *     layer with no solid in the (2r+1)^2 window of the layer below, where
 *     r = round(tan(max_overhang)) (45 deg -> the 3x3 window). The first
 *     layer rests on the bed. The best direction is the one with the
 *     smallest unsupported fraction, and that is what the verdict uses: the
 *     orientation is ours to choose;
 *   - closed pores: void not in the largest void component (fraction of the
 *     whole cube). Reported, not a failure: FDM prints closed cells, and
 *     their inner overhangs are already counted above.
 *
 * `voxel_mm` converts the wall threshold into voxels. Below about two voxels
 * per minimum wall the opening cannot resolve anything; the report says so.
 */
struct PrintabilityThresholds {
    double voxel_mm                 = 0.25;  ///< edge of one voxel, mm
    double min_wall_mm              = 0.8;   ///< two nozzle diameters
    double max_overhang_deg         = 45.0;
    double max_thin_fraction        = 0.02;
    int    max_island_vox           = 8;     ///< largest floating piece tolerated, voxels (2x2x2 specks)
    double max_unsupported_fraction = 0.05;
    bool   periodic                 = true;
};

struct PrintabilityReport {
    bool   computed              = false;
    double solid_fraction        = 0.0;
    int    components            = 0;      ///< solid components (6-connected)
    int    islands               = 0;      ///< solid components that do not wrap (periodic) / touch a face
    double island_fraction       = 0.0;    ///< solid in islands, fraction of the solid
    std::size_t largest_island_vox = 0;
    double island_fraction_box   = 0.0;    ///< same measure on the cube cut out as a box: what hangs loose at a part's side faces
    int    void_components       = 0;
    double closed_pore_fraction  = 0.0;
    double thin_fraction         = 0.0;
    double opening_radius_vox    = 0.0;    ///< min_wall / 2 in voxels
    /// Rough mean wall thickness in voxels: 4 x the mean distance of solid
    /// voxels to the nearest void (exact for a slab). Lets families without an
    /// analytic estimate still report a thickness.
    double mean_thickness_vox    = 0.0;
    bool   thin_check_resolved   = true;   ///< false when the radius is below one voxel
    double unsupported[6]        = { 0, 0, 0, 0, 0, 0 };   ///< +X -X +Y -Y +Z -Z
    int    best_direction        = 4;      ///< index into unsupported[]; 4 = +Z
    double unsupported_best      = 0.0;
    bool   ok                    = false;
    std::string verdict;                   ///< one line, why it failed or "printable"

    static const char* directionName(int d);
};

/// Runs the check on voxels[x][y][z], a cube of edge N.
PrintabilityReport checkPrintability(int32_t*** voxels, int N, const PrintabilityThresholds& t);

#endif // PRINTABILITY_H

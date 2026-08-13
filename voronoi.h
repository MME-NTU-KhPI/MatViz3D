#ifndef VORONOI_H
#define VORONOI_H

#include <cstdint>
#include <vector>
#include "parent_algorithm.h"

/**
 * @brief Reference RVE generator: Voronoi tessellation under a Minkowski
 *        L_p metric.
 *
 * Every voxel is assigned to the seed that minimises
 *
 *     d_p(v, s) = ( |dx|^p + |dy|^p + |dz|^p )^(1/p)
 *
 * with the exponent p supplied by the user (Parameters::minkowski_p):
 * p = 1 gives octahedral grains, p = 2 the classical Euclidean Voronoi
 * tessellation, and large p approaches the Chebyshev metric and cuboidal
 * grains. On a periodic cell the offsets use the minimum-image convention, so
 * grains wrap across faces and the result is a valid homogenization cell.
 *
 * Unlike the cellular-automaton algorithms this is not an iterative process --
 * the tessellation is exact and computed in one pass. To stay usable with the
 * animation / GIF-recording path it is still *revealed* iteratively: the voxels
 * are sorted into distance bands and Next_Iteration() uncovers one band, which
 * looks like grains growing at a uniform rate and ends at the exact same
 * structure Generate_To_End() produces in a single shot.
 *
 * This is also the reference for the plugin-style registration: the name,
 * parameter schema and factory all live at the bottom of voronoi.cpp and
 * nothing outside this pair of files mentions the class.
 */
class Voronoi : public Parent_Algorithm
{
public:
    Voronoi();
    Voronoi(short int numCubes, int numColors);

    void Initialization(bool isWaveGeneration) override;
    void Next_Iteration() override;
    void Generate_To_End() override;
    bool getDone() const override;
    void CleanUp() override;

private:
    /// Uniform bucket grid over the seed points, in CSR form, so a voxel only
    /// tests the handful of seeds that can plausibly be its nearest.
    struct SeedGrid {
        int nb   = 1;                 ///< buckets per axis
        int cell = 1;                 ///< bucket edge, in voxels
        std::vector<int32_t> start;   ///< offsets into items, size nb^3 + 1
        std::vector<int32_t> items;   ///< seed indices, bucket-major
    };

    void   readParameters();
    void   buildPowTable();
    void   buildSeedGrid();

    /// |d|^p, from the lookup table when possible.
    inline double axisPow(int d) const;

    /// Grain id (1-based) of the seed nearest to (x,y,z); sum_p is the
    /// un-rooted |dx|^p + |dy|^p + |dz|^p, which orders identically to d_p.
    int32_t nearestSeed(int x, int y, int z, double& sum_p) const;

    /// Fills every voxel directly -- no auxiliary per-voxel storage.
    void tessellateDirect();

    /// Debug hook (MATVIZ_VORONOI_VERIFY=1): cross-checks the bucket search
    /// against a brute-force scan over all seeds.
    void verifyAgainstBruteForce() const;

    /// Same hook, after the last band: checks the animated reveal ends on the
    /// exact tessellation.
    void verifyRevealedGrid() const;

    /// Builds the label + reveal-order arrays the animated path walks.
    void prepareWaves();

    SeedGrid            m_grid;
    std::vector<double> m_pow;        ///< m_pow[d] == d^p for integer offsets

    double m_p        = 2.0;
    bool   m_periodic = false;

    // Animated reveal state (allocated only when Next_Iteration() is used).
    std::vector<int32_t>  m_label;    ///< nearest-seed id per voxel
    std::vector<uint32_t> m_order;    ///< voxel indices, grouped by distance band
    std::vector<uint32_t> m_bandEnd;  ///< one past the last m_order entry per band
    size_t m_cursor  = 0;
    size_t m_band    = 0;
    bool   m_prepared = false;
};

#endif // VORONOI_H

#ifndef VORONOI_H
#define VORONOI_H

#include <cstdint>
#include <vector>
#include "parent_algorithm.h"

/**
 * @brief Reference RVE generator: Voronoi tessellation under a Minkowski
 *        L_p metric and 3D Riemannian metric tensor.
 *
 * Every voxel is assigned to the seed that minimises
 *
 *     d(v, s) = ( sum_k |xi_k|^p )^(1/p),   xi = A * (v - s)
 *
 * where M = A^T * A is a symmetric positive-definite 3x3 metric tensor
 * (Parameters::voronoi_mxx .. voronoi_mxz) and p is the Minkowski exponent
 * (Parameters::minkowski_p). For the standard Euclidean case (p = 2, M = I),
 * this reproduces classical Voronoi tessellation. A diagonal metric tensor
 * scales individual axes to produce elongated/columnar grains (e.g. rolled
 * or extruded microstructures), while off-diagonal components rotate the
 * principal growth directions arbitrarily.
 * On a periodic cell the offsets use the minimum-image convention, so
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

    /// Symmetric 3x3 metric tensor M for anisotropic Voronoi tessellation.
    struct MetricTensor {
        double xx = 1.0, yy = 1.0, zz = 1.0;
        double xy = 0.0, yz = 0.0, xz = 0.0;
    };

    void   readParameters();
    void   buildPowTable();
    void   buildSeedGrid();

    /// |d|^p, from the lookup table when possible.
    inline double axisPow(int d) const;

    /// Evaluates metric distance s_p for displacement (dx, dy, dz)
    inline double distanceSp(int dx, int dy, int dz) const;

    /// Grain id (1-based) of the seed nearest to (x,y,z); sum_p is the
    /// un-rooted metric distance, which orders identically to d.
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
    std::vector<double> m_powX;       ///< m_pow[d] * Mxx^(p/2) for diagonal metric
    std::vector<double> m_powY;       ///< m_pow[d] * Myy^(p/2) for diagonal metric
    std::vector<double> m_powZ;       ///< m_pow[d] * Mzz^(p/2) for diagonal metric

    MetricTensor m_metric;
    double m_A[3][3] = {{1,0,0},{0,1,0},{0,0,1}}; ///< Coordinate transformation A such that A^T A = M
    double m_boundScale = 1.0;        ///< Scale factor for the expanding-ring pruning bound
    bool   m_isDiagonal = true;       ///< True if off-diagonals are zero
    bool   m_isIsotropic = true;      ///< True if M is identity

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

#ifndef LATTICE_H
#define LATTICE_H

#include <cstdint>
#include <vector>
#include "parent_algorithm.h"
#include "latticefamily.h"

/**
 * @brief Periodic cellular metamaterial RVE: one solid phase and void.
 *
 * The cube is tiled with `cells_per_edge` unit cells of one family
 * (latticefamily.h) and every voxel is either solid (grain id 1) or void
 * (id 0, which both stress solvers already treat as a pore). The solid is the
 * sublevel set {F(x) <= c} of a scalar field F that depends on the family:
 *
 *   - TPMS (gyroid, Schwarz P, Schwarz D, IWP): the trigonometric level-set
 *     approximation f of the minimal surface. *Sheet* variants thicken the
 *     surface itself, F = |f|; *skeletal* variants fill one of its two
 *     labyrinths, F = f.
 *   - Strut lattices (simple cubic, BCC, FCC, octet): F = distance to the
 *     nearest strut centre-line, so c is the strut radius in cell units.
 *   - Honeycombs (hexagonal, re-entrant): F = in-plane distance to the
 *     nearest wall centre-line, extruded along Z; c is half the wall thickness.
 *     A hexagonal tiling is not periodic on a square, so the number of cells
 *     along Y is rounded and the cells stretched to fit (reported).
 *   - Spinodoid: a Gaussian random field built from waves on the integer
 *     lattice (periodic in the cube by construction), seeded by
 *     Parameters::seed. F = the field, c its level. The wave directions can
 *     be confined to cones about the axes (`spinodoid_theta_x/y/z`, Kumar
 *     et al. 2020): all 90 deg = isotropic, one cone = lamellar, two =
 *     columnar, so the statistics, not a name, define the geometry.
 *   - Smoothed noise: the most random printable thing. White noise per
 *     voxel (seeded), blurred with a periodic Gaussian of sigma
 *     `noise_smooth` cube edges, normalized to unit variance. Skeletal
 *     (F = field, blobs) and sheet (F = |field|, walls around the zero
 *     level). The correlation length, and with it the wall thickness, is
 *     set by the smoothing; cells_per_edge plays no role.
 *   - Fourier series: an open family. F is a sum of `fourier_terms` cosine
 *     waves on the integer lattice with amplitudes ~ |k|^-decay in the band
 *     [kmin, kmax] waves per edge, drawn from the seed -- or read from a
 *     text file (`fourier_file`: kx ky kz amplitude phase per line), which
 *     is how a property-to-geometry network hands over a design. Sheet
 *     (|F| <= c) and skeletal (F <= c) variants. The waves actually used
 *     are published to Parameters::lattice_wave_terms and the HDF5 file.
 *
 * ## Relative density is the input
 *
 * The threshold c is solved from `relative_density`: F is sampled on a coarse
 * copy of the grid (<= 64^3), sorted, and c is read off at the requested
 * quantile -- exact on that grid, no bisection. The structure is then
 * rasterized at full resolution with that c and the achieved density is
 * reported (and published in Parameters::lattice_achieved_density for the
 * HDF5 metadata). Wherever the family allows it, the implied wall or strut
 * thickness is estimated in voxels and a warning is logged below 6 voxels,
 * where the voxel staircase starts to distort the homogenized stiffness.
 *
 * ## Imperfections and grading
 *
 *   - `lattice_grading` g: linear thickness gradient along Z, from (1-g) at
 *     the bottom face to (1+g) at the top -- a functionally graded cell.
 *   - `lattice_jitter` j: a smooth random thickness modulation of relative
 *     amplitude j (sum of low-frequency waves, seeded), standing in for the
 *     wall-thickness scatter of a printed part.
 *   - `orientation_angle_a/b/c`: rotation of the infinite lattice about the
 *     cube centre (Z, X, Z', degrees). Note that a rotated cut is no longer
 *     periodic across the cube faces; the FFT solver, being periodic, then
 *     sees a mismatch at the faces. Leave at zero for homogenization and use
 *     the material orientation for print-direction anisotropy instead.
 *
 * ## Phases
 *
 * The solid is one grain (id 1) made of `db_material`, published as a
 * single-entry PhaseAssignment with the identity orientation, so an
 * anisotropic (printed) material keeps its full 6x6 stiffness in both
 * solvers. `Next_Iteration()` reveals the structure layer by layer along Z,
 * the way a printer would, and ends on exactly what `Generate_To_End()`
 * produces in one shot.
 */
class Lattice : public Parent_Algorithm
{
public:
    Lattice();
    Lattice(short int numCubes, int numColors);

    void Initialization(bool isWaveGeneration) override;
    void Next_Iteration() override;
    void Generate_To_End() override;
    bool getDone() const override;
    void CleanUp() override;

    /// Grain id of the solid phase; void voxels keep id 0.
    static constexpr int32_t SolidId = 1;

    // Results of the last Initialization()/rasterization, for logging, tests
    // and the HDF5 geometry metadata.
    LatticeFamily family() const         { return m_family; }
    double achievedDensity() const       { return m_achieved; }
    double threshold() const             { return m_c; }
    /// Estimated wall / strut thickness in voxels, 0 when the family has no
    /// simple estimate (skeletal TPMS, spinodoid).
    double wallVoxels() const            { return m_wallVox; }
    /// Honeycombs: factor the cells were stretched along Y to fit the cube.
    double honeycombStretch() const      { return m_hcStretch; }
    int    cellsAlongY() const           { return m_cellsY; }

    /**
     * @brief The field whose sublevel set is the solid, at a point given in
     *        voxel units (a voxel centre is at i + 0.5). Includes rotation,
     *        grading and jitter. Exposed so a test can probe the geometry
     *        without rasterizing a whole cube.
     */
    double fieldAt(double x, double y, double z) const;

private:
    struct Segment { double ax, ay, az, bx, by, bz; };
    struct Wave    { double kx, ky, kz, phase, amp; };

    void   readParameters();
    void   buildRotation();
    void   buildSegments();
    void   buildWaves();
    void   buildNoise();
    bool   loadFourierFile(const QString& path);
    void   publishWaves();
    /// Family field at a point in cube fractions [0,1)^3 (already rotated).
    double rawField(double X, double Y, double Z) const;
    /// Level-set value f and its gradient (per cube fraction) for the
    /// surface families; false for families that are not level sets.
    bool   levelSet(double X, double Y, double Z, double& f, double g[3]) const;
    bool   isSheet() const;
    /// Unnormalized thickness-jitter field at a point in cube fractions.
    double jitterField(double X, double Y, double Z) const;
    /// Grading + jitter at a point in cube fractions (not rotated).
    double modulation(double X, double Y, double Z) const;
    double solveThreshold();
    /// Fills voxels[x][y][z] for z0 <= z < z1; returns the solid count written.
    long long rasterize(double c, int z0, int z1);
    void   estimateWall();
    void   publishPhases();
    void   locateSeed();
    void   finish();
    void   checkPrintable();
    void   report() const;
    void   reportAchieved() const;

    // Resolved inputs.
    LatticeFamily m_family   = LatticeFamily::GyroidSheet;
    bool    m_signed         = false;
    bool    m_uniformWall    = false;  ///< sheets: |f|/|grad f| instead of |f|
    int     m_cells          = 2;
    double  m_target         = 0.30;
    double  m_grading        = 0.0;
    double  m_jitter         = 0.0;
    bool    m_periodic       = true;
    QString m_material;
    double  m_angles[3]      = { 0.0, 0.0, 0.0 };   ///< radians
    double  m_theta[3]       = { 90.0, 90.0, 90.0 }; ///< spinodoid cone half-angles about X, Y, Z, degrees (0 = off)
    int     m_fourierTerms   = 24;
    double  m_fourierKmin    = 1.0;    ///< waves per cube edge
    double  m_fourierKmax    = 4.0;
    double  m_fourierDecay   = 1.0;    ///< amplitude ~ |k|^-decay
    QString m_fourierFile;
    size_t  m_directions     = 0;      ///< distinct wave directions available to the random families
    double  m_R[3][3]        = { {1,0,0}, {0,1,0}, {0,0,1} };
    bool    m_rotated        = false;

    // Family geometry.
    std::vector<Segment> m_segments;   ///< strut / wall centre-lines, one cell
    bool    m_segments2D     = false;  ///< honeycombs: walls extruded along Z
    double  m_hcW            = 1.0;    ///< honeycomb cell width, wall-length units
    double  m_hcP            = 1.0;    ///< honeycomb cell height (period along Y)
    int     m_cellsY         = 1;      ///< honeycomb: cells along Y
    double  m_hcStretch      = 1.0;
    std::vector<Wave> m_waves;         ///< spinodoid random field
    std::vector<Wave> m_jitterWaves;   ///< thickness-modulation field
    double  m_jitterNorm     = 1.0;
    std::vector<float> m_noise;        ///< smoothed-noise families: field on the full grid, unit variance
    double  m_noiseSmooth    = 0.06;   ///< Gaussian sigma as a fraction of the cube edge
    double  m_noiseSigmaVox  = 0.0;

    // Solved / measured.
    double  m_c              = 0.0;
    double  m_achieved       = 0.0;
    double  m_wallVox        = 0.0;
    int     m_coarseN        = 1;
    long long m_solidCount   = 0;
    int     m_zFilled        = 0;

    // Layer-by-layer reveal for the animated path.
    int     m_step           = 0;
    int     m_steps          = 1;
};

#endif // LATTICE_H

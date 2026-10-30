#ifndef LATTICE_H
#define LATTICE_H

#include <array>
#include <cstdint>
#include <vector>
#include "parent_algorithm.h"
#include "latticefamily.h"
#include "mycelium.h"

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
 *     The presets (+30 deg / rib 1, -30 deg / rib 2) can be overridden by
 *     `honeycomb_angle` (inclined wall to the horizontal, negative folds it
 *     inward) and `honeycomb_rib` (vertical rib length over wall length):
 *     0 deg gives brick-like rectangles, a large rib stretches the cells.
 *     A hexagonal tiling is not periodic on a square, so the number of cells
 *     along Y is rounded and the cells stretched to fit (reported).
 *   - Spinodoid: a Gaussian random field built from waves on the integer
 *     lattice (periodic in the cube by construction), seeded by
 *     Parameters::seed. F = the field, c its level. The wave directions can
 *     be confined to cones about the axes (`spinodoid_theta_x/y/z`, Kumar
 *     et al. 2020): all 90 deg = isotropic, one cone = lamellar, two =
 *     columnar, so the statistics, not a name, define the geometry.
 *   - Smoothed noise: the most random printable thing. White noise on a
 *     fixed 64^3 grid (seeded), blurred with a periodic Gaussian of sigma
 *     `noise_smooth` cube edges, normalized to unit variance, and read
 *     between its nodes by periodic cubic interpolation -- so one seed is
 *     one foam on any cube size, and a 64^3 cube reads the nodes. Skeletal
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
 *     With `fourier_symmetry = cubic` every drawn wave is replaced by its
 *     orbit under the 48 symmetries of the cube (same amplitude and phase),
 *     so the random cell has full cubic symmetry and Ex = Ey = Ez.
 *   - Plate lattices (Tancogne-Dejean et al. 2018, Berger et al. 2017):
 *     F = distance to the nearest plate mid-plane of the chosen plane
 *     families, {100} (SC), {110} (BCC), {111} (FCC) or their unions, so c
 *     is half the plate thickness in cell units. `plate_hole` punches a
 *     circular hole of that diameter (cell edges) through the centre of
 *     every panel, along its normal.
 *   - `lattice_union`: a second family whose solid is united with the
 *     first (F = min of the two fields, one shared level c), e.g. plates +
 *     octet struts. Both fields are distances for the strut, plate,
 *     honeycomb and uniform-wall sheet families, so c then means the same
 *     thickness for both. The second family cannot be a random one.
 *   - `lattice_subtract` + `lattice_subtract_size` s: the sublevel set
 *     {F2 <= s} of a third family is carved out of the solid (F = +inf
 *     there, so the level is solved on what remains): plates minus spheres
 *     at the BCC sites give spherical windows, minus `cylinders_axes` give
 *     round channels through the face centres.
 *   - Site primitives: `spheres_sc/bcc/fcc` (F = distance to the nearest
 *     lattice site, c the sphere radius) and `cylinders_axes` (distance to
 *     the three axis lines through the face centres). Meant as the second
 *     body of a union or subtraction; alone, spheres below the touching
 *     radius are separate bodies.
 *   - Cubic assembly (`cubic_assembly`): free-form cells with full cubic
 *     symmetry, the kind topology optimization and generative models produce.
 *     Members are drawn from the seed in the asymmetric unit of the cubic
 *     cell, the orthoscheme 0 <= z <= y <= x <= 1/2 with vertices at a cube
 *     corner, an edge midpoint, a face centre and the cell centre:
 *     `assembly_struts` segments, `assembly_plates` triangles,
 *     `assembly_shells` spherical shells about the special positions (corner,
 *     cell centre, face centre, edge midpoint) and `assembly_holes` spherical
 *     windows cut through the plates and shells (struts pass through them,
 *     as with `plate_hole`). Every point of the cell is folded into
 *     that unit (x -> min(x, 1 - x) per axis, then sorted), so the members are
 *     repeated by all 48 symmetries of the cube and the lattice translations.
 *     Pm-3m is generated by the reflections in the walls of that unit, so for
 *     members inside it the distance in the folded frame is the distance to
 *     the whole orbit (shells, which reach outside the unit, carry their
 *     image centres). The first member spans a wall or an edge of the unit
 *     (a periodic backbone: SC / BCC / FCC struts or {100} / {110} plates)
 *     and every later one starts on a point of an earlier one, so the solid is
 *     connected by construction (until windows cut it). F = distance to the
 *     nearest member divided by its thickness weight (`assembly_spread`), so
 *     c is half the thickness of a weight-1 member. The members are published
 *     to Parameters::lattice_assembly_terms and the HDF5 file, and
 *     `assembly_file` reads them back (one member per line).
 *   - Foams, the 3D honeycombs (`foam_kelvin`, `foam_rhombic`,
 *     `foam_weaire_phelan`, `foam_random`): the cell is the Voronoi partition
 *     of a site set in the unit cell -- BCC sites (Kelvin's truncated
 *     octahedra), FCC sites (rhombic dodecahedra), A15 sites (Weaire-Phelan)
 *     or `foam_bubbles` seeded random sites kept apart by a hard core (or read
 *     from `foam_file`). Every Voronoi cell is cut out of a box by the
 *     bisector planes of its candidate neighbours, so its true faces and
 *     edges are known. `foam_mode` walls: F = distance to the cell boundary
 *     (exact: the nearest wall of a point is a face of its own cell), c half
 *     the wall thickness, closed cells; windows: the same wall, but reaching
 *     only c + `foam_frame` bubble sizes from the face's edges (in-plane), so
 *     every face has a window of its own shape that narrows as the wall
 *     thickens; edges: F = distance to the nearest edge
 *     of the cell, c the strut radius (an open-cell foam). The sites go to
 *     Parameters::lattice_foam_sites and the HDF5 file.
 *   - Mycelium (`mycelium`): a fungal colony grown in the unit cell from the
 *     seed (mycelium.h) -- hyphae forage on a nutrient substrate, branch every
 *     `mycelium_unit` of length, fuse with hyphae they come close to, and every
 *     branch order is `mycelium_taper` times thinner. `mycelium_mode` substrate,
 *     polypore (straight skeletal hyphae tied by branched binding hyphae), mat
 *     (two layers joined by vertical anchors) or layered (rich in the mid plane,
 *     poor at z = 0). The colony is a table of straight pieces with thickness
 *     weights; F = distance to the nearest piece / its weight (periodic), so c is
 *     the radius of a weight-1 hypha, evaluated through a periodic bucket grid
 *     up to a cut-off that is widened when the solved level reaches it. The
 *     pieces go to Parameters::lattice_mycelium_segments and the HDF5 file, and
 *     `mycelium_file` reads them back (one piece per line).
 *   - `lattice_smooth` sigma (cell edges): the full field (after grading and
 *     jitter) is sampled on the grid, values of carved-out voxels are set to
 *     the largest finite value, and a periodic Gaussian blur of sigma is
 *     applied before the level is solved. Sharp corners and junctions come
 *     out rounded, like fillets; walls thinner than ~2 sigma vanish.
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
    /// One member of a cubic assembly, in the asymmetric unit 0 <= z <= y <= x <= 1/2.
    /// One face of a foam cell in the site's frame: outward unit normal, distance
    /// of its plane from the site, and its edges (segments a -> b).
    struct FoamFace {
        double n[3] = { 0.0, 0.0, 0.0 };
        double half = 0.0;
        int    nb = -1;                            ///< site across the face ...
        double off[3] = { 0.0, 0.0, 0.0 };         ///< ... at this offset from the cell's site
        std::vector<std::array<double,6>> edges;
    };
    struct FoamCell {
        std::vector<FoamFace> faces;
        std::vector<std::array<double,6>> edges;   ///< every edge of the cell once
        /// The cell's edges plus the neighbours' edges that leave its vertices:
        /// near a vertex the nearest strut can belong to a neighbour only.
        std::vector<std::array<double,6>> struts;
    };
    struct Member {
        enum Kind { Strut = 0, Plate = 1, Shell = 2, Hole = 3 };
        int    kind = Strut;
        double p[3][3] = {};    ///< strut: p0-p1; plate: triangle p0 p1 p2; shell / hole: centre p0
        double r = 0.0;         ///< shell / hole radius, cell edges
        double w = 1.0;         ///< thickness weight of a solid member (F = distance / w)
        int    host = -1;       ///< window: index of the plate / shell it cuts (-1 = all of them)
        std::vector<std::array<double,3>> centres;   ///< shell: images of the centre near the unit
        std::vector<std::array<double,4>> cuts;      ///< plate / shell: its windows (centre, radius)
    };

    void   readParameters();
    void   buildRotation();
    void   buildSegments();
    void   buildWaves();
    void   buildNoise();
    /// Smoothed noise at a point in cube fractions: periodic Catmull-Rom
    /// interpolation of the kNoiseGrid^3 field (node i at (i + 1/2) / kNoiseGrid,
    /// latticefamily.h).
    double noiseAt(double X, double Y, double Z) const;
    bool   loadFourierFile(const QString& path);
    void   publishWaves();
    /// Cubic assembly: draw the members from the seed (or read `assembly_file`) and publish them.
    void   buildAssembly();
    bool   loadAssemblyFile(const QString& path);
    void   finishMember(Member& m) const;
    /// Hand every window to the plate / shell it cuts (Member::cuts).
    void   attachCuts();
    /// Foams: the site set, its periodic images and the faces / edges of every Voronoi cell.
    void   buildFoam();
    bool   loadFoamFile(const QString& path);
    /// Foam field at a point in cube fractions: nearest site, then its cell's walls,
    /// windowed walls or edges (`foam_mode`).
    double foamField(double X, double Y, double Z) const;
    /// Mycelium: grow the colony from the seed (or read `mycelium_file`) and publish it.
    void   buildMycelium();
    bool   loadMyceliumFile(const QString& path);
    /// Bucket grid over the pieces for distances below m_mycCut.
    void   buildMyceliumGrid();
    /// Mycelium field at a point in cube fractions: distance to the nearest
    /// piece / its weight, capped at m_mycCut.
    double myceliumField(double X, double Y, double Z) const;
    /// Cubic assembly field at a point in cube fractions: fold into the asymmetric
    /// unit, distance to the nearest member / its weight, +inf inside a window.
    double assemblyField(double X, double Y, double Z) const;
    /// Exact Fourier expansion of the TPMS level sets (k per cube edge), for publishing.
    std::vector<Wave> tpmsWaves() const;
    /// Family field at a point in cube fractions [0,1)^3 (already rotated):
    /// the primary family, united with `lattice_union` when one is set.
    double rawField(double X, double Y, double Z) const;
    /// Field of one family (uniform-wall option applied to sheets).
    double familyField(LatticeFamily fam, double X, double Y, double Z) const;
    /// Plate lattices: distance to the nearest plate mid-plane, +inf inside a hole.
    double plateField(LatticeFamily fam, double X, double Y, double Z) const;
    /// Spheres at lattice sites / cylinders along the axes: distance to the site set.
    double siteField(LatticeFamily fam, double X, double Y, double Z) const;
    /// Jitter normalisation (max |jitter field| on the coarse grid).
    void   normalizeJitter();
    /// `lattice_smooth`: sample, clamp and blur the full field into m_smoothed.
    void   buildSmoothed();
    /// Periodic separable Gaussian blur of a N^3 grid in place (three 1-D passes).
    static void periodicBlur(std::vector<float>& a, int N, double sigmaVox);
    /// Replace every wave by its orbit under the cubic point group.
    void   symmetrizeWaves();
    /// Level-set value f and its gradient (per cube fraction) for the
    /// surface families; false for families that are not level sets.
    bool   levelSet(LatticeFamily fam, double X, double Y, double Z, double& f, double g[3]) const;
    bool   levelSet(double X, double Y, double Z, double& f, double g[3]) const { return levelSet(m_family, X, Y, Z, f, g); }
    bool   isSheet(LatticeFamily fam) const;
    bool   isSheet() const { return isSheet(m_family); }
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
    bool    m_cubicSym       = false;  ///< Fourier families: symmetrize the waves under the cubic group
    double  m_plateHole      = 0.0;    ///< plate lattices: hole diameter, cell edges (0 = none)
    bool    m_hasUnion       = false;
    LatticeFamily m_union    = LatticeFamily::GyroidSheet;   ///< second family united with the first
    bool    m_hasSubtract    = false;
    LatticeFamily m_subtract = LatticeFamily::SpheresBCC;    ///< family carved out of the solid
    double  m_subtractSize   = 0.3;    ///< its level, cell units (sphere radius, channel radius, ...)
    double  m_smooth         = 0.0;    ///< Gaussian sigma of the field blur, cell edges (0 = off)
    int     m_asmStruts      = 3;      ///< cubic assembly: members drawn in the asymmetric unit
    int     m_asmPlates      = 2;
    int     m_asmShells      = 0;
    int     m_asmHoles       = 1;
    double  m_asmSpread      = 0.3;    ///< thickness weights drawn in [1 - spread, 1 + spread]
    QString m_asmFile;
    std::vector<Member> m_members;
    double  m_asmMinWeight   = 1.0;    ///< thinnest solid member, for the wall estimate
    QString m_foamMode       = QStringLiteral("walls");   ///< walls | windows | edges
    double  m_foamFrame      = 0.1;    ///< windows: frame beyond the half wall, bubble sizes
    int     m_foamBubbles    = 8;      ///< random foam: sites per cell
    QString m_foamFile;
    bool    m_hasFoam        = false;
    LatticeFamily m_foamFamily = LatticeFamily::FoamKelvin;  ///< the foam in use (primary, union or carve-out)
    double  m_foamBubble     = 1.0;    ///< bubble size (cell volume per site)^(1/3), cell edges
    std::vector<std::array<double,3>> m_foamSites;    ///< sites in the unit cell, cell units
    std::vector<std::array<double,3>> m_foamImages;   ///< sites + their 26 neighbour-cell images
    std::vector<int>      m_foamImageSite;            ///< site index of every image
    std::vector<FoamCell> m_foamCells;                ///< Voronoi cell of every site
    mycelium::Params m_myc;                           ///< growth settings
    QString m_mycFile;
    std::vector<mycelium::Segment> m_mycSegments;     ///< the hyphae, one cell
    mycelium::Stats m_mycStats;
    double  m_mycMinWeight   = 1.0;    ///< thinnest hypha, for the wall estimate
    double  m_mycCut         = 0.2;    ///< field values are exact below this, cell edges
    int     m_mycG           = 1;      ///< buckets per cell edge
    std::vector<int> m_mycStart;       ///< bucket b holds pieces m_mycIndex[m_mycStart[b] .. m_mycStart[b + 1])
    std::vector<int> m_mycIndex;
    std::vector<float> m_smoothed;     ///< blurred full field when m_smooth > 0
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
    bool    m_hcCustom       = false;  ///< honeycomb shape from honeycomb_angle / honeycomb_rib instead of the family preset
    double  m_hcAngle        = 30.0;   ///< inclined wall angle to the horizontal, degrees (< 0 = re-entrant)
    double  m_hcRib          = 1.0;    ///< vertical rib length / inclined wall length
    std::vector<Wave> m_waves;         ///< spinodoid random field
    std::vector<Wave> m_jitterWaves;   ///< thickness-modulation field
    double  m_jitterNorm     = 1.0;
    std::vector<float> m_noise;        ///< smoothed-noise families: field on the kNoiseGrid^3 grid, unit variance
    double  m_noiseSmooth    = 0.06;   ///< Gaussian sigma as a fraction of the cube edge
    double  m_noiseSigmaVox  = 0.0;    ///< the same in voxels of the noise grid

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

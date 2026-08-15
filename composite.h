#ifndef COMPOSITE_H
#define COMPOSITE_H

#include <cstdint>
#include <vector>
#include "parent_algorithm.h"

/**
 * @brief Fiber-reinforced composite RVE with controllable imperfections.
 *
 * The cell is filled with a matrix phase and one, two or three orthogonal
 * families of continuous fibers:
 *
 *   1D -- fibers along Z                 (unidirectional lamina)
 *   2D -- fibers along X and Y           (0/90 crossed layup)
 *   3D -- fibers along X, Y and Z        (orthogonal 3D weave)
 *
 * A fiber is a straight cylinder of elliptical cross-section, so within one
 * family everything is decided in the 2D plane normal to that family's axis:
 * the family owns an nU x nV lattice of centers (rectangular, or row-staggered
 * for hexagonal packing) and the volume is just that plane extruded. That is
 * what keeps the cost at O(N^2) per family plus one O(N^3) pass to combine
 * them, instead of testing every voxel against every fiber.
 *
 * ## Volume fraction is the input, not an outcome
 *
 * The user gives a target Vf and the number of fibers per row; the fiber
 * semi-axes are then *solved* for (bisection against the volume fraction
 * actually measured on a sub-sampled copy of the grid) so the structure hits
 * the requested concentration. For 2D/3D that matters: the families intersect,
 * so the naive "sum of the families' areas" over-counts and would land well
 * above the target.
 *
 * Packing puts a hard ceiling on Vf. Unless overlap is explicitly allowed the
 * target is clamped to the largest fraction the current lattice, aspect ratio
 * and packing can reach without fibers touching, and the clamp is reported.
 *
 * ## Imperfections
 *
 * All three are driven off Parameters::seed, so a given seed reproduces the
 * same imperfect cell exactly:
 *
 *  - **center jitter** -- each center displaced by a uniform random vector of
 *    up to `jitter` half-pitches. Unless overlap is allowed the displacement is
 *    rejection-sampled so the fibers of a family still do not touch.
 *  - **ellipticity a/b** -- circular at 1. The area is held at pi*a*b = pi*r^2,
 *    so changing a/b at a fixed Vf changes only the shape.
 *  - **angle scatter** -- per-fiber in-plane rotation of the ellipse, drawn
 *    uniformly from +/- scatter/2 degrees (180 = fully random orientation).
 *
 * When the ellipses are all aligned (zero scatter) the fiber lattice is
 * stretched by the same a/b, which keeps the no-overlap ceiling at the circular
 * value (pi/4 square, ~0.907 hexagonal) for any aspect ratio -- without that,
 * an ellipticity sweep at fixed Vf would run into the packing limit almost
 * immediately. Once the fibers can rotate the stretch buys nothing (a rotated
 * ellipse needs room for its major axis in every direction) and the lattice
 * stays isotropic.
 */
class Composite : public Parent_Algorithm
{
public:
    Composite();
    Composite(short int numCubes, int numColors);

    void Initialization(bool isWaveGeneration) override;
    void Next_Iteration() override;
    void Generate_To_End() override;
    bool getDone() const override;
    void CleanUp() override;

    /// Grain id of the matrix phase. Fibers take ids from MatrixId + 1 up.
    static constexpr int32_t MatrixId = 1;

private:
    struct Fiber {
        double  cu = 0.0, cv = 0.0;   ///< center in the family's (u,v) plane
        double  cosT = 1.0, sinT = 0.0; ///< ellipse major-axis direction
        int32_t id = 0;
        bool    placed = false;       ///< already positioned (jitter rejection)
    };

    /// Grain id of the matrix, and the constituent index each phase takes in
    /// Parameters::phaseAssignment::materials.
    enum Phase { MatrixPhase = 0, FiberPhase = 1 };

    /// One set of parallel fibers, all running along `axis`.
    struct Family {
        int axis = 2;                 ///< 0 = X, 1 = Y, 2 = Z
        int nU = 1, nV = 1;           ///< centers per row / number of rows
        double pitchU = 1.0, pitchV = 1.0;
        /// Center offsets divided by the semi-axis they act against; the
        /// fibers touch when the resulting normalised distance reaches 2.
        double sepU = 1.0, sepV = 1.0;
        std::vector<Fiber> fibers;    ///< row-major, nV rows of nU
    };

    void readParameters();
    void buildFamilies();
    void placeFamily(Family& fam, double r);
    void assignIds();

    /// Fills Parameters::phaseAssignment: which constituent every grain is and,
    /// for the fibers, the orientation their geometry implies.
    void publishPhases();

    /// Analytic first guess for the fiber radius from the target Vf, treating
    /// the families as statistically independent.
    double nominalRadius() const;

    /// Largest radius at which no two fibers of a family touch.
    double packingLimitRadius() const;

    /// Bisects the radius so the measured volume fraction hits the target,
    /// clamping to the packing limit when overlap is not allowed.
    double solveRadius();

    /// Volume fraction on a sub-sampled copy of the grid (cheap enough to call
    /// ~30 times inside the bisection).
    double measureVolumeFraction(double r) const;

    /// Fills voxels[][][] at radius r and records the volume fraction reached.
    void rasterize(double r);

    /// Fiber id covering (u, v) in this family's plane, or 0 for matrix.
    /// `rho` receives the normalised elliptical radius of the winning fiber
    /// (0 on its axis, 1 on its surface), or kOutside when none covers the
    /// point -- that is what lets crossings be settled by proximity.
    int32_t fiberAt(const Family& fam, double u, double v, double r,
                    float& rho) const;

    /// Rasterizes one family's cross-section into an n x n id plane, with the
    /// matching normalised radii.
    void buildIdPlane(const Family& fam, double r, int n,
                      std::vector<int32_t>& plane,
                      std::vector<float>& rho) const;

    /// Sentinel radius for "no fiber of this family covers the point".
    static constexpr float kOutside = 2.0f;

    /// True if a center at (cu, cv) would touch an already-placed neighbour.
    bool overlapsPlaced(const Family& fam, int i, int j,
                        double cu, double cv, double r) const;

    void reportLayout(double r) const;
    void reportVolumeFraction() const;

    std::vector<Family> m_families;

    // Resolved inputs.
    int    m_dim          = 1;
    bool   m_hex          = false;
    bool   m_periodic     = false;
    bool   m_allowOverlap = false;
    int    m_fibersPerRow = 3;
    QString m_matrixMaterial;
    QString m_fiberMaterial;
    double m_targetVf     = 0.40;
    double m_aspect       = 1.0;   ///< a/b
    double m_sqrtK        = 1.0;   ///< sqrt(a/b); a = r*m_sqrtK, b = r/m_sqrtK
    double m_jitter       = 0.0;
    double m_angleScatter = 0.0;   ///< degrees, full width

    // Solved / measured.
    double m_radius     = 0.0;     ///< equivalent-area radius, sqrt(a*b)
    double m_achievedVf = 0.0;
    bool   m_clamped    = false;
    int    m_jitterFallbacks = 0;  ///< centers that had to stay on the lattice

    /// Grid resolution used inside the bisection.
    int m_coarseN = 1;

    // Animated reveal: the fibers grow from their axes to full size.
    int m_step  = 0;
    int m_steps = 1;
};

#endif // COMPOSITE_H

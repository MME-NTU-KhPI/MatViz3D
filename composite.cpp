#include "composite.h"
#include "algorithmplugin.h"
#include "parameters.h"
#include "openglwidgetqml.h"
#include "matviz_homog.hpp"   // mvh::Mat3, mvh::bunge_from_matrix

#include <QDebug>
#include <QString>
#include <algorithm>
#include <cmath>
#include <limits>
#include <omp.h>

namespace {



// The (u, v) axes of the cross-section a fiber running along `axis` lives in:
//   X -> (y, z),   Y -> (z, x),   Z -> (x, y)
// Cyclic, so the three families are treated identically.
inline void planeIndex(int axis, int x, int y, int z, int& iu, int& iv)
{
    switch (axis) {
        case 0:  iu = y; iv = z; break;
        case 1:  iu = z; iv = x; break;
        default: iu = x; iv = y; break;
    }
}

inline const char* axisName(int axis)
{
    return axis == 0 ? "X" : (axis == 1 ? "Y" : "Z");
}

// Material frame of one fiber, as Bunge ZXZ Euler angles.
//
// Axis 3 of the material -- the distinguished axis of a transversely isotropic
// row like C-fiber, and <100> for a cubic one -- runs along the fiber; axis 1
// runs along the ellipse major axis, so the per-fiber angle scatter rotates the
// stiffness with the shape rather than leaving the two inconsistent; axis 2
// completes the right-handed set.
//
// The rows of a Bunge orientation matrix are exactly the material axes written
// in sample coordinates, so the matrix can be filled in directly from those
// three vectors and converted. (Spot check: family Z comes back as
// (theta, 0, 0), family X as (pi/2, pi/2, theta).)
std::array<double,3> fiberOrientation(int axis, double cosT, double sinT)
{
    std::array<double,3> e3{}, e1{};
    switch (axis) {
        case 0:  e3 = {1,0,0}; e1 = {0.0,  cosT, sinT}; break;   // (u,v) = (y,z)
        case 1:  e3 = {0,1,0}; e1 = {sinT, 0.0,  cosT}; break;   // (u,v) = (z,x)
        default: e3 = {0,0,1}; e1 = {cosT, sinT, 0.0 }; break;   // (u,v) = (x,y)
    }

    const std::array<double,3> e2 = { e3[1]*e1[2] - e3[2]*e1[1],
                                      e3[2]*e1[0] - e3[0]*e1[2],
                                      e3[0]*e1[1] - e3[1]*e1[0] };

    mvh::Mat3 g{};
    for (int k = 0; k < 3; ++k) { g[0][k] = e1[k]; g[1][k] = e2[k]; g[2][k] = e3[k]; }

    std::array<double,3> b{};
    mvh::bunge_from_matrix(g, b[0], b[1], b[2]);
    return b;
}

} // namespace

Composite::Composite() {}

Composite::Composite(short int numCubes, int numColors)
{
    this->numCubes  = numCubes;
    this->numColors = numColors;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────────────────────────────────────

void Composite::readParameters()
{
    const Parameters& p = *Parameters::instance();

    m_dim          = Parameters::compositeDimensions();
    m_hex          = Parameters::compositeHexagonal();
    m_periodic     = flags.isPeriodicStructure || p.getIsPeriodic();
    m_allowOverlap = p.getFiberAllowOverlap();
    m_fibersPerRow = std::max(1, p.getFibersPerRow());

    m_matrixMaterial = p.getMatrixMaterial();
    m_fiberMaterial  = p.getFiberMaterial();

    // Leave room for both phases: a cell that is all fiber or all matrix is not
    // a composite and would make the radius solve degenerate.
    m_targetVf = std::clamp(p.getFiberVolumeFraction(), 1e-3, 0.99);

    // a/b is major over minor by definition; a value below 1 is the same shape
    // rotated by 90 degrees and would break the packing-limit algebra below.
    m_aspect = p.getFiberAspectRatio();
    if (m_aspect < 1.0) {
        qWarning() << "[Composite] fiber a/b" << m_aspect
                   << "is below 1; a/b is major over minor, using" << 1.0 / m_aspect;
        m_aspect = 1.0 / m_aspect;
    }
    m_aspect = std::clamp(m_aspect, 1.0, 100.0);
    m_sqrtK  = std::sqrt(m_aspect);

    m_jitter       = std::clamp(p.getFiberCenterJitter(), 0.0, 1.0);
    m_angleScatter = std::clamp(p.getFiberAngleScatter(), 0.0, 180.0);
}

void Composite::buildFamilies()
{
    const int N = numCubes;
    const int n = m_fibersPerRow;

    std::vector<int> axes;
    switch (m_dim) {
        case 1:  axes = { 2 };       break;   // Z
        case 2:  axes = { 0, 1 };    break;   // X, Y
        default: axes = { 0, 1, 2 }; break;
    }

    // Aligned ellipses get a lattice stretched by the same a/b, so the
    // no-overlap ceiling stays at the circular value for any aspect ratio.
    // Rotated ones cannot use that -- the major axis needs room in every
    // direction -- so their lattice stays isotropic.
    const bool   aligned    = (m_angleScatter <= 0.0);
    const double stretch    = aligned ? m_aspect : 1.0;
    const double rowFactor  = m_hex ? (2.0 / std::sqrt(3.0)) : 1.0;

    m_families.clear();
    for (int ax : axes) {
        Family fam;
        fam.axis = ax;
        fam.nU   = std::min(n, N);

        // Row count that would make the lattice exactly equilateral (in ellipse
        // units); it is almost never an integer, so round and report the gap.
        const double nvIdeal = fam.nU * stretch * rowFactor;

        int nv;
        if (m_hex && m_periodic) {
            // The half-pitch stagger only tiles a torus when the row count is
            // even -- an odd one leaves two unstaggered rows adjacent across
            // the seam. Take the nearest even count, which is not always the
            // one above: 4.62 ideal rows sit closer to 4 than to 6.
            nv = 2 * static_cast<int>(std::lround(0.5 * nvIdeal));
        } else {
            nv = static_cast<int>(std::lround(nvIdeal));
        }
        if (m_hex) nv = std::max(2, nv);

        fam.nV = std::clamp(nv, 1, N);

        fam.pitchU = static_cast<double>(N) / fam.nU;
        fam.pitchV = static_cast<double>(N) / fam.nV;

        // Center offsets divided by the semi-axis that resists them. For
        // aligned ellipses that is a across u and b across v; for rotated ones
        // it is the circumscribed radius a in both directions.
        fam.sepU = fam.pitchU / m_sqrtK;
        fam.sepV = aligned ? fam.pitchV * m_sqrtK : fam.pitchV / m_sqrtK;

        m_families.push_back(std::move(fam));
    }
}

double Composite::nominalRadius() const
{
    const int F = static_cast<int>(m_families.size());
    if (F == 0) return 0.0;

    // Orthogonal families intersect, so their areal fractions do not simply
    // add. Treating them as independent gives 1 - (1 - phi)^F for the union,
    // which is exact for F = 1 and close enough to start a bisection from.
    const double phi = 1.0 - std::pow(1.0 - m_targetVf, 1.0 / F);

    const Family& fam = m_families.front();
    const double  M   = static_cast<double>(fam.nU) * fam.nV;
    const double  N2  = static_cast<double>(numCubes) * numCubes;

    return std::sqrt(phi * N2 / (M * M_PI));
}

double Composite::packingLimitRadius() const
{
    double rMax = std::numeric_limits<double>::max();
    for (const Family& fam : m_families) {
        // Nearest neighbours in the normalised metric: one step along u, and
        // for a staggered lattice the diagonal to the next row.
        const double d = m_hex
                             ? std::min(fam.sepU, std::hypot(0.5 * fam.sepU, fam.sepV))
                             : std::min(fam.sepU, fam.sepV);
        rMax = std::min(rMax, 0.5 * d);
    }
    return rMax;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fiber placement
// ─────────────────────────────────────────────────────────────────────────────

bool Composite::overlapsPlaced(const Family& fam, int i, int j,
                               double cu, double cv, double r) const
{
    const double N  = numCubes;
    const double su = r * m_sqrtK;                                  // semi-major
    const double sv = (m_angleScatter <= 0.0) ? r / m_sqrtK : su;   // see buildFamilies
    if (su <= 0.0 || sv <= 0.0) return false;

    // Jitter never exceeds half a pitch, so only the eight surrounding lattice
    // cells can hold a fiber close enough to touch this one.
    for (int dj = -1; dj <= 1; ++dj) {
        const int jj = ((j + dj) % fam.nV + fam.nV) % fam.nV;
        for (int di = -1; di <= 1; ++di) {
            const int ii = ((i + di) % fam.nU + fam.nU) % fam.nU;

            const Fiber& o = fam.fibers[static_cast<size_t>(jj) * fam.nU + ii];
            if (!o.placed) continue;

            double du = cu - o.cu, dv = cv - o.cv;
            if (du >  0.5 * N) du -= N; else if (du < -0.5 * N) du += N;
            if (dv >  0.5 * N) dv -= N; else if (dv < -0.5 * N) dv += N;

            const double nu = du / su, nv = dv / sv;
            if (nu * nu + nv * nv < 4.0)   // normalised distance below 2 = touching
                return true;
        }
    }
    return false;
}

void Composite::placeFamily(Family& fam, double r)
{
    const double N          = numCubes;
    const double jitterAmp  = 0.5 * m_jitter * std::min(fam.pitchU, fam.pitchV);
    const double halfSpread = 0.5 * m_angleScatter * M_PI / 180.0;

    std::uniform_real_distribution<double> unit(-1.0, 1.0);
    std::uniform_real_distribution<double> angle(-halfSpread, halfSpread);

    fam.fibers.assign(static_cast<size_t>(fam.nU) * fam.nV, Fiber{});

    auto wrap = [N](double c) {
        c = std::fmod(c, N);
        return (c < 0.0) ? c + N : c;
    };

    for (int j = 0; j < fam.nV; ++j) {
        const double shift = (m_hex && (j % 2)) ? 0.5 * fam.pitchU : 0.0;

        for (int i = 0; i < fam.nU; ++i) {
            const double u0 = wrap((i + 0.5) * fam.pitchU + shift);
            const double v0 = (j + 0.5) * fam.pitchV;

            Fiber f;
            f.cu = u0;
            f.cv = v0;

            if (jitterAmp > 0.0) {
                // Overlap allowed -> take the first draw. Otherwise resample
                // until the fiber clears its neighbours, falling back to the
                // lattice site (which is always valid) if it never does.
                const int tries = m_allowOverlap ? 1 : 64;
                for (int t = 0; t < tries; ++t) {
                    double du, dv;
                    do {
                        du = unit(m_rng);
                        dv = unit(m_rng);
                    } while (du * du + dv * dv > 1.0);   // uniform on the disk

                    const double cu = wrap(u0 + du * jitterAmp);
                    const double cv = wrap(v0 + dv * jitterAmp);

                    if (m_allowOverlap || !overlapsPlaced(fam, i, j, cu, cv, r)) {
                        f.cu = cu;
                        f.cv = cv;
                        break;
                    }
                    if (t == tries - 1)
                        ++m_jitterFallbacks;
                }
            }

            if (halfSpread > 0.0) {
                const double theta = angle(m_rng);
                f.cosT = std::cos(theta);
                f.sinT = std::sin(theta);
            }

            f.placed = true;
            fam.fibers[static_cast<size_t>(j) * fam.nU + i] = f;
        }
    }
}

void Composite::assignIds()
{
    // Every fiber gets its own grain id. Orientation is carried per grain id,
    // and each fiber's material frame is derived from its own axis and ellipse
    // angle -- sharing one id across a family would collapse them onto a single
    // frame and silently throw the angle scatter away.
    int32_t next = MatrixId + 1;
    for (Family& fam : m_families)
        for (Fiber& f : fam.fibers)
            f.id = next++;

    numColors = static_cast<int>(next - 1);
    Parameters::instance()->setPoints(numColors);
    if (auto* ogl = OpenGLWidgetQML::getInstance()) {
        ogl->setNumColors(numColors);
    }
}

void Composite::publishPhases()
{
    PhaseAssignment& pa = Parameters::phaseAssignment;
    pa.clear();

    PhaseMaterial matrix, fiber;
    matrix.name = m_matrixMaterial;
    fiber.name  = m_fiberMaterial;

    const bool okMatrix = Parameters::materialStiffnessPa(m_matrixMaterial, matrix.C);
    const bool okFiber  = Parameters::materialStiffnessPa(m_fiberMaterial,  fiber.C);
    if (!okMatrix || !okFiber) {
        // Leaving the assignment empty is the honest outcome: both solvers then
        // take their historical single-material path rather than quietly
        // solving a composite made of one phase.
        qWarning().noquote()
            << QString("[Composite] could not resolve %1; the stress solvers will "
                       "fall back to the single selected material and the "
                       "matrix/fiber contrast will be lost")
                   .arg(!okMatrix ? ("matrix material \"" + m_matrixMaterial + "\"")
                                  : ("fiber material \"" + m_fiberMaterial + "\""));
        return;
    }

    pa.materials = { matrix, fiber };

    const size_t nIds = static_cast<size_t>(numColors) + 1;
    pa.grainPhase.assign(nIds, static_cast<int>(MatrixPhase));
    pa.grainOrientation.assign(nIds, std::array<double,3>{0.0, 0.0, 0.0});

    for (const Family& fam : m_families) {
        for (const Fiber& f : fam.fibers) {
            if (f.id <= 0 || static_cast<size_t>(f.id) >= nIds) continue;
            pa.grainPhase[static_cast<size_t>(f.id)]       = static_cast<int>(FiberPhase);
            pa.grainOrientation[static_cast<size_t>(f.id)] =
                fiberOrientation(fam.axis, f.cosT, f.sinT);
        }
    }

    // The matrix keeps the identity orientation: its material axes line up with
    // the cell axes. For an isotropic row that is meaningless (rotating it is a
    // no-op) and for an anisotropic one it is at least deterministic, rather
    // than an arbitrary draw that would swamp the geometry this algorithm is
    // meant to isolate.
    qDebug().noquote()
        << QString("[Composite] phases: matrix = %1%2, fiber = %3%4 "
                   "(fiber axis 3 along the fiber, axis 1 along the ellipse major axis)")
               .arg(matrix.name, matrix.isIsotropic() ? " (isotropic)" : "")
               .arg(fiber.name,  fiber.isIsotropic()  ? " (isotropic)" : "");

    if (!matrix.isIsotropic())
        qDebug().noquote()
            << QString("[Composite] note: %1 is anisotropic, so the matrix acts as a "
                       "single crystal aligned with the cell axes")
                   .arg(matrix.name);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Rasterization
// ─────────────────────────────────────────────────────────────────────────────

int32_t Composite::fiberAt(const Family& fam, double u, double v, double r,
                           float& rho) const
{
    rho = kOutside;
    if (r <= 0.0) return 0;

    const double N = numCubes;
    const double a = r * m_sqrtK;     // semi-major
    const double b = r / m_sqrtK;     // semi-minor

    // A fiber can only reach this point from a cell within its own major axis,
    // plus one for the jitter (which is capped at half a pitch).
    const int spanU = 1 + static_cast<int>(std::ceil(a / fam.pitchU));
    const int spanV = 1 + static_cast<int>(std::ceil(a / fam.pitchV));

    const int j0 = static_cast<int>(std::floor(v / fam.pitchV));

    int32_t best = 0;

    for (int dj = -spanV; dj <= spanV; ++dj) {
        // Lattice indices always wrap: the *lattice* is periodic by
        // construction. Only the geometry below honours is_periodic, so a
        // non-periodic cell simply clips the fibers that reach past a face.
        const int j = ((j0 + dj) % fam.nV + fam.nV) % fam.nV;

        const double shift = (m_hex && (j % 2)) ? 0.5 * fam.pitchU : 0.0;
        const int    i0    = static_cast<int>(std::floor((u - shift) / fam.pitchU));

        for (int di = -spanU; di <= spanU; ++di) {
            const int i = ((i0 + di) % fam.nU + fam.nU) % fam.nU;

            const Fiber& f = fam.fibers[static_cast<size_t>(j) * fam.nU + i];

            double du = u - f.cu, dv = v - f.cv;
            if (m_periodic) {          // minimum image
                if (du >  0.5 * N) du -= N; else if (du < -0.5 * N) du += N;
                if (dv >  0.5 * N) dv -= N; else if (dv < -0.5 * N) dv += N;
            }

            const double up =  du * f.cosT + dv * f.sinT;
            const double vp = -du * f.sinT + dv * f.cosT;

            // Normalised elliptical radius: <= 1 inside, and smaller the
            // deeper inside the fiber the point sits.
            const double q = (up * up) / (a * a) + (vp * vp) / (b * b);
            if (q <= 1.0) {
                const float qr = static_cast<float>(std::sqrt(q));
                // Jitter can make two fibers of the same family overlap when
                // the user allows it; the nearer one owns the voxel, the same
                // rule that settles crossings between families.
                if (qr < rho) {
                    rho = qr;
                    best = f.id;
                }
            }
        }
    }
    return best;
}

void Composite::buildIdPlane(const Family& fam, double r, int n,
                             std::vector<int32_t>& plane,
                             std::vector<float>& rho) const
{
    plane.assign(static_cast<size_t>(n) * n, 0);
    rho.assign(static_cast<size_t>(n) * n, kOutside);

    // n == numCubes samples voxel centers exactly; a smaller n is the
    // sub-sampled grid the bisection runs on.
    const double h = static_cast<double>(numCubes) / n;

    #pragma omp parallel for schedule(static)
    for (int iu = 0; iu < n; ++iu) {
        const double u = (iu + 0.5) * h;
        for (int iv = 0; iv < n; ++iv) {
            const size_t k = static_cast<size_t>(iu) * n + iv;
            plane[k] = fiberAt(fam, u, (iv + 0.5) * h, r, rho[k]);
        }
    }
}

double Composite::measureVolumeFraction(double r) const
{
    const int    n = m_coarseN;
    const size_t F = m_families.size();

    std::vector<std::vector<int32_t>> planes(F);
    std::vector<std::vector<float>>   rho(F);
    for (size_t f = 0; f < F; ++f)
        buildIdPlane(m_families[f], r, n, planes[f], rho[f]);

    long long occupied = 0;

    // Only the union matters here -- which family owns a crossing does not
    // change the volume fraction, so the solve is unaffected by the
    // attribution rule rasterize() applies.
    #pragma omp parallel for collapse(2) schedule(static) reduction(+:occupied)
    for (int x = 0; x < n; ++x) {
        for (int y = 0; y < n; ++y) {
            for (int z = 0; z < n; ++z) {
                for (size_t f = 0; f < F; ++f) {
                    int iu = 0, iv = 0;
                    planeIndex(m_families[f].axis, x, y, z, iu, iv);
                    if (planes[f][static_cast<size_t>(iu) * n + iv]) {
                        ++occupied;
                        break;
                    }
                }
            }
        }
    }

    return static_cast<double>(occupied) /
           (static_cast<double>(n) * n * n);
}

void Composite::rasterize(double r)
{
    const int    N = numCubes;
    const size_t F = m_families.size();

    std::vector<std::vector<int32_t>> planes(F);
    std::vector<std::vector<float>>   rho(F);
    for (size_t f = 0; f < F; ++f)
        buildIdPlane(m_families[f], r, N, planes[f], rho[f]);

    long long fiberVoxels = 0;

    // Orthogonal families necessarily intersect, and a voxel can only belong to
    // one of them, so the crossings go to whichever fiber the voxel sits
    // deepest inside. Taking the first family listed instead would hand it the
    // whole intersection, which at 2 families and Vf 0.7 is nearly half the
    // reinforcement -- a 0/90 layup that is anything but balanced.
    #pragma omp parallel for collapse(2) schedule(static) reduction(+:fiberVoxels)
    for (int x = 0; x < N; ++x) {
        for (int y = 0; y < N; ++y) {
            for (int z = 0; z < N; ++z) {
                // Collect the families this voxel sits deepest inside. Exact
                // ties are not a corner case here: on a symmetric 0/90 lattice
                // every voxel equidistant from an X fiber and a Y fiber ties,
                // which is a quarter of the crossing volume. Handing those to
                // whichever family is listed first tilts the layup badly, so
                // they are dealt out alternately instead -- deterministic, and
                // even across the cell.
                constexpr float kTieTol = 1e-6f;
                int32_t cand[3] = {0, 0, 0};
                int     nc      = 0;
                float   best    = kOutside;

                for (size_t f = 0; f < F; ++f) {
                    int iu = 0, iv = 0;
                    planeIndex(m_families[f].axis, x, y, z, iu, iv);
                    const size_t  k = static_cast<size_t>(iu) * N + iv;
                    const int32_t g = planes[f][k];
                    if (!g) continue;

                    const float q = rho[f][k];
                    if (q < best - kTieTol) {          // a clear winner so far
                        best = q;
                        nc   = 0;
                        cand[nc++] = g;
                    } else if (q <= best + kTieTol) {  // tied with it
                        best = std::min(best, q);
                        if (nc < 3) cand[nc++] = g;
                    }
                }

                const int32_t id = (nc > 0) ? cand[(x + y + z) % nc] : MatrixId;
                if (nc > 0) ++fiberVoxels;
                voxels[x][y][z] = id;
            }
        }
    }

    m_achievedVf = static_cast<double>(fiberVoxels) /
                   (static_cast<double>(N) * N * N);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Volume fraction solve
// ─────────────────────────────────────────────────────────────────────────────

double Composite::solveRadius()
{
    const double rMax = packingLimitRadius();

    double hi = rMax;
    if (m_allowOverlap) {
        // Merged fibers can reach any fraction below 1, so push the bracket out
        // until it actually contains the target.
        for (int i = 0; i < 20 && measureVolumeFraction(hi) < m_targetVf; ++i)
            hi *= 1.3;
    } else {
        const double vfMax = measureVolumeFraction(rMax);
        if (m_targetVf > vfMax + 1e-6) {
            qWarning().noquote()
                << QString("[Composite] fiber volume fraction %1 is above the "
                           "no-overlap limit %2 for this cell "
                           "(%3 packing, %4 fibers per row, a/b = %5); clamping to it. "
                           "Raise the fiber count, lower a/b, or enable overlap.")
                       .arg(m_targetVf, 0, 'f', 3)
                       .arg(vfMax, 0, 'f', 3)
                       .arg(m_hex ? "hexagonal" : "square")
                       .arg(m_fibersPerRow)
                       .arg(m_aspect, 0, 'g', 3);
            m_targetVf = vfMax;
            m_clamped  = true;
            return rMax;
        }
    }

    // Volume fraction is monotone in the radius once the centers are fixed, so
    // plain bisection converges without any bracketing surprises.
    //
    // On a voxel grid Vf(r) is a staircase, and on a perfect lattice every
    // fiber crosses the same step at the same radius -- so the steps are as
    // wide as (fibers) x (voxels flipped per fiber) and the target usually
    // falls strictly inside one. Bisection alone would then return whichever
    // side of that step it happened to converge on, so keep the radius whose
    // measured fraction is actually closest to what was asked for.
    double lo      = 0.0;
    double bestR   = hi;
    double bestErr = std::numeric_limits<double>::max();

    for (int it = 0; it < 40; ++it) {
        const double mid = 0.5 * (lo + hi);
        const double vf  = measureVolumeFraction(mid);

        const double err = std::abs(vf - m_targetVf);
        if (err < bestErr) { bestErr = err; bestR = mid; }

        if (err < 2e-4 || (hi - lo) < 1e-4)
            break;

        if (vf < m_targetVf) lo = mid; else hi = mid;
    }
    return bestR;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Generation
// ─────────────────────────────────────────────────────────────────────────────

void Composite::Initialization(bool /*isWaveGeneration*/)
{
    m_rng.seed(Parameters::seed);
    readParameters();
    buildFamilies();

    // Fast enough to call ~40 times inside the bisection, and exact whenever
    // the cell itself is no bigger than this.
    m_coarseN = std::min<int>(numCubes, 128);

    m_jitterFallbacks = 0;
    m_clamped         = false;

    // The centers have to be fixed before the radius can be solved for, so the
    // jitter rejection uses the analytic estimate. It over-estimates the final
    // radius for 2D/3D (the families overlap), which only makes the clearance
    // it enforces more conservative.
    const double rNom = std::min(nominalRadius(), packingLimitRadius());
    for (Family& fam : m_families)
        placeFamily(fam, rNom);

    assignIds();
    publishPhases();
    m_radius = solveRadius();

    // One representative voxel for the matrix (grain id 1) plus one on each fiber
    // axis, so the seed CSV and grain bookkeeping describe the full multi-phase
    // structure matching grain IDs 1..numColors.
    seedPoints.clear();
    seedPoints.push_back({ 0, 0, 0 });
    const int mid = numCubes / 2;
    for (const Family& fam : m_families) {
        for (const Fiber& f : fam.fibers) {
            const int cu = std::clamp(static_cast<int>(f.cu), 0, numCubes - 1);
            const int cv = std::clamp(static_cast<int>(f.cv), 0, numCubes - 1);
            switch (fam.axis) {
                case 0:  seedPoints.push_back({ mid, cu,  cv  }); break;
                case 1:  seedPoints.push_back({ cv,  mid, cu  }); break;
                default: seedPoints.push_back({ cu,  cv,  mid }); break;
            }
        }
    }

    remainingPoints = 0;
    filled_voxels   = 0;
    flags.isDone    = false;
    m_step          = 0;
    m_steps         = std::clamp(static_cast<int>(numCubes) / 2, 8, 40);

    reportLayout(m_radius);
}

void Composite::reportLayout(double r) const
{
    if (m_families.empty()) return;

    const Family& fam = m_families.front();
    const double  a   = r * m_sqrtK;
    const double  b   = r / m_sqrtK;

    QString axes;
    for (const Family& f : m_families)
        axes += QString(axes.isEmpty() ? "" : "+") + axisName(f.axis);

    qDebug().noquote()
        << QString("[Composite] %1D %2 packing, fibers along %3, %4^3 grid%5")
               .arg(m_dim)
               .arg(m_hex ? "hexagonal" : "square")
               .arg(axes)
               .arg(numCubes)
               .arg(m_periodic ? ", periodic" : "");

    qDebug().noquote()
        << QString("[Composite] %1 x (%2 x %3) fibers, pitch %4 x %5 vox, "
                   "semi-axes a = %6  b = %7 vox (a/b = %8)")
               .arg(m_families.size())
               .arg(fam.nU).arg(fam.nV)
               .arg(fam.pitchU, 0, 'f', 2).arg(fam.pitchV, 0, 'f', 2)
               .arg(a, 0, 'f', 2).arg(b, 0, 'f', 2)
               .arg(m_aspect, 0, 'g', 3);

    // An equilateral lattice wants pitchV / pitchU = sqrt(3)/2 (times b/a for
    // stretched aligned ellipses), which almost never lands on an integer row
    // count -- so say how far off the cell that was actually built is.
    if (m_hex) {
        const double aligned = (m_angleScatter <= 0.0) ? m_aspect : 1.0;
        const double ideal   = std::sqrt(3.0) / 2.0 / aligned;
        const double actual  = fam.pitchV / fam.pitchU;
        if (std::abs(actual - ideal) > 0.02 * ideal)
            qDebug().noquote()
                << QString("[Composite] row spacing is %1 of the in-row pitch "
                           "against %2 for an equilateral lattice: it would take "
                           "%3 rows, and only whole%4 ones tile the cell")
                       .arg(actual, 0, 'f', 3).arg(ideal, 0, 'f', 3)
                       .arg(fam.nU / ideal, 0, 'f', 2)
                       .arg(m_periodic ? " even" : "");
    }

    if (m_jitter > 0.0 || m_angleScatter > 0.0)
        qDebug().noquote()
            << QString("[Composite] imperfections: center jitter %1 pitch, "
                       "angle scatter %2 deg%3")
                   .arg(m_jitter, 0, 'g', 3)
                   .arg(m_angleScatter, 0, 'g', 3)
                   .arg(m_allowOverlap ? ", overlap allowed" : "");

    if (m_jitterFallbacks > 0)
        qWarning().noquote()
            << QString("[Composite] %1 fiber centers could not be jittered "
                       "without touching a neighbour and stayed on the lattice; "
                       "lower the volume fraction or the jitter to free them up")
                   .arg(m_jitterFallbacks);

    // Under ~2 voxels across, a fiber is a staircase rather than an ellipse and
    // the measured volume fraction stops meaning much.
    if (b < 1.5)
        qWarning().noquote()
            << QString("[Composite] the fiber minor semi-axis is only %1 voxels; "
                       "raise the cube size or lower the fiber count for a "
                       "resolved cross-section")
                   .arg(b, 0, 'f', 2);
}

void Composite::reportVolumeFraction() const
{
    const long long total = static_cast<long long>(numCubes) * numCubes * numCubes;

    qDebug().noquote()
        << QString("[Composite] volume fraction: target %1, achieved %2 "
                   "(%3 of %4 voxels are fiber)%5")
               .arg(m_targetVf, 0, 'f', 4)
               .arg(m_achievedVf, 0, 'f', 4)
               .arg(std::llround(m_achievedVf * static_cast<double>(total)))
               .arg(total)
               .arg(m_clamped ? "  [clamped to the packing limit]" : "");

    // Vf(r) is a staircase on a voxel grid, and identical fibers on a perfect
    // lattice all cross a step together -- so the reachable fractions come in
    // jumps of (fiber count) x (voxels per symmetric ring) / N^2 and the target
    // can simply fall between two of them. Nothing is wrong; the cell just
    // cannot express what was asked for at this resolution.
    if (!m_clamped && std::abs(m_achievedVf - m_targetVf) > 0.01)
        qWarning().noquote()
            << QString("[Composite] that is %1 off the requested fraction: on a "
                       "%2^3 grid the fibers can only step through discrete "
                       "voxel counts. Raise the cube size for finer control.")
                   .arg(m_achievedVf - m_targetVf, 0, 'f', 4)
                   .arg(numCubes);
}

void Composite::Generate_To_End()
{
    if (m_families.empty()) {
        qCritical() << "[Composite] no fiber families were built; nothing to fill";
        flags.isDone = true;
        return;
    }

    rasterize(m_radius);

    if (!seedPoints.empty()) {
        for (int x = 0; x < numCubes; ++x) {
            for (int y = 0; y < numCubes; ++y) {
                for (int z = 0; z < numCubes; ++z) {
                    if (voxels[x][y][z] == MatrixId) {
                        seedPoints[0] = { x, y, z };
                        x = y = z = numCubes;
                    }
                }
            }
        }
    }

    filled_voxels = static_cast<unsigned int>(
        static_cast<long long>(numCubes) * numCubes * numCubes);
    IterationNumber = 1;
    m_step          = m_steps;
    flags.isDone    = true;

    reportVolumeFraction();
}

void Composite::Next_Iteration()
{
    if (getDone()) return;

    if (m_families.empty()) {
        qCritical() << "[Composite] no fiber families were built; nothing to fill";
        flags.isDone = true;
        return;
    }

    // The animated path grows the fibers out of their axes. The last step uses
    // the full radius, so it lands on exactly the structure Generate_To_End()
    // produces in one shot.
    ++m_step;
    const double frac = static_cast<double>(m_step) / m_steps;

    rasterize(m_radius * frac);
    ++IterationNumber;

    const long long total = static_cast<long long>(numCubes) * numCubes * numCubes;
    filled_voxels = static_cast<unsigned int>(
        (m_step >= m_steps) ? total : static_cast<long long>(frac * total));

    if (m_step >= m_steps) {
        if (!seedPoints.empty()) {
            for (int x = 0; x < numCubes; ++x) {
                for (int y = 0; y < numCubes; ++y) {
                    for (int z = 0; z < numCubes; ++z) {
                        if (voxels[x][y][z] == MatrixId) {
                            seedPoints[0] = { x, y, z };
                            x = y = z = numCubes;
                        }
                    }
                }
            }
        }
        flags.isDone = true;
        reportVolumeFraction();
    }
}

// Not the base class's filled_voxels test: the matrix phase fills every voxel
// on the very first step, which would end the animation immediately.
bool Composite::getDone() const
{
    return flags.isDone;
}

void Composite::CleanUp()
{
    Parent_Algorithm::CleanUp();

    m_families.clear();
    m_families.shrink_to_fit();
    m_radius          = 0.0;
    m_achievedVf      = 0.0;
    m_clamped         = false;
    m_jitterFallbacks = 0;
    m_step            = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Plugin registration -- the complete wiring for this algorithm
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<ParamField> compositeSchema()
{
    // No baseAlgorithmSchema(): a composite has neither nucleation waves nor
    // seed points, so "Points" and the wave fields would be dead controls. The
    // fiber count and the volume fraction take their place.
    std::vector<ParamField> s = {
        { "size", "Cube size", ParamField::Int, 60, 4, 500, {}, "main" },

        { "composite_dim", "Reinforcement", ParamField::Enum,
         "1D (fibers along Z)", {}, {},
         { "1D (fibers along Z)", "2D (fibers along X,Y)", "3D (fibers along X,Y,Z)" },
         "main" },

        { "composite_packing", "Packing", ParamField::Enum, "Square", {}, {},
         { "Square", "Hexagonal" }, "main" },

        { "fiber_volume_fraction", "Fiber volume fraction", ParamField::Double,
         0.40, 0.001, 0.99, {}, "main" },
        { "fibers_per_row", "Fibers per row", ParamField::Int, 3, 1, 64, {}, "main" },

        { "fiber_aspect_ratio",  "Fiber a/b",                 ParamField::Double, 1.0, 1.0, 10.0,  {}, "main" },
        { "fiber_angle_scatter", "Fiber angle scatter (deg)", ParamField::Double, 0.0, 0.0, 180.0, {}, "main" },
        { "fiber_center_jitter", "Center jitter (0..1)",      ParamField::Double, 0.0, 0.0, 1.0,   {}, "main" },

        { "fiber_allow_overlap", "Allow fibers to overlap", ParamField::Bool, false, {}, {}, {}, "main" },

        { "is_periodic", "Periodic cell", ParamField::Bool, false, {}, {}, {}, "main" },
    };

    // Two constituents rather than the single db_material the other algorithms
    // use, and no texture fields: a fiber's orientation comes from its own
    // geometry, and the matrix is a single unrotated phase, so there is no
    // grain-orientation distribution left for a texture preset to describe.
    ParamField matrix{ "matrix_material", "Matrix material", ParamField::Enum,
                      "Epoxy", {}, {}, {}, "main" };
    matrix.optionsProvider = "materials";
    s.push_back(matrix);

    ParamField fiber{ "fiber_material", "Fiber material", ParamField::Enum,
                     "C-fiber", {}, {}, {}, "main" };
    fiber.optionsProvider = "materials";
    s.push_back(fiber);

    return s;
}

MATVIZ_REGISTER_ALGORITHM(AlgorithmPlugin{
    "Composite",
    "Fiber-reinforced RVE: 1D, 2D or 3D families of elliptical fibers on a "
    "square or hexagonal lattice, sized to hit a target volume fraction, with "
    "center jitter and per-fiber ellipse rotation as RVE imperfections.",
    /*order=*/ 1,
    compositeSchema(),
    [](const Parameters& p) {
        return std::make_shared<Composite>(static_cast<short int>(p.getSize()),
                                           p.getPoints());
    }
});

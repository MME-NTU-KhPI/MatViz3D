#include "lattice.h"

#include <QDebug>
#include <QString>
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <vector>

#include "parameters.h"
#include "algorithmplugin.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Geometry helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

constexpr double kPi = 3.14159265358979323846;

// TPMS level-set approximations. Arguments in radians: one cell is 2*pi.
inline double gyroidF(double X, double Y, double Z)
{
    return std::sin(X) * std::cos(Y) + std::sin(Y) * std::cos(Z) + std::sin(Z) * std::cos(X);
}

inline double schwarzPF(double X, double Y, double Z)
{
    return std::cos(X) + std::cos(Y) + std::cos(Z);
}

inline double schwarzDF(double X, double Y, double Z)
{
    const double sx = std::sin(X), cx = std::cos(X);
    const double sy = std::sin(Y), cy = std::cos(Y);
    const double sz = std::sin(Z), cz = std::cos(Z);
    return sx * sy * sz + sx * cy * cz + cx * sy * cz + cx * cy * sz;
}

inline double iwpF(double X, double Y, double Z)
{
    const double cx = std::cos(X), cy = std::cos(Y), cz = std::cos(Z);
    return 2.0 * (cx * cy + cy * cz + cz * cx)
         - (std::cos(2.0 * X) + std::cos(2.0 * Y) + std::cos(2.0 * Z));
}

// Distance from the point p to the segment ab.
inline double segmentDistance(double px, double py, double pz,
                              double ax, double ay, double az,
                              double bx, double by, double bz)
{
    const double dx = bx - ax, dy = by - ay, dz = bz - az;
    const double L2 = dx * dx + dy * dy + dz * dz;
    double t = 0.0;
    if (L2 > 0.0)
        t = std::clamp(((px - ax) * dx + (py - ay) * dy + (pz - az) * dz) / L2, 0.0, 1.0);
    const double qx = ax + t * dx - px;
    const double qy = ay + t * dy - py;
    const double qz = az + t * dz - pz;
    return std::sqrt(qx * qx + qy * qy + qz * qz);
}

inline double wrap01(double x) { return x - std::floor(x); }

// Surface area of one unit cell of the level-set surface, for the sheet
// thickness estimate t ~ rho * a / A. Textbook values for the trigonometric
// approximations; an estimate only, used to warn about thin walls.
inline double tpmsAreaPerCell(LatticeFamily f)
{
    switch (f) {
        case LatticeFamily::GyroidSheet:   return 3.09;
        case LatticeFamily::SchwarzPSheet: return 2.35;
        case LatticeFamily::SchwarzDSheet: return 3.84;
        case LatticeFamily::IWPSheet:      return 3.47;
        default:                           return 0.0;
    }
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Construction / setup
// ─────────────────────────────────────────────────────────────────────────────

Lattice::Lattice() {}

Lattice::Lattice(short int numCubes, int numColors)
{
    this->numCubes  = numCubes;
    this->numColors = numColors;
}

void Lattice::readParameters()
{
    Parameters& p = *Parameters::instance();

    LatticeFamily f = LatticeFamily::GyroidSheet;
    if (!parseLatticeFamily(p.getLatticeFamily(), f)) {
        qWarning().noquote() << QString("[Lattice] unknown cell family \"%1\"; using Gyroid (sheet)")
                                    .arg(p.getLatticeFamily());
        f = LatticeFamily::GyroidSheet;
    }
    m_family = f;
    m_signed = latticeFamilyInfo(f).signedLevelSet;

    m_cells    = std::clamp(p.getCellsPerEdge(), 1, 64);
    m_target   = std::clamp(p.getRelativeDensity(), 0.01, 0.99);
    m_grading  = std::clamp(p.getLatticeGrading(), 0.0, 0.9);
    m_jitter   = std::clamp(p.getLatticeJitter(), 0.0, 1.0);
    m_periodic = flags.isPeriodicStructure || p.getIsPeriodic();

    // A lattice without a material would silently be solved as the solvers'
    // fallback (copper). Default to the printed polymer the cell is meant for
    // and say so, and write it back so the HDF5 metadata matches what ran.
    m_material = p.getDbMaterial();
    if (m_material.isEmpty()) {
        m_material = QStringLiteral("PETG");
        p.setDbMaterial(m_material);
        qInfo() << "[Lattice] no material selected; using PETG";
    }

    const double d2r = kPi / 180.0;
    m_angles[0] = p.getOrientationAngleA() * d2r;
    m_angles[1] = p.getOrientationAngleB() * d2r;
    m_angles[2] = p.getOrientationAngleC() * d2r;
}

// R = Rz(a) * Rx(b) * Rz(c), applied to coordinates about the cube centre.
void Lattice::buildRotation()
{
    const double a = m_angles[0], b = m_angles[1], c = m_angles[2];
    m_rotated = std::abs(a) > 1e-12 || std::abs(b) > 1e-12 || std::abs(c) > 1e-12;

    const double ca = std::cos(a), sa = std::sin(a);
    const double cb = std::cos(b), sb = std::sin(b);
    const double cc = std::cos(c), sc = std::sin(c);
    const double Rz1[3][3] = { { ca, -sa, 0.0 }, { sa, ca, 0.0 }, { 0.0, 0.0, 1.0 } };
    const double Rx [3][3] = { { 1.0, 0.0, 0.0 }, { 0.0, cb, -sb }, { 0.0, sb, cb } };
    const double Rz2[3][3] = { { cc, -sc, 0.0 }, { sc, cc, 0.0 }, { 0.0, 0.0, 1.0 } };

    double T[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += Rz1[i][k] * Rx[k][j];
            T[i][j] = s;
        }
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += T[i][k] * Rz2[k][j];
            m_R[i][j] = s;
        }
}

void Lattice::buildSegments()
{
    m_segments.clear();
    m_segments2D = false;
    m_hcW = m_hcP = 1.0;
    m_cellsY = m_cells;
    m_hcStretch = 1.0;

    auto add = [&](double ax, double ay, double az, double bx, double by, double bz) {
        m_segments.push_back({ ax, ay, az, bx, by, bz });
    };

    // The six face diagonals of the unit cube (opposite faces coincide under
    // periodicity, so one face per axis is enough).
    auto addFcc = [&]() {
        add(0, 0, 0,  0, 1, 1);  add(0, 1, 0,  0, 0, 1);   // face x = 0
        add(0, 0, 0,  1, 0, 1);  add(1, 0, 0,  0, 0, 1);   // face y = 0
        add(0, 0, 0,  1, 1, 0);  add(1, 0, 0,  0, 1, 0);   // face z = 0
    };

    // Hexagonal honeycomb family in wall-length units (l = 1): vertical ribs
    // of length h at x = 0 and, staggered by half a period, at x = W/2;
    // inclined walls of length 1 at angle theta to the horizontal join the end
    // of each rib to the ends of its two staggered neighbours. theta > 0 is
    // the regular hexagon, theta < 0 folds the walls inward (re-entrant,
    // auxetic). Periodic images supply the rest of the tiling.
    auto honeycomb = [&](double thetaDeg, double h) {
        const double th = thetaDeg * kPi / 180.0;
        const double W  = 2.0 * std::cos(th);
        const double P  = 2.0 * (h + std::sin(th));
        m_hcW = W;
        m_hcP = P;
        m_segments2D = true;

        add(0.0, 0.0, 0.0,  0.0, h, 0.0);
        add(W / 2, P / 2, 0.0,  W / 2, P / 2 + h, 0.0);
        add(0.0, h, 0.0,   W / 2, P / 2, 0.0);
        add(0.0, h, 0.0,  -W / 2, P / 2, 0.0);
        add(0.0, 0.0, 0.0,  W / 2, P / 2 + h - P, 0.0);
        add(0.0, 0.0, 0.0, -W / 2, P / 2 + h - P, 0.0);

        // A hexagonal tiling has period P along Y for every W along X, so a
        // square cube cannot hold whole cells both ways. Round the count along
        // Y and stretch the cells to fit; the factor is reported.
        const double ideal = m_cells * W / P;
        m_cellsY    = std::max(1, static_cast<int>(std::lround(ideal)));
        m_hcStretch = (m_cells * W) / (m_cellsY * P);
    };

    switch (m_family) {
        case LatticeFamily::SimpleCubic:
            add(0, 0, 0,  1, 0, 0);  add(0, 0, 0,  0, 1, 0);  add(0, 0, 0,  0, 0, 1);
            break;
        case LatticeFamily::BCC:
            add(0, 0, 0,  1, 1, 1);  add(1, 0, 0,  0, 1, 1);
            add(0, 1, 0,  1, 0, 1);  add(0, 0, 1,  1, 1, 0);
            break;
        case LatticeFamily::FCC:
            addFcc();
            break;
        case LatticeFamily::Octet: {
            addFcc();
            // The inner octahedron: edges between the centres of adjacent faces.
            const double c[6][3] = { { 0.0, 0.5, 0.5 }, { 1.0, 0.5, 0.5 },
                                     { 0.5, 0.0, 0.5 }, { 0.5, 1.0, 0.5 },
                                     { 0.5, 0.5, 0.0 }, { 0.5, 0.5, 1.0 } };
            for (int i = 0; i < 6; ++i)
                for (int j = i + 1; j < 6; ++j) {
                    if (j / 2 == i / 2) continue;   // opposite faces: no edge
                    add(c[i][0], c[i][1], c[i][2], c[j][0], c[j][1], c[j][2]);
                }
            break;
        }
        case LatticeFamily::Honeycomb:  honeycomb( 30.0, 1.0); break;
        case LatticeFamily::Reentrant:  honeycomb(-30.0, 2.0); break;
        default:
            break;
    }
}

void Lattice::buildWaves()
{
    m_waves.clear();
    m_jitterWaves.clear();
    m_jitterNorm = 1.0;

    std::uniform_real_distribution<double> phase(0.0, 2.0 * kPi);

    // Draws `count` waves whose integer wave vectors (periodic in the cube by
    // construction) have kmin <= |k| <= kmax, one of each +-k pair. Returns
    // how many distinct directions were available.
    auto draw = [&](double kmin, double kmax, int count, std::vector<Wave>& out) -> size_t {
        std::vector<std::array<int,3>> cand;
        const int K = static_cast<int>(std::ceil(kmax));
        for (int kx = -K; kx <= K; ++kx)
            for (int ky = -K; ky <= K; ++ky)
                for (int kz = -K; kz <= K; ++kz) {
                    if (kx < 0 || (kx == 0 && (ky < 0 || (ky == 0 && kz <= 0)))) continue;
                    const double n = std::sqrt(double(kx * kx + ky * ky + kz * kz));
                    if (n >= kmin && n <= kmax) cand.push_back({ kx, ky, kz });
                }
        if (cand.empty()) return 0;
        std::uniform_int_distribution<size_t> pick(0, cand.size() - 1);
        for (int i = 0; i < count; ++i) {
            const std::array<int,3> k = cand[pick(m_rng)];
            out.push_back({ double(k[0]), double(k[1]), double(k[2]), phase(m_rng) });
        }
        return cand.size();
    };

    if (m_family == LatticeFamily::Spinodoid) {
        // Waves per cube edge ~ cells per edge, i.e. one "cell" per wavelength.
        const double beta = static_cast<double>(m_cells);
        if (draw(0.7 * beta, 1.3 * beta, 40, m_waves) < 10) {
            m_waves.clear();
            draw(0.5 * beta, 2.0 * beta, 40, m_waves);
        }
    }

    if (m_jitter > 0.0)
        draw(1.0, 2.0 * m_cells, 12, m_jitterWaves);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fields
// ─────────────────────────────────────────────────────────────────────────────

double Lattice::rawField(double X, double Y, double Z) const
{
    switch (m_family) {
        case LatticeFamily::GyroidSheet:
        case LatticeFamily::GyroidSkeletal: {
            const double a = 2.0 * kPi * m_cells;
            const double f = gyroidF(a * X, a * Y, a * Z);
            return m_signed ? f : std::abs(f);
        }
        case LatticeFamily::SchwarzPSheet:
        case LatticeFamily::SchwarzPSkeletal: {
            const double a = 2.0 * kPi * m_cells;
            const double f = schwarzPF(a * X, a * Y, a * Z);
            return m_signed ? f : std::abs(f);
        }
        case LatticeFamily::SchwarzDSheet:
        case LatticeFamily::SchwarzDSkeletal: {
            const double a = 2.0 * kPi * m_cells;
            const double f = schwarzDF(a * X, a * Y, a * Z);
            return m_signed ? f : std::abs(f);
        }
        case LatticeFamily::IWPSheet:
        case LatticeFamily::IWPSkeletal: {
            const double a = 2.0 * kPi * m_cells;
            const double f = iwpF(a * X, a * Y, a * Z);
            return m_signed ? f : std::abs(f);
        }
        case LatticeFamily::SimpleCubic:
        case LatticeFamily::BCC:
        case LatticeFamily::FCC:
        case LatticeFamily::Octet: {
            const double u = wrap01(X * m_cells);
            const double v = wrap01(Y * m_cells);
            const double w = wrap01(Z * m_cells);
            double best = 1e300;
            for (const Segment& s : m_segments)
                for (int ox = -1; ox <= 1; ++ox)
                    for (int oy = -1; oy <= 1; ++oy)
                        for (int oz = -1; oz <= 1; ++oz) {
                            const double d = segmentDistance(u + ox, v + oy, w + oz,
                                                             s.ax, s.ay, s.az, s.bx, s.by, s.bz);
                            if (d < best) best = d;
                        }
            return best;
        }
        case LatticeFamily::Honeycomb:
        case LatticeFamily::Reentrant: {
            double u = X * m_cells  * m_hcW;
            double v = Y * m_cellsY * m_hcP;
            u -= m_hcW * std::floor(u / m_hcW);
            v -= m_hcP * std::floor(v / m_hcP);
            double best = 1e300;
            for (const Segment& s : m_segments)
                for (int ox = -1; ox <= 1; ++ox)
                    for (int oy = -1; oy <= 1; ++oy) {
                        const double d = segmentDistance(u + ox * m_hcW, v + oy * m_hcP, 0.0,
                                                         s.ax, s.ay, 0.0, s.bx, s.by, 0.0);
                        if (d < best) best = d;
                    }
            return best;
        }
        case LatticeFamily::Spinodoid: {
            if (m_waves.empty()) return 0.0;
            const double amp = std::sqrt(2.0 / static_cast<double>(m_waves.size()));
            double g = 0.0;
            for (const Wave& w : m_waves)
                g += std::cos(2.0 * kPi * (w.kx * X + w.ky * Y + w.kz * Z) + w.phase);
            return amp * g;
        }
    }
    return 0.0;
}

double Lattice::jitterField(double X, double Y, double Z) const
{
    if (m_jitterWaves.empty()) return 0.0;
    double n = 0.0;
    for (const Wave& w : m_jitterWaves)
        n += std::cos(2.0 * kPi * (w.kx * X + w.ky * Y + w.kz * Z) + w.phase);
    return n / std::sqrt(static_cast<double>(m_jitterWaves.size()));
}

double Lattice::modulation(double X, double Y, double Z) const
{
    double d = m_grading * (2.0 * Z - 1.0);
    if (m_jitter > 0.0)
        d += m_jitter * jitterField(X, Y, Z) / m_jitterNorm;
    return d;
}

double Lattice::fieldAt(double x, double y, double z) const
{
    const double invN = 1.0 / static_cast<double>(numCubes);
    const double X = x * invN, Y = y * invN, Z = z * invN;

    double Xr = X, Yr = Y, Zr = Z;
    if (m_rotated) {
        const double px = X - 0.5, py = Y - 0.5, pz = Z - 0.5;
        Xr = m_R[0][0] * px + m_R[0][1] * py + m_R[0][2] * pz + 0.5;
        Yr = m_R[1][0] * px + m_R[1][1] * py + m_R[1][2] * pz + 0.5;
        Zr = m_R[2][0] * px + m_R[2][1] * py + m_R[2][2] * pz + 0.5;
    }

    const double F = rawField(Xr, Yr, Zr);
    if (m_grading <= 0.0 && m_jitter <= 0.0) return F;

    // Grading and jitter are defined in the cube frame (Z is the build
    // direction), not in the rotated lattice frame.
    const double d = modulation(X, Y, Z);
    if (m_signed) return F - d;                   // shift the level
    return F / std::max(0.1, 1.0 + d);            // scale the thickness
}

// ─────────────────────────────────────────────────────────────────────────────
//  Density -> threshold
// ─────────────────────────────────────────────────────────────────────────────

double Lattice::solveThreshold()
{
    const int    Nc   = m_coarseN;
    const double step = static_cast<double>(numCubes) / Nc;
    const size_t n    = static_cast<size_t>(Nc) * Nc * Nc;

    if (m_jitter > 0.0 && !m_jitterWaves.empty()) {
        double mx = 0.0;
        #pragma omp parallel for collapse(3) reduction(max:mx) schedule(static)
        for (int i = 0; i < Nc; ++i)
            for (int j = 0; j < Nc; ++j)
                for (int k = 0; k < Nc; ++k) {
                    const double v = std::abs(jitterField((i + 0.5) / Nc, (j + 0.5) / Nc, (k + 0.5) / Nc));
                    if (v > mx) mx = v;
                }
        m_jitterNorm = std::max(1e-9, mx);
    }

    std::vector<double> vals(n);
    #pragma omp parallel for collapse(3) schedule(static)
    for (int i = 0; i < Nc; ++i)
        for (int j = 0; j < Nc; ++j)
            for (int k = 0; k < Nc; ++k)
                vals[(static_cast<size_t>(i) * Nc + j) * Nc + k] =
                    fieldAt((i + 0.5) * step, (j + 0.5) * step, (k + 0.5) * step);

    std::sort(vals.begin(), vals.end());
    long long idx = std::llround(m_target * static_cast<double>(n)) - 1;
    idx = std::clamp<long long>(idx, 0, static_cast<long long>(n) - 1);
    const double v = vals[static_cast<size_t>(idx)];

    // Ties: a symmetric cell repeats one field value at many voxels (whole
    // shells of equal distance around an axis-aligned strut, mirror images of
    // a TPMS), and {F <= c} takes all of them or none. Pick whichever side of
    // the tie lands closer to the target.
    const auto lo = std::lower_bound(vals.begin(), vals.end(), v) - vals.begin();   // first == v
    const auto hi = std::upper_bound(vals.begin(), vals.end(), v) - vals.begin();   // first  > v
    const double withTies    = static_cast<double>(hi) / static_cast<double>(n);
    const double withoutTies = static_cast<double>(lo) / static_cast<double>(n);
    if (lo > 0 && std::abs(withoutTies - m_target) < std::abs(withTies - m_target))
        return vals[static_cast<size_t>(lo - 1)];
    return v;
}

long long Lattice::rasterize(double c, int z0, int z1)
{
    const int N = numCubes;
    z0 = std::clamp(z0, 0, N);
    z1 = std::clamp(z1, z0, N);
    long long solid = 0;

    #pragma omp parallel for collapse(2) schedule(static) reduction(+:solid)
    for (int x = 0; x < N; ++x)
        for (int y = 0; y < N; ++y)
            for (int z = z0; z < z1; ++z) {
                const bool s = fieldAt(x + 0.5, y + 0.5, z + 0.5) <= c;
                voxels[x][y][z] = s ? SolidId : 0;
                if (s) ++solid;
            }
    return solid;
}

void Lattice::estimateWall()
{
    const double rho        = m_achieved > 0.0 ? m_achieved : m_target;
    const double voxPerCell = static_cast<double>(numCubes) / m_cells;
    m_wallVox = 0.0;

    switch (m_family) {
        case LatticeFamily::GyroidSheet:
        case LatticeFamily::SchwarzPSheet:
        case LatticeFamily::SchwarzDSheet:
        case LatticeFamily::IWPSheet:
            m_wallVox = rho * voxPerCell / tpmsAreaPerCell(m_family);
            break;
        case LatticeFamily::SimpleCubic:
        case LatticeFamily::BCC:
        case LatticeFamily::FCC:
        case LatticeFamily::Octet:
            m_wallVox = 2.0 * m_c * voxPerCell;                       // strut diameter
            break;
        case LatticeFamily::Honeycomb:
        case LatticeFamily::Reentrant:
            m_wallVox = 2.0 * m_c * numCubes / (m_cells * m_hcW);     // wall thickness
            break;
        default:
            break;   // skeletal / spinodoid: no simple estimate
    }
}

void Lattice::publishPhases()
{
    PhaseAssignment& pa = Parameters::phaseAssignment;
    pa.clear();

    PhaseMaterial solid;
    solid.name = m_material;
    if (!Parameters::materialStiffnessPa(m_material, solid.C)) {
        qWarning().noquote()
            << QString("[Lattice] could not resolve material \"%1\"; the stress solvers "
                       "will fall back to the cubic constants of the selected material")
                   .arg(m_material);
        return;
    }

    // One constituent, one grain (id 1), identity orientation: the material
    // axes are the cube axes, so a transversely isotropic printed polymer has
    // its stiff direction along the build (Z) axis.
    pa.materials = { solid };
    pa.grainPhase.assign(2, 0);
    pa.grainOrientation.assign(2, std::array<double,3>{ 0.0, 0.0, 0.0 });

    qDebug().noquote() << QString("[Lattice] solid phase = %1%2, void = grain id 0")
                              .arg(solid.name, solid.isIsotropic() ? " (isotropic)" : "");
}

void Lattice::locateSeed()
{
    seedPoints.clear();
    for (int x = 0; x < numCubes; ++x)
        for (int y = 0; y < numCubes; ++y)
            for (int z = 0; z < numCubes; ++z)
                if (voxels[x][y][z] == SolidId) {
                    seedPoints.push_back({ x, y, z });
                    return;
                }
    seedPoints.push_back({ 0, 0, 0 });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void Lattice::Initialization(bool /*isWaveGeneration*/)
{
    m_rng.seed(Parameters::seed);
    readParameters();
    buildRotation();
    buildSegments();
    buildWaves();

    numColors = 1;
    m_coarseN = std::min<int>(numCubes, 64);
    m_achieved = 0.0;
    m_c = solveThreshold();
    estimateWall();
    publishPhases();

    seedPoints.clear();
    seedPoints.push_back({ 0, 0, 0 });
    remainingPoints = 0;
    filled_voxels   = 0;
    flags.isDone    = false;
    m_solidCount    = 0;
    m_zFilled       = 0;
    m_step          = 0;
    m_steps         = std::clamp(static_cast<int>(numCubes) / 2, 8, 40);

    Parameters::lattice_achieved_density = m_target;   // refined once rasterized
    Parameters::lattice_wall_voxels      = m_wallVox;

    report();
}

void Lattice::Generate_To_End()
{
    m_solidCount = rasterize(m_c, 0, numCubes);
    m_zFilled    = numCubes;
    finish();
}

void Lattice::Next_Iteration()
{
    if (getDone()) return;

    // Reveal the structure layer by layer along Z, the way a printer builds
    // it. The last step lands on exactly what Generate_To_End() produces.
    ++m_step;
    const int zEnd = (m_step >= m_steps)
                         ? numCubes
                         : static_cast<int>(static_cast<long long>(numCubes) * m_step / m_steps);
    m_solidCount += rasterize(m_c, m_zFilled, zEnd);
    m_zFilled = zEnd;
    ++IterationNumber;
    filled_voxels = static_cast<unsigned int>(m_solidCount);

    if (m_step >= m_steps)
        finish();
}

void Lattice::finish()
{
    const long long total = static_cast<long long>(numCubes) * numCubes * numCubes;
    m_achieved    = static_cast<double>(m_solidCount) / static_cast<double>(total);
    filled_voxels = static_cast<unsigned int>(m_solidCount);
    estimateWall();
    locateSeed();

    Parameters::lattice_achieved_density = m_achieved;
    Parameters::lattice_wall_voxels      = m_wallVox;

    m_step       = m_steps;
    flags.isDone = true;
    reportAchieved();
}

bool Lattice::getDone() const
{
    return flags.isDone;
}

void Lattice::CleanUp()
{
    Parent_Algorithm::CleanUp();
    m_segments.clear();
    m_segments.shrink_to_fit();
    m_waves.clear();
    m_jitterWaves.clear();
    m_solidCount = 0;
    m_zFilled    = 0;
    m_step       = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Reporting
// ─────────────────────────────────────────────────────────────────────────────

void Lattice::report() const
{
    qInfo().noquote()
        << QString("[Lattice] %1: %2 cells per edge (%3 voxels per cell), target relative "
                   "density %4, level c = %5, material %6")
               .arg(latticeFamilyLabel(m_family)).arg(m_cells)
               .arg(static_cast<double>(numCubes) / m_cells, 0, 'f', 1)
               .arg(m_target, 0, 'f', 3).arg(m_c, 0, 'g', 4).arg(m_material);

    if (m_segments2D)
        qInfo().noquote()
            << QString("[Lattice] honeycomb: %1 cells along Y; cells stretched by %2 along Y "
                       "to tile the square cube")
                   .arg(m_cellsY).arg(m_hcStretch, 0, 'f', 3);

    if (m_rotated)
        qInfo().noquote()
            << QString("[Lattice] lattice rotated by (%1, %2, %3) deg about the cube centre; "
                       "the cut is no longer periodic across the cube faces")
                   .arg(m_angles[0] * 180.0 / kPi, 0, 'f', 1)
                   .arg(m_angles[1] * 180.0 / kPi, 0, 'f', 1)
                   .arg(m_angles[2] * 180.0 / kPi, 0, 'f', 1);

    if (m_grading > 0.0 || m_jitter > 0.0)
        qInfo().noquote()
            << QString("[Lattice] thickness grading %1 along Z, jitter %2 (seed %3)")
                   .arg(m_grading, 0, 'f', 2).arg(m_jitter, 0, 'f', 2).arg(Parameters::seed);

    if (m_wallVox > 0.0 && m_wallVox < 6.0)
        qWarning().noquote()
            << QString("[Lattice] estimated wall/strut thickness is only %1 voxels; below ~6 "
                       "the voxel staircase distorts the homogenized stiffness -- raise the "
                       "cube size or the relative density")
                   .arg(m_wallVox, 0, 'f', 1);
}

void Lattice::reportAchieved() const
{
    QString wall = m_wallVox > 0.0
                       ? QString(", estimated wall/strut thickness %1 voxels").arg(m_wallVox, 0, 'f', 1)
                       : QString();
    qInfo().noquote()
        << QString("[Lattice] achieved relative density %1 (target %2)%3")
               .arg(m_achieved, 0, 'f', 4).arg(m_target, 0, 'f', 3).arg(wall);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Plugin registration -- the complete wiring for this algorithm
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<ParamField> latticeSchema()
{
    // No baseAlgorithmSchema(): a lattice has neither nucleation seeds nor a
    // wave; its "points" are the cells and the density.
    std::vector<ParamField> s = {
        { "size", "Cube size", ParamField::Int, 48, 4, 500, {}, "main" },

        { "lattice_family", "Cell family", ParamField::Enum,
          latticeFamilyLabel(LatticeFamily::GyroidSheet), {}, {},
          latticeFamilyLabels(), "main" },

        { "cells_per_edge",   "Cells per edge",            ParamField::Int,    2,    1,    64,   {}, "main" },
        { "relative_density", "Relative density",          ParamField::Double, 0.30, 0.01, 0.99, {}, "main" },
        { "lattice_grading",  "Thickness grading along Z", ParamField::Double, 0.0,  0.0,  0.9,  {}, "main" },
        { "lattice_jitter",   "Thickness jitter",          ParamField::Double, 0.0,  0.0,  1.0,  {}, "main" },

        { "orientation_angle_a", "Rotation about Z (deg)",  ParamField::Double, 0.0, -180.0, 180.0, {}, "main" },
        { "orientation_angle_b", "Rotation about X (deg)",  ParamField::Double, 0.0, -180.0, 180.0, {}, "main" },
        { "orientation_angle_c", "Rotation about Z' (deg)", ParamField::Double, 0.0, -180.0, 180.0, {}, "main" },

        { "is_periodic", "Periodic cell", ParamField::Bool, true, {}, {}, {}, "main" },
    };

    // One constituent: the printed base material. Void is the other phase.
    ParamField mat = materialParamField();
    mat.defValue = QStringLiteral("PETG");
    s.push_back(mat);

    return s;
}

MATVIZ_REGISTER_ALGORITHM(AlgorithmPlugin{
    "Lattice",
    "Periodic cellular metamaterial RVE (TPMS sheets and skeletons, strut "
    "lattices, honeycombs, spinodoids): one solid phase plus void, sized to a "
    "target relative density, with optional thickness grading, jitter and "
    "lattice rotation.",
    /*order=*/ 2,
    latticeSchema(),
    [](const Parameters& p) {
        return std::make_shared<Lattice>(static_cast<short int>(p.getSize()), 1);
    }
});

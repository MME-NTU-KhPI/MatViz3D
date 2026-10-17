#include "lattice.h"

#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <functional>
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

    m_uniformWall = p.getLatticeUniformWall();
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

    m_theta[0] = std::clamp(p.getSpinodoidThetaX(), 0.0, 90.0);
    m_theta[1] = std::clamp(p.getSpinodoidThetaY(), 0.0, 90.0);
    m_theta[2] = std::clamp(p.getSpinodoidThetaZ(), 0.0, 90.0);
    if (m_theta[0] <= 0.0 && m_theta[1] <= 0.0 && m_theta[2] <= 0.0) {
        qWarning() << "[Lattice] all spinodoid cones are closed; using the isotropic field";
        m_theta[0] = m_theta[1] = m_theta[2] = 90.0;
    }

    m_fourierTerms = std::clamp(p.getFourierTerms(), 2, 256);
    m_fourierKmin  = std::max(1.0, p.getFourierKmin());
    m_fourierKmax  = std::max(m_fourierKmin, p.getFourierKmax());
    m_fourierDecay = std::clamp(p.getFourierDecay(), 0.0, 4.0);
    m_fourierFile  = p.getFourierFile();
    m_noiseSmooth  = std::clamp(p.getNoiseSmooth(), 0.005, 0.5);
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
    m_jitterNorm  = 1.0;
    m_directions  = 0;

    std::uniform_real_distribution<double> phase(0.0, 2.0 * kPi);

    // Integer wave vectors (periodic in the cube by construction) with
    // kmin <= |k| <= kmax, one of each +-k pair, filtered by `admissible`.
    auto candidates = [&](double kmin, double kmax,
                          const std::function<bool(int,int,int)>& admissible) {
        std::vector<std::array<int,3>> cand;
        const int K = static_cast<int>(std::ceil(kmax));
        for (int kx = -K; kx <= K; ++kx)
            for (int ky = -K; ky <= K; ++ky)
                for (int kz = -K; kz <= K; ++kz) {
                    if (kx < 0 || (kx == 0 && (ky < 0 || (ky == 0 && kz <= 0)))) continue;
                    const double n = std::sqrt(double(kx * kx + ky * ky + kz * kz));
                    if (n < kmin || n > kmax) continue;
                    if (admissible && !admissible(kx, ky, kz)) continue;
                    cand.push_back({ kx, ky, kz });
                }
        return cand;
    };
    auto drawFrom = [&](const std::vector<std::array<int,3>>& cand, int count,
                        std::vector<Wave>& out, double decay) {
        if (cand.empty()) return;
        std::uniform_int_distribution<size_t> pick(0, cand.size() - 1);
        for (int i = 0; i < count; ++i) {
            const std::array<int,3> k = cand[pick(m_rng)];
            const double n = std::sqrt(double(k[0] * k[0] + k[1] * k[1] + k[2] * k[2]));
            out.push_back({ double(k[0]), double(k[1]), double(k[2]), phase(m_rng),
                            decay > 0.0 ? std::pow(n, -decay) : 1.0 });
        }
    };

    if (m_family == LatticeFamily::Spinodoid) {
        // Kumar, Tan, Zheng & Kochmann (2020): wave directions drawn from the
        // union of cones of half-angle theta_i about the axes; a closed cone
        // (theta = 0) contributes nothing, 90 degrees is the whole sphere.
        const double cx = std::cos(m_theta[0] * kPi / 180.0);
        const double cy = std::cos(m_theta[1] * kPi / 180.0);
        const double cz = std::cos(m_theta[2] * kPi / 180.0);
        auto inCones = [&](int kx, int ky, int kz) {
            const double n = std::sqrt(double(kx * kx + ky * ky + kz * kz));
            const double ax = std::abs(kx) / n, ay = std::abs(ky) / n, az = std::abs(kz) / n;
            return (m_theta[0] > 0.0 && ax >= cx - 1e-12)
                || (m_theta[1] > 0.0 && ay >= cy - 1e-12)
                || (m_theta[2] > 0.0 && az >= cz - 1e-12);
        };
        // Waves per cube edge ~ cells per edge, i.e. one "cell" per wavelength.
        const double beta = static_cast<double>(m_cells);
        std::vector<std::array<int,3>> cand = candidates(0.7 * beta, 1.3 * beta, inCones);
        if (cand.size() < 5) cand = candidates(0.5 * beta, 2.0 * beta, inCones);
        m_directions = cand.size();
        drawFrom(cand, 40, m_waves, 0.0);
        if (m_directions < 3)
            qWarning().noquote()
                << QString("[Lattice] only %1 admissible wave direction(s) for the spinodoid; "
                           "raise cells_per_edge or open the cones")
                       .arg(m_directions);
    }

    if (m_family == LatticeFamily::FourierSheet || m_family == LatticeFamily::FourierSkeletal) {
        bool loaded = false;
        if (!m_fourierFile.isEmpty()) {
            loaded = loadFourierFile(m_fourierFile);
            if (!loaded)
                qWarning().noquote()
                    << QString("[Lattice] could not read Fourier terms from \"%1\"; "
                               "drawing random terms from seed %2 instead")
                           .arg(m_fourierFile).arg(Parameters::seed);
        }
        if (!loaded) {
            std::vector<std::array<int,3>> cand = candidates(m_fourierKmin, m_fourierKmax, nullptr);
            m_directions = cand.size();
            // Distinct wave vectors when there are enough, so two draws never
            // collapse onto one; shuffle and take the first `terms`.
            if (static_cast<int>(cand.size()) >= m_fourierTerms) {
                std::shuffle(cand.begin(), cand.end(), m_rng);
                cand.resize(static_cast<size_t>(m_fourierTerms));
                std::uniform_real_distribution<double> jit(0.5, 1.5);
                for (const std::array<int,3>& k : cand) {
                    const double n = std::sqrt(double(k[0] * k[0] + k[1] * k[1] + k[2] * k[2]));
                    const double a = (m_fourierDecay > 0.0 ? std::pow(n, -m_fourierDecay) : 1.0) * jit(m_rng);
                    m_waves.push_back({ double(k[0]), double(k[1]), double(k[2]), phase(m_rng), a });
                }
            } else {
                drawFrom(cand, m_fourierTerms, m_waves, m_fourierDecay);
            }
            // Unit variance, so the level c means the same thing for every
            // draw. Terms from a file are kept exactly as given: a uniform
            // scale changes c, not the geometry, and the file is the record.
            double var = 0.0;
            for (const Wave& w : m_waves) var += 0.5 * w.amp * w.amp;
            if (var > 0.0) {
                const double s = 1.0 / std::sqrt(var);
                for (Wave& w : m_waves) w.amp *= s;
            }
        }
    }

    if (m_jitter > 0.0)
        drawFrom(candidates(1.0, 2.0 * m_cells, nullptr), 12, m_jitterWaves, 0.0);
}

// Smoothed noise: Gaussian white noise on the full grid, blurred by a
// periodic separable Gaussian of sigma = noise_smooth * N voxels, then scaled
// to unit variance so the level c means the same thing for every draw.
void Lattice::buildNoise()
{
    m_noise.clear();
    if (m_family != LatticeFamily::NoiseSheet && m_family != LatticeFamily::NoiseSkeletal) return;

    const int N = numCubes;
    const std::size_t M = static_cast<std::size_t>(N) * N * N;
    std::vector<float> a(M), b(M);
    std::normal_distribution<double> gauss(0.0, 1.0);
    for (std::size_t i = 0; i < M; ++i) a[i] = static_cast<float>(gauss(m_rng));

    m_noiseSigmaVox = m_noiseSmooth * N;
    const int K = std::max(1, static_cast<int>(std::ceil(3.0 * m_noiseSigmaVox)));
    std::vector<float> w(2 * K + 1);
    double wsum = 0.0;
    for (int d = -K; d <= K; ++d) { w[d + K] = static_cast<float>(std::exp(-0.5 * d * d / (m_noiseSigmaVox * m_noiseSigmaVox))); wsum += w[d + K]; }
    for (float& v : w) v = static_cast<float>(v / wsum);

    auto idx = [N](int x, int y, int z) { return (static_cast<std::size_t>(x) * N + y) * N + z; };
    auto wrapi = [N](int i) { return (i % N + N) % N; };
    // Three periodic 1-D passes; axis 0 = x, 1 = y, 2 = z.
    for (int axis = 0; axis < 3; ++axis) {
        #pragma omp parallel for collapse(2) schedule(static)
        for (int p = 0; p < N; ++p)
            for (int q = 0; q < N; ++q)
                for (int i = 0; i < N; ++i) {
                    double acc = 0.0;
                    for (int d = -K; d <= K; ++d) {
                        const int j = wrapi(i + d);
                        const std::size_t src = axis == 0 ? idx(j, p, q) : axis == 1 ? idx(p, j, q) : idx(p, q, j);
                        acc += w[d + K] * a[src];
                    }
                    const std::size_t dst = axis == 0 ? idx(i, p, q) : axis == 1 ? idx(p, i, q) : idx(p, q, i);
                    b[dst] = static_cast<float>(acc);
                }
        std::swap(a, b);
    }

    double mean = 0.0, var = 0.0;
    for (float v : a) mean += v;
    mean /= static_cast<double>(M);
    for (float v : a) var += (v - mean) * (v - mean);
    var /= static_cast<double>(M);
    const float scale = static_cast<float>(1.0 / std::sqrt(std::max(var, 1e-30)));
    for (float& v : a) v = (v - static_cast<float>(mean)) * scale;
    m_noise = std::move(a);
}

// One term per line: kx ky kz amplitude phase. Blank lines and lines starting
// with '#' are skipped; wave vectors must be integers so the field stays
// periodic in the cube.
bool Lattice::loadFourierFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    std::vector<Wave> terms;
    int lineNo = 0;
    while (!f.atEnd()) {
        ++lineNo;
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        const QStringList parts = line.split(QRegularExpression("[\\s,;]+"), Qt::SkipEmptyParts);
        if (parts.size() < 5) {
            qWarning().noquote() << QString("[Lattice] %1:%2: expected kx ky kz amplitude phase").arg(path).arg(lineNo);
            return false;
        }
        bool ok[5];
        const int    kx = parts[0].toInt(&ok[0]);
        const int    ky = parts[1].toInt(&ok[1]);
        const int    kz = parts[2].toInt(&ok[2]);
        const double a  = parts[3].toDouble(&ok[3]);
        const double ph = parts[4].toDouble(&ok[4]);
        if (!(ok[0] && ok[1] && ok[2] && ok[3] && ok[4])) {
            qWarning().noquote() << QString("[Lattice] %1:%2: could not parse \"%3\"").arg(path).arg(lineNo).arg(line);
            return false;
        }
        if (kx == 0 && ky == 0 && kz == 0) continue;   // a constant only shifts c
        terms.push_back({ double(kx), double(ky), double(kz), ph, a });
    }
    if (terms.empty()) return false;
    m_waves = std::move(terms);
    m_directions = m_waves.size();
    return true;
}

// The waves actually used, for the HDF5 metadata: this is the geometry
// vector a property-to-geometry network would output.
void Lattice::publishWaves()
{
    Parameters::lattice_wave_terms.clear();
    if (m_family == LatticeFamily::Spinodoid ||
        m_family == LatticeFamily::FourierSheet || m_family == LatticeFamily::FourierSkeletal) {
        Parameters::lattice_wave_terms.reserve(m_waves.size());
        for (const Wave& w : m_waves)
            Parameters::lattice_wave_terms.push_back({ float(w.kx), float(w.ky), float(w.kz),
                                                       float(w.amp), float(w.phase) });
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fields
// ─────────────────────────────────────────────────────────────────────────────

bool Lattice::isSheet() const
{
    switch (m_family) {
        case LatticeFamily::GyroidSheet: case LatticeFamily::SchwarzPSheet:
        case LatticeFamily::SchwarzDSheet: case LatticeFamily::IWPSheet:
        case LatticeFamily::FourierSheet:
            return true;
        default:
            return false;
    }
}

// f and grad f (derivatives with respect to the cube-fraction coordinates)
// for the level-set families. |f| / |grad f| is the first-order distance to
// the surface f = 0, which makes a sheet of (nearly) uniform thickness.
bool Lattice::levelSet(double X, double Y, double Z, double& f, double g[3]) const
{
    const double a = 2.0 * kPi * m_cells;
    const double x = a * X, y = a * Y, z = a * Z;
    const double sx = std::sin(x), cx = std::cos(x);
    const double sy = std::sin(y), cy = std::cos(y);
    const double sz = std::sin(z), cz = std::cos(z);
    switch (m_family) {
        case LatticeFamily::GyroidSheet:
        case LatticeFamily::GyroidSkeletal:
            f = sx * cy + sy * cz + sz * cx;
            g[0] = a * (cx * cy - sz * sx);
            g[1] = a * (-sx * sy + cy * cz);
            g[2] = a * (-sy * sz + cz * cx);
            return true;
        case LatticeFamily::SchwarzPSheet:
        case LatticeFamily::SchwarzPSkeletal:
            f = cx + cy + cz;
            g[0] = -a * sx; g[1] = -a * sy; g[2] = -a * sz;
            return true;
        case LatticeFamily::SchwarzDSheet:
        case LatticeFamily::SchwarzDSkeletal:
            f = sx * sy * sz + sx * cy * cz + cx * sy * cz + cx * cy * sz;
            g[0] = a * (cx * sy * sz + cx * cy * cz - sx * sy * cz - sx * cy * sz);
            g[1] = a * (sx * cy * sz - sx * sy * cz + cx * cy * cz - cx * sy * sz);
            g[2] = a * (sx * sy * cz - sx * cy * sz - cx * sy * sz + cx * cy * cz);
            return true;
        case LatticeFamily::IWPSheet:
        case LatticeFamily::IWPSkeletal:
            f = 2.0 * (cx * cy + cy * cz + cz * cx) - (std::cos(2 * x) + std::cos(2 * y) + std::cos(2 * z));
            g[0] = a * (-2.0 * sx * (cy + cz) + 2.0 * std::sin(2 * x));
            g[1] = a * (-2.0 * sy * (cx + cz) + 2.0 * std::sin(2 * y));
            g[2] = a * (-2.0 * sz * (cx + cy) + 2.0 * std::sin(2 * z));
            return true;
        case LatticeFamily::FourierSheet:
        case LatticeFamily::FourierSkeletal: {
            f = 0.0; g[0] = g[1] = g[2] = 0.0;
            for (const Wave& w : m_waves) {
                const double ph = 2.0 * kPi * (w.kx * X + w.ky * Y + w.kz * Z) + w.phase;
                f += w.amp * std::cos(ph);
                const double d = -w.amp * 2.0 * kPi * std::sin(ph);
                g[0] += d * w.kx; g[1] += d * w.ky; g[2] += d * w.kz;
            }
            return true;
        }
        default:
            return false;
    }
}

double Lattice::rawField(double X, double Y, double Z) const
{
    if (m_uniformWall && isSheet()) {
        double f, g[3];
        levelSet(X, Y, Z, f, g);
        const double gn = std::sqrt(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
        // At a critical point on the surface (f = 0, grad f = 0) the ratio is
        // 0/0: that point is on the surface, so it is solid.
        return std::abs(f) / std::max(gn, 1e-12);
    }
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
        case LatticeFamily::FourierSheet:
        case LatticeFamily::FourierSkeletal: {
            double f = 0.0;
            for (const Wave& w : m_waves)
                f += w.amp * std::cos(2.0 * kPi * (w.kx * X + w.ky * Y + w.kz * Z) + w.phase);
            return m_signed ? f : std::abs(f);
        }
        case LatticeFamily::NoiseSheet:
        case LatticeFamily::NoiseSkeletal: {
            // The field lives on the voxel grid: nearest voxel, wrapped.
            if (m_noise.empty()) return 0.0;
            const int N = numCubes;
            auto wrapi = [N](int i) { return (i % N + N) % N; };
            const int ix = wrapi(static_cast<int>(std::floor(X * N)));
            const int iy = wrapi(static_cast<int>(std::floor(Y * N)));
            const int iz = wrapi(static_cast<int>(std::floor(Z * N)));
            const double v = m_noise[(static_cast<std::size_t>(ix) * N + iy) * N + iz];
            return m_signed ? v : std::abs(v);
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

    if (m_uniformWall && isSheet()) {
        m_wallVox = 2.0 * m_c * numCubes;          // c is the half thickness in cube fractions
        return;
    }
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
    buildNoise();
    publishWaves();

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
    checkPrintable();
}

// Printability of the rasterized cube: islands, thin walls, overhangs,
// closed pores (printability.h). Needs a physical scale, which the cell
// size in mm supplies; the report goes to Parameters::printability and from
// there to the log and the HDF5 metadata.
void Lattice::checkPrintable()
{
    const Parameters& p = *Parameters::instance();
    const double voxel_mm = p.getLatticeCellMm() * m_cells / static_cast<double>(numCubes);
    Parameters::lattice_voxel_mm = voxel_mm;

    const PrintabilityReport r = checkPrintability(voxels, numCubes, Parameters::printThresholds(voxel_mm));
    Parameters::printability = r;
    if (!r.computed) return;
    if (m_wallVox <= 0.0 && r.mean_thickness_vox > 0.0) {
        m_wallVox = r.mean_thickness_vox;          // families without an analytic estimate
        Parameters::lattice_wall_voxels = m_wallVox;
    }

    qInfo().noquote()
        << QString("[Lattice] printability (voxel %1 mm, wall ≈ %2 mm): %3 solid component(s), %4 floating island(s), "
                   "largest %5 vox ≈ %6 mm, islands %7 % of the solid (%8 % if the cube is cut out as a box), "
                   "thin walls %9 %, closed pores %10 %, unsupported %11 % along %12 (best of six); %13")
               .arg(voxel_mm, 0, 'f', 3)
               .arg(m_wallVox > 0.0 ? QString::number(m_wallVox * voxel_mm, 'f', 2) : QStringLiteral("?"))
               .arg(r.components)
               .arg(r.islands)
               .arg(static_cast<qulonglong>(r.largest_island_vox))
               .arg(std::cbrt(static_cast<double>(r.largest_island_vox)) * voxel_mm, 0, 'f', 2)
               .arg(r.island_fraction * 100.0, 0, 'f', 2)
               .arg(r.island_fraction_box * 100.0, 0, 'f', 2)
               .arg(r.thin_fraction * 100.0, 0, 'f', 2)
               .arg(r.closed_pore_fraction * 100.0, 0, 'f', 2)
               .arg(r.unsupported_best * 100.0, 0, 'f', 2)
               .arg(PrintabilityReport::directionName(r.best_direction))
               .arg(r.ok ? QStringLiteral("PRINTABLE") : QString("NOT printable: %1").arg(QString::fromStdString(r.verdict)));
    if (!r.thin_check_resolved)
        qWarning().noquote()
            << QString("[Lattice] the thin-wall check needs at least one voxel per half wall; here %1 voxel(s) -- "
                       "raise the cube size or lower lattice_cell_mm")
                   .arg(r.opening_radius_vox, 0, 'f', 2);
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
    m_noise.clear();
    m_noise.shrink_to_fit();
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

    if (m_family == LatticeFamily::Spinodoid)
        qInfo().noquote()
            << QString("[Lattice] spinodoid: cones about X/Y/Z = %1/%2/%3 deg, %4 admissible directions, %5 waves (seed %6)")
                   .arg(m_theta[0], 0, 'f', 0).arg(m_theta[1], 0, 'f', 0).arg(m_theta[2], 0, 'f', 0)
                   .arg(m_directions).arg(m_waves.size()).arg(Parameters::seed);
    if (m_family == LatticeFamily::NoiseSheet || m_family == LatticeFamily::NoiseSkeletal)
        qInfo().noquote()
            << QString("[Lattice] smoothed noise: sigma %1 of the edge = %2 voxels (seed %3)")
                   .arg(m_noiseSmooth, 0, 'g', 3).arg(m_noiseSigmaVox, 0, 'f', 1).arg(Parameters::seed);
    if (m_family == LatticeFamily::FourierSheet || m_family == LatticeFamily::FourierSkeletal)
        qInfo().noquote()
            << QString("[Lattice] Fourier series: %1 waves%2 (|k| in [%3, %4] per edge, decay %5, seed %6)")
                   .arg(m_waves.size())
                   .arg(m_fourierFile.isEmpty() ? QString() : QString(" from \"%1\"").arg(m_fourierFile))
                   .arg(m_fourierKmin, 0, 'g', 3).arg(m_fourierKmax, 0, 'g', 3)
                   .arg(m_fourierDecay, 0, 'g', 3).arg(Parameters::seed);

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

        // The random families (spinodoid, Fourier) and the jitter draw from
        // this; the GUI has no seed field of its own, so expose it here.
        { "seed",             "Seed",                      ParamField::Int,    1,    0,    2147483647, {}, "main" },
        { "cells_per_edge",   "Cells per edge",            ParamField::Int,    2,    1,    64,   {}, "main" },
        { "relative_density", "Relative density",          ParamField::Double, 0.30, 0.01, 0.99, {}, "main" },
        { "lattice_grading",  "Thickness grading along Z", ParamField::Double, 0.0,  0.0,  0.9,  {}, "main" },
        { "lattice_jitter",   "Thickness jitter",          ParamField::Double, 0.0,  0.0,  1.0,  {}, "main" },

        { "orientation_angle_a", "Rotation about Z (deg)",  ParamField::Double, 0.0, -180.0, 180.0, {}, "main" },
        { "orientation_angle_b", "Rotation about X (deg)",  ParamField::Double, 0.0, -180.0, 180.0, {}, "main" },
        { "orientation_angle_c", "Rotation about Z' (deg)", ParamField::Double, 0.0, -180.0, 180.0, {}, "main" },

        { "is_periodic", "Periodic cell", ParamField::Bool, true, {}, {}, {}, "main" },

        // Physical scale and printability thresholds (the fractions are CLI/config only).
        { "lattice_cell_mm",        "Cell size (mm)",               ParamField::Double, 10.0, 0.5, 200.0, {}, "main" },
        { "lattice_uniform_wall",   "Uniform wall thickness (sheets)", ParamField::Bool, false, {}, {}, {}, "main" },
        { "print_min_wall_mm",      "Printable wall, min (mm)",     ParamField::Double, 0.8,  0.1, 5.0,   {}, "main" },
        { "print_max_overhang_deg", "Printable overhang, max (deg)", ParamField::Double, 45.0, 0.0, 89.0,  {}, "main" },

        // Spinodoid: cones of admissible wave directions about the axes.
        { "spinodoid_theta_x", "Spinodoid cone about X (deg, 0 = off)", ParamField::Double, 90.0, 0.0, 90.0, {}, "main" },
        { "spinodoid_theta_y", "Spinodoid cone about Y (deg, 0 = off)", ParamField::Double, 90.0, 0.0, 90.0, {}, "main" },
        { "spinodoid_theta_z", "Spinodoid cone about Z (deg, 0 = off)", ParamField::Double, 90.0, 0.0, 90.0, {}, "main" },

        // Fourier series: random draw controls (a coefficient file is CLI/config only).
        { "fourier_terms", "Fourier waves",                   ParamField::Int,    24,  2,   256,  {}, "main" },
        { "fourier_kmin",  "Fourier min waves per edge",      ParamField::Double, 1.0, 1.0, 32.0, {}, "main" },
        { "fourier_kmax",  "Fourier max waves per edge",      ParamField::Double, 4.0, 1.0, 32.0, {}, "main" },
        { "fourier_decay", "Fourier amplitude decay exponent", ParamField::Double, 1.0, 0.0, 4.0,  {}, "main" },

        // Smoothed noise: Gaussian sigma as a fraction of the cube edge.
        { "noise_smooth",  "Noise smoothing (fraction of edge)", ParamField::Double, 0.06, 0.005, 0.5, {}, "main" },
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
    "lattices, honeycombs, anisotropic spinodoids, open Fourier-series level "
    "sets, smoothed random noise): one solid phase plus void, sized to a target relative density, "
    "with optional thickness grading, jitter and lattice rotation.",
    /*order=*/ 2,
    latticeSchema(),
    [](const Parameters& p) {
        return std::make_shared<Lattice>(static_cast<short int>(p.getSize()), 1);
    }
});

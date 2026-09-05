#include "voronoi.h"
#include "algorithmplugin.h"
#include "parameters.h"
#include "tensormath.hpp"

#include <QDebug>
#include <algorithm>
#include <cmath>
#include <limits>
#include <omp.h>

Voronoi::Voronoi() {}

Voronoi::Voronoi(short int numCubes, int numColors)
{
    this->numCubes  = numCubes;
    this->numColors = numColors;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────────────────────────────────────

void Voronoi::readParameters()
{
    // Clamped rather than rejected: p <= 0 is not a metric at all, and beyond
    // ~20 the |d|^p table overflows for large grids while the tessellation has
    // already converged to Chebyshev.
    m_p = std::clamp(Parameters::instance()->getMinkowskiP(), 0.2, 20.0);
    m_periodic = flags.isPeriodicStructure || Parameters::instance()->getIsPeriodic();

    m_metric.xx = Parameters::instance()->getVoronoiMxx();
    m_metric.yy = Parameters::instance()->getVoronoiMyy();
    m_metric.zz = Parameters::instance()->getVoronoiMzz();
    m_metric.xy = Parameters::instance()->getVoronoiMxy();
    m_metric.yz = Parameters::instance()->getVoronoiMyz();
    m_metric.xz = Parameters::instance()->getVoronoiMxz();

    // Check symmetry and positive-definiteness via eigen-decomposition
    mvt::Sym3 symM{ m_metric.xx, m_metric.yy, m_metric.zz, m_metric.xy, m_metric.yz, m_metric.xz };
    mvt::Eig3 eig = mvt::eigenSym3(symM);

    constexpr double kMinEig = 1e-4;
    bool clamped = false;
    for (int i = 0; i < 3; ++i) {
        if (eig.lambda[i] < kMinEig) {
            eig.lambda[i] = kMinEig;
            clamped = true;
        }
    }

    if (clamped) {
        qWarning() << "[Voronoi] metric tensor is not positive-definite; clamped eigenvalues to" << kMinEig;
        // Reconstruct positive-definite metric tensor: M = R * diag(lambda) * R^T
        m_metric.xx = 0; m_metric.yy = 0; m_metric.zz = 0;
        m_metric.xy = 0; m_metric.yz = 0; m_metric.xz = 0;
        for (int k = 0; k < 3; ++k) {
            const double l = eig.lambda[k];
            const auto& e = eig.e[k];
            m_metric.xx += l * e[0] * e[0];
            m_metric.yy += l * e[1] * e[1];
            m_metric.zz += l * e[2] * e[2];
            m_metric.xy += l * e[0] * e[1];
            m_metric.yz += l * e[1] * e[2];
            m_metric.xz += l * e[0] * e[2];
        }
    }

    // Build transformation matrix A = diag(sqrt(lambda)) * R^T
    // so that A_k,j = sqrt(lambda_k) * e_k[j]
    for (int k = 0; k < 3; ++k) {
        const double s = std::sqrt(eig.lambda[k]);
        for (int j = 0; j < 3; ++j) {
            m_A[k][j] = s * eig.e[k][j];
        }
    }

    const double minLambda = std::min({ eig.lambda[0], eig.lambda[1], eig.lambda[2] });

    // Check if off-diagonal entries are negligible
    const double maxOffDiag = std::max({ std::abs(m_metric.xy), std::abs(m_metric.yz), std::abs(m_metric.xz) });
    const double maxDiag = std::max({ m_metric.xx, m_metric.yy, m_metric.zz });
    m_isDiagonal = (maxOffDiag <= 1e-9 * maxDiag);

    // Check if isotropic (diagonal and equal diagonal entries)
    m_isIsotropic = m_isDiagonal &&
                    std::abs(m_metric.xx - m_metric.yy) <= 1e-9 * maxDiag &&
                    std::abs(m_metric.xx - m_metric.zz) <= 1e-9 * maxDiag;

    // Expanding-ring pruning bound scale
    if (m_isDiagonal) {
        const double minDiag = std::min({ m_metric.xx, m_metric.yy, m_metric.zz });
        m_boundScale = std::pow(minDiag, m_p / 2.0);
    } else {
        const double cp_p = (m_p >= 2.0) ? std::min(1.0, std::pow(3.0, 1.0 - m_p / 2.0)) : 1.0;
        m_boundScale = cp_p * std::pow(minLambda, m_p / 2.0);
    }
}

void Voronoi::buildPowTable()
{
    // Offsets are integer voxel counts, so every |d|^p the solver will ever
    // need can be tabulated once -- that is what keeps an arbitrary real p as
    // cheap as p = 1 in the inner loop.
    m_pow.assign(static_cast<size_t>(numCubes) + 1, 0.0);
    for (int d = 0; d <= numCubes; ++d)
        m_pow[d] = std::pow(static_cast<double>(d), m_p);

    if (m_isDiagonal) {
        const double px = std::pow(m_metric.xx, m_p / 2.0);
        const double py = std::pow(m_metric.yy, m_p / 2.0);
        const double pz = std::pow(m_metric.zz, m_p / 2.0);

        m_powX.assign(static_cast<size_t>(numCubes) + 1, 0.0);
        m_powY.assign(static_cast<size_t>(numCubes) + 1, 0.0);
        m_powZ.assign(static_cast<size_t>(numCubes) + 1, 0.0);

        for (int d = 0; d <= numCubes; ++d) {
            m_powX[d] = m_pow[d] * px;
            m_powY[d] = m_pow[d] * py;
            m_powZ[d] = m_pow[d] * pz;
        }
    } else {
        m_powX.clear();
        m_powY.clear();
        m_powZ.clear();
    }
}

double Voronoi::axisPow(int d) const
{
    if (d < 0) d = -d;
    return (d < static_cast<int>(m_pow.size())) ? m_pow[d]
                                                : std::pow(static_cast<double>(d), m_p);
}

double Voronoi::distanceSp(int dx, int dy, int dz) const
{
    if (m_periodic) {
        const int N = numCubes;
        if (dx > N / 2) dx -= N; else if (dx < -N / 2) dx += N;
        if (dy > N / 2) dy -= N; else if (dy < -N / 2) dy += N;
        if (dz > N / 2) dz -= N; else if (dz < -N / 2) dz += N;
    }

    if (m_isIsotropic) {
        const int ex = std::abs(dx);
        const int ey = std::abs(dy);
        const int ez = std::abs(dz);
        return m_pow[ex] + m_pow[ey] + m_pow[ez];
    }

    if (m_isDiagonal) {
        const int ex = std::abs(dx);
        const int ey = std::abs(dy);
        const int ez = std::abs(dz);
        return m_powX[ex] + m_powY[ey] + m_powZ[ez];
    }

    const double fdx = static_cast<double>(dx);
    const double fdy = static_cast<double>(dy);
    const double fdz = static_cast<double>(dz);

    if (std::abs(m_p - 2.0) < 1e-6) {
        return m_metric.xx * fdx * fdx +
               m_metric.yy * fdy * fdy +
               m_metric.zz * fdz * fdz +
               2.0 * (m_metric.xy * fdx * fdy +
                      m_metric.yz * fdy * fdz +
                      m_metric.xz * fdx * fdz);
    }

    const double xi0 = std::abs(m_A[0][0] * fdx + m_A[0][1] * fdy + m_A[0][2] * fdz);
    const double xi1 = std::abs(m_A[1][0] * fdx + m_A[1][1] * fdy + m_A[1][2] * fdz);
    const double xi2 = std::abs(m_A[2][0] * fdx + m_A[2][1] * fdy + m_A[2][2] * fdz);
    return std::pow(xi0, m_p) + std::pow(xi1, m_p) + std::pow(xi2, m_p);
}

void Voronoi::buildSeedGrid()
{
    const int nSeeds = static_cast<int>(seedPoints.size());

    // Aim for ~1 seed per bucket: the ring search then touches a couple of
    // dozen candidates per voxel regardless of how many seeds there are.
    int nb = std::max(1, static_cast<int>(std::cbrt(static_cast<double>(std::max(1, nSeeds)))));
    nb = std::min(nb, static_cast<int>(numCubes));

    m_grid.cell = std::max(1, (numCubes + nb - 1) / nb);
    m_grid.nb   = (numCubes + m_grid.cell - 1) / m_grid.cell;

    const int nbc = m_grid.nb;
    const size_t nBuckets = static_cast<size_t>(nbc) * nbc * nbc;

    auto bucketOf = [&](const Coordinate& c) {
        const int bx = std::min(c.x / m_grid.cell, nbc - 1);
        const int by = std::min(c.y / m_grid.cell, nbc - 1);
        const int bz = std::min(c.z / m_grid.cell, nbc - 1);
        return (static_cast<size_t>(bx) * nbc + by) * nbc + bz;
    };

    m_grid.start.assign(nBuckets + 1, 0);
    for (const Coordinate& c : seedPoints)
        ++m_grid.start[bucketOf(c) + 1];
    for (size_t i = 0; i < nBuckets; ++i)
        m_grid.start[i + 1] += m_grid.start[i];

    std::vector<int32_t> fill(m_grid.start.begin(), m_grid.start.end() - 1);
    m_grid.items.assign(static_cast<size_t>(nSeeds), 0);
    for (int i = 0; i < nSeeds; ++i)
        m_grid.items[fill[bucketOf(seedPoints[i])]++] = i;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Nearest seed under the L_p metric
// ─────────────────────────────────────────────────────────────────────────────

int32_t Voronoi::nearestSeed(int x, int y, int z, double& sum_p) const
{
    const int nbc  = m_grid.nb;
    const int cell = m_grid.cell;

    const int hx = std::min(x / cell, nbc - 1);
    const int hy = std::min(y / cell, nbc - 1);
    const int hz = std::min(z / cell, nbc - 1);

    double  best   = std::numeric_limits<double>::max();
    int32_t bestId = 0;

    // Rings 0..floor(nb/2) already cover every bucket once wrapping is taken
    // into account, and stopping exactly there also keeps the ring bound below
    // sound: for r <= nb/2 the minimum-image index distance is still r.
    const int maxR = m_periodic ? (nbc / 2) : nbc;

    for (int r = 0; r <= maxR; ++r) {
        // Every bucket whose index distance from home is r sits at least
        // (r-1)*cell voxels away along its dominant axis, and d_p >= that gap
        // for any p > 0. So once gap^p exceeds the best sum found, no further
        // ring can improve on it.
        if (r >= 2 && bestId != 0 && (m_boundScale * axisPow((r - 1) * cell)) > best)
            break;

        for (int dx = -r; dx <= r; ++dx) {
            const bool edgeX = (dx == -r || dx == r);
            for (int dy = -r; dy <= r; ++dy) {
                const bool edgeY = (dy == -r || dy == r);
                for (int dz = -r; dz <= r; ++dz) {
                    // Shell only: the interior was covered by a smaller r.
                    if (!edgeX && !edgeY && (dz != -r && dz != r))
                        continue;

                    int bx = hx + dx, by = hy + dy, bz = hz + dz;
                    if (m_periodic) {
                        bx = ((bx % nbc) + nbc) % nbc;
                        by = ((by % nbc) + nbc) % nbc;
                        bz = ((bz % nbc) + nbc) % nbc;
                    } else if (bx < 0 || bx >= nbc || by < 0 || by >= nbc ||
                               bz < 0 || bz >= nbc) {
                        continue;
                    }

                    const size_t b = (static_cast<size_t>(bx) * nbc + by) * nbc + bz;
                    for (int32_t k = m_grid.start[b]; k < m_grid.start[b + 1]; ++k) {
                        const Coordinate& s = seedPoints[m_grid.items[k]];
                        const double s_p = distanceSp(x - s.x, y - s.y, z - s.z);
                        if (s_p < best) {
                            best   = s_p;
                            bestId = m_grid.items[k] + 1;   // grain ids are 1-based
                        }
                    }
                }
            }
        }
    }

    sum_p = best;
    return bestId;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Generation
// ─────────────────────────────────────────────────────────────────────────────

void Voronoi::Initialization(bool /*isWaveGeneration*/)
{
    m_rng.seed(Parameters::seed);
    readParameters();

    // Exactly numColors distinct seeds, so grain ids run 1..numColors with no
    // gaps. The base class's Random_Generate_Points() silently drops duplicate
    // draws, which leaves the orientation table (indexed by grain id) longer
    // than the structure actually uses.
    const long long capacity = static_cast<long long>(numCubes) * numCubes * numCubes;
    const int target = static_cast<int>(std::min<long long>(numColors, capacity));

    long long attempts = 0;
    const long long maxAttempts = 100LL * target + 1000LL;
    while (static_cast<int>(seedPoints.size()) < target && attempts++ < maxAttempts) {
        Coordinate c = randomCoord();
        if (voxels[c.x][c.y][c.z] == 0)
            birthGrain(c.x, c.y, c.z);
    }

    // Densely seeded grids (points approaching size^3) make rejection sampling
    // stall; finish deterministically from a shuffled list of the free voxels.
    if (static_cast<int>(seedPoints.size()) < target) {
        std::vector<Coordinate> free;
        free.reserve(static_cast<size_t>(target - seedPoints.size()) * 2);
        for (int x = 0; x < numCubes; ++x)
            for (int y = 0; y < numCubes; ++y)
                for (int z = 0; z < numCubes; ++z)
                    if (voxels[x][y][z] == 0) free.push_back({x, y, z});

        std::shuffle(free.begin(), free.end(), m_rng);
        for (size_t i = 0; i < free.size() && static_cast<int>(seedPoints.size()) < target; ++i)
            birthGrain(free[i].x, free[i].y, free[i].z);
    }

    remainingPoints = 0;
    numColors = static_cast<int>(seedPoints.size());

    buildPowTable();
    buildSeedGrid();
    verifyAgainstBruteForce();

    qDebug().noquote()
        << QString("[Voronoi] %1^3 grid, %2 seeds, Minkowski p = %3, metric: diag(%4, %5, %6) offdiag(%7, %8, %9)%10")
               .arg(numCubes).arg(seedPoints.size())
               .arg(m_p, 0, 'g', 4)
               .arg(m_metric.xx, 0, 'g', 3)
               .arg(m_metric.yy, 0, 'g', 3)
               .arg(m_metric.zz, 0, 'g', 3)
               .arg(m_metric.xy, 0, 'g', 3)
               .arg(m_metric.yz, 0, 'g', 3)
               .arg(m_metric.xz, 0, 'g', 3)
               .arg(m_periodic ? ", periodic" : "");
}

// Set MATVIZ_VORONOI_VERIFY=1 to cross-check the bucket-accelerated search
// against a brute-force scan over every seed on a random sample of voxels.
// Distances are compared rather than ids: two seeds exactly equidistant from a
// voxel are both correct answers and the two searches may pick either.
void Voronoi::verifyAgainstBruteForce() const
{
    if (qEnvironmentVariableIntValue("MATVIZ_VORONOI_VERIFY") <= 0)
        return;

    const int N = numCubes;
    const long long nVox = static_cast<long long>(N) * N * N;
    const int nSamples = static_cast<int>(std::min<long long>(2000, nVox));

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> pick(0, N - 1);

    int mismatches = 0;
    double worst = 0.0;

    for (int i = 0; i < nSamples; ++i) {
        const int x = pick(rng), y = pick(rng), z = pick(rng);

        double fast = 0.0;
        nearestSeed(x, y, z, fast);

        double brute = std::numeric_limits<double>::max();
        for (const Coordinate& s : seedPoints) {
            brute = std::min(brute, distanceSp(x - s.x, y - s.y, z - s.z));
        }

        const double rel = std::abs(fast - brute) / std::max(1e-12, brute);
        if (rel > 1e-12) {
            ++mismatches;
            worst = std::max(worst, rel);
        }
    }

    if (mismatches == 0)
        qInfo() << "[Voronoi] verify: bucket search matches brute force on"
                << nSamples << "sampled voxels";
    else
        qCritical() << "[Voronoi] verify: " << mismatches << "of" << nSamples
                    << "voxels disagree with brute force, worst relative error" << worst;
}

// Second half of the same debug hook: proves the banded reveal lands on the
// exact tessellation, i.e. that animating costs nothing in accuracy. Every
// voxel must be filled and must sit at the minimum distance -- compared as a
// distance again, since equidistant seeds are interchangeable.
void Voronoi::verifyRevealedGrid() const
{
    if (qEnvironmentVariableIntValue("MATVIZ_VORONOI_VERIFY") <= 0)
        return;

    const int N = numCubes;
    int unfilled = 0, wrong = 0;

    for (int x = 0; x < N; ++x)
        for (int y = 0; y < N; ++y)
            for (int z = 0; z < N; ++z) {
                const int32_t id = voxels[x][y][z];
                if (id <= 0 || id > static_cast<int32_t>(seedPoints.size())) {
                    ++unfilled;
                    continue;
                }

                double best = 0.0;
                nearestSeed(x, y, z, best);

                const Coordinate& s = seedPoints[id - 1];
                if (distanceSp(x - s.x, y - s.y, z - s.z) > best * (1.0 + 1e-12))
                    ++wrong;
            }

    if (unfilled == 0 && wrong == 0)
        qInfo() << "[Voronoi] verify: banded reveal reproduced the exact tessellation";
    else
        qCritical() << "[Voronoi] verify: banded reveal left" << unfilled
                    << "voxels unfilled and misassigned" << wrong;
}

void Voronoi::tessellateDirect()
{
    const int N = numCubes;

    #pragma omp parallel for collapse(2) schedule(static)
    for (int x = 0; x < N; ++x) {
        for (int y = 0; y < N; ++y) {
            for (int z = 0; z < N; ++z) {
                double d = 0.0;
                voxels[x][y][z] = nearestSeed(x, y, z, d);
            }
        }
    }

    filled_voxels = static_cast<unsigned int>(static_cast<long long>(N) * N * N);
    IterationNumber = 1;
}

void Voronoi::Generate_To_End()
{
    if (seedPoints.empty()) {
        qCritical() << "[Voronoi] no seed points were placed; nothing to tessellate";
        flags.isDone = true;
        return;
    }

    tessellateDirect();
    flags.isDone = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Animated reveal
// ─────────────────────────────────────────────────────────────────────────────

void Voronoi::prepareWaves()
{
    const int N = numCubes;
    const size_t nVox = static_cast<size_t>(N) * N * N;

    m_label.assign(nVox, 0);
    std::vector<float> dist(nVox, 0.0f);

    float dmax = 0.0f;
    const double invP = 1.0 / m_p;

    #pragma omp parallel for collapse(2) schedule(static) reduction(max:dmax)
    for (int x = 0; x < N; ++x) {
        for (int y = 0; y < N; ++y) {
            for (int z = 0; z < N; ++z) {
                double s_p = 0.0;
                const int32_t id = nearestSeed(x, y, z, s_p);
                const size_t idx = (static_cast<size_t>(x) * N + y) * N + z;
                m_label[idx] = id;
                // Bands are cut on the true d_p, not on the un-rooted sum, so
                // the fronts advance at a visually uniform rate.
                const float d = static_cast<float>(std::pow(s_p, invP));
                dist[idx] = d;
                if (d > dmax) dmax = d;
            }
        }
    }

    const int nBands = std::clamp(static_cast<int>(numCubes), 8, 120);
    const double scale = (dmax > 0.0f) ? (nBands / static_cast<double>(dmax)) : 0.0;

    auto bandOf = [&](float d) {
        int b = static_cast<int>(d * scale);
        return std::clamp(b, 0, nBands - 1);
    };

    // Counting sort of the voxels into distance bands.
    std::vector<uint32_t> count(static_cast<size_t>(nBands) + 1, 0);
    for (size_t i = 0; i < nVox; ++i)
        ++count[bandOf(dist[i]) + 1];
    for (int b = 0; b < nBands; ++b)
        count[b + 1] += count[b];

    m_bandEnd.assign(count.begin() + 1, count.end());

    std::vector<uint32_t> fill(count.begin(), count.end() - 1);
    m_order.assign(nVox, 0);
    for (size_t i = 0; i < nVox; ++i)
        m_order[fill[bandOf(dist[i])]++] = static_cast<uint32_t>(i);

    m_cursor    = 0;
    m_band      = 0;
    m_prepared  = true;
    filled_voxels = 0;
}

void Voronoi::Next_Iteration()
{
    if (getDone()) return;

    if (!m_prepared) {
        if (seedPoints.empty()) {
            qCritical() << "[Voronoi] no seed points were placed; nothing to tessellate";
            flags.isDone = true;
            return;
        }
        prepareWaves();
    }

    const int N = numCubes;
    const size_t end = m_bandEnd[m_band];

    for (size_t i = m_cursor; i < end; ++i) {
        const uint32_t idx = m_order[i];
        const int z = static_cast<int>(idx % N);
        const int y = static_cast<int>((idx / N) % N);
        const int x = static_cast<int>(idx / (static_cast<size_t>(N) * N));
        voxels[x][y][z] = m_label[idx];
    }

    m_cursor = end;
    ++m_band;
    ++IterationNumber;
    filled_voxels = static_cast<unsigned int>(m_cursor);

    if (m_band >= m_bandEnd.size())
        verifyRevealedGrid();

    qDebug().noquote()
        << QString("%1 %2 %3")
               .arg(static_cast<double>(m_cursor) / static_cast<double>(m_order.size()), -12, 'g', 6)
               .arg(IterationNumber, -6)
               .arg(static_cast<int>(m_band), -10);
}

bool Voronoi::getDone() const
{
    if (flags.isDone) return true;
    if (m_prepared)   return m_band >= m_bandEnd.size();
    return false;
}

void Voronoi::CleanUp()
{
    Parent_Algorithm::CleanUp();

    m_grid = SeedGrid{};
    m_pow.clear();
    m_powX.clear();
    m_powY.clear();
    m_powZ.clear();
    m_label.clear();
    m_label.shrink_to_fit();
    m_order.clear();
    m_order.shrink_to_fit();
    m_bandEnd.clear();
    m_cursor   = 0;
    m_band     = 0;
    m_prepared = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Plugin registration -- the complete wiring for this algorithm
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<ParamField> voronoiSchema()
{
    // Deliberately not baseAlgorithmSchema(): Voronoi has no nucleation wave,
    // so the wave coefficient/spread fields would be dead controls.
    std::vector<ParamField> s = {
        { "size",        "Cube size",     ParamField::Int,    10,  1, 500,    {}, "main" },
        { "points",      "Points",        ParamField::PointsMode, 10, 1, 100000,
         { "Size", "Concentration" }, "main" },

        { "minkowski_p", "Minkowski p",   ParamField::Double, 2.0, 0.2, 20.0,  {}, "main" },
        { "is_periodic", "Periodic cell", ParamField::Bool,   false, {}, {},   {}, "main" },

        { "voronoi_metric_preset", "Shape / Metric preset", ParamField::Enum, "Sphere (Circle)", {}, {},
         { "Sphere (Circle)", "Prolate (Needle)", "Oblate (Disc)", "Triaxial Ellipsoid", "Superellipsoid (Cube)",
           "Columnar (Z-axis)", "Columnar (X-axis)", "Rolled (Orthotropic)", "Sheared (45° XY)", "Custom" },
         "main", /*invokeMethod=*/ "setVoronoiMetricPreset" },

        { "voronoi_mxx", "Metric M_xx",   ParamField::Double, 1.0, 0.001, 1000.0, {}, "main" },
        { "voronoi_myy", "Metric M_yy",   ParamField::Double, 1.0, 0.001, 1000.0, {}, "main" },
        { "voronoi_mzz", "Metric M_zz",   ParamField::Double, 1.0, 0.001, 1000.0, {}, "main" },
        { "voronoi_mxy", "Metric M_xy",   ParamField::Double, 0.0, -500.0, 500.0, {}, "main" },
        { "voronoi_myz", "Metric M_yz",   ParamField::Double, 0.0, -500.0, 500.0, {}, "main" },
        { "voronoi_mxz", "Metric M_xz",   ParamField::Double, 0.0, -500.0, 500.0, {}, "main" },
    };

    s.push_back(materialParamField());

    const std::vector<ParamField> tex = textureParamFields();
    s.insert(s.end(), tex.begin(), tex.end());

    return s;
}

MATVIZ_REGISTER_ALGORITHM(AlgorithmPlugin{
    "Voronoi",
    "Voronoi tessellation under a Minkowski L_p metric and 3D Riemannian metric tensor "
    "(p = 1 octahedral, 2 Euclidean, large p cuboidal; metric tensor controls anisotropic grain elongation and orientation), "
    "with material and texture selection.",
    /*order=*/ 0,
    voronoiSchema(),
    [](const Parameters& p) {
        return std::make_shared<Voronoi>(static_cast<short int>(p.getSize()), p.getPoints());
    }
});

#include "probability_algorithm.h"
#include "algorithmplugin.h"
#include "parameters.h"
#include "grain_analyzer.h"
#include <random>
#include <cmath>
#include <omp.h>
#include <QString>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <algorithm>

const std::array<std::array<int32_t, 3>, 26> PROBABILITY_OFFSETS = {{
    {-1, -1, -1}, {-1, -1, 0}, {-1, -1, 1},
    {-1, 0, -1},  {-1, 0, 0},  {-1, 0, 1},
    {-1, 1, -1},  {-1, 1, 0},  {-1, 1, 1},
    {0, -1, -1},  {0, -1, 0},  {0, -1, 1},
    {0, 0, -1},                {0, 0, 1},
    {0, 1, -1},   {0, 1, 0},   {0, 1, 1},
    {1, -1, -1},  {1, -1, 0},  {1, -1, 1},
    {1, 0, -1},   {1, 0, 0},   {1, 0, 1},
    {1, 1, -1},   {1, 1, 0},   {1, 1, 1}
}};

// Deterministic 64-bit integer hash mapped to [0.0, 1.0)
// Guarantees exact reproducibility independent of thread count and thread scheduling.
static inline double hashDice(uint32_t seed, uint32_t step, int32_t x, int32_t y, int32_t z, int32_t offset_idx)
{
    uint64_t h = seed;
    h ^= (static_cast<uint64_t>(step) * 0x9e3779b97f4a7c15ULL);
    h ^= (static_cast<uint64_t>(x) + (static_cast<uint64_t>(y) << 10) + (static_cast<uint64_t>(z) << 20));
    h ^= (static_cast<uint64_t>(offset_idx) * 0x517cc1b727220a95ULL);
    h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
    h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
    h ^= (h >> 31);
    return (h & 0xFFFFFFFFFFFFULL) * (1.0 / static_cast<double>(0x1000000000000ULL));
}

Probability_Algorithm::Probability_Algorithm()
    : Parent_Algorithm()
{
}

Probability_Algorithm::Probability_Algorithm(short int numCubes, int numColors)
    : Parent_Algorithm()
{
    this->numCubes = numCubes;
    this->numColors = numColors;
}

Probability_Algorithm::~Probability_Algorithm()
{
    if (m_claimGrid) {
        Delete3D(m_claimGrid);
        m_claimGrid = nullptr;
    }
}

void Probability_Algorithm::setNumCubes(short int size)
{
    numCubes = size;
}

void Probability_Algorithm::setNumColors(int points)
{
    numColors = points;
}

double Probability_Algorithm::toRadians(double degrees)
{
    return degrees * (M_PI / 180.0);
}

void Probability_Algorithm::rotatePoint(double& x, double& y, double& z)
{
    const double rad_a = toRadians(Parameters::orientation_angle_a);
    const double rad_b = toRadians(Parameters::orientation_angle_b);
    const double rad_c = toRadians(Parameters::orientation_angle_c);

    if (std::abs(rad_a) < 1e-9 && std::abs(rad_b) < 1e-9 && std::abs(rad_c) < 1e-9) {
        return;
    }

    // Global-to-local transformation: R^T = R_x(-rad_a) * R_y(-rad_b) * R_z(-rad_c)
    // 1. Rotate around Z by -rad_c
    const double cos_c = std::cos(rad_c);
    const double sin_c = std::sin(rad_c);
    double x1 =  cos_c * x + sin_c * y;
    double y1 = -sin_c * x + cos_c * y;
    double z1 =  z;

    // 2. Rotate around Y by -rad_b
    const double cos_b = std::cos(rad_b);
    const double sin_b = std::sin(rad_b);
    double x2 =  cos_b * x1 - sin_b * z1;
    double y2 =  y1;
    double z2 =  sin_b * x1 + cos_b * z1;

    // 3. Rotate around X by -rad_a
    const double cos_a = std::cos(rad_a);
    const double sin_a = std::sin(rad_a);
    x = x2;
    y =  cos_a * y2 + sin_a * z2;
    z = -sin_a * y2 + cos_a * z2;
}

bool Probability_Algorithm::isPointIn(double x, double y, double z)
{
    double dx = x - 1.5;
    double dy = y - 1.5;
    double dz = z - 1.5;
    rotatePoint(dx, dy, dz);

    const double a = std::max(0.01f, Parameters::halfaxis_a);
    const double b = std::max(0.01f, Parameters::halfaxis_b);
    const double c = std::max(0.01f, Parameters::halfaxis_c);
    const double p = std::max(0.1, Parameters::ellipse_order);

    if (std::abs(p - 2.0) < 1e-6) {
        return (std::pow(dx / a, 2.0) + std::pow(dy / b, 2.0) + std::pow(dz / c, 2.0)) <= 1.0;
    } else {
        return (std::pow(std::abs(dx / a), p) + std::pow(std::abs(dy / b), p) + std::pow(std::abs(dz / c), p)) <= 1.0;
    }
}

void Probability_Algorithm::calculateVolumeProbabilities()
{
    const double a = std::max(0.01f, Parameters::halfaxis_a);
    const double b = std::max(0.01f, Parameters::halfaxis_b);
    const double c = std::max(0.01f, Parameters::halfaxis_c);
    const double p = std::max(0.1, Parameters::ellipse_order);
    const double gamma = 3.0;

    double raw_prob[26] = {0.0};
    double max_prob = 0.0;

    for (size_t o = 0; o < PROBABILITY_OFFSETS.size(); ++o) {
        double dx = PROBABILITY_OFFSETS[o][0];
        double dy = PROBABILITY_OFFSETS[o][1];
        double dz = PROBABILITY_OFFSETS[o][2];
        double length = std::sqrt(dx * dx + dy * dy + dz * dz);

        // Transform global neighbor direction into local grain crystallographic frame
        double lx = dx, ly = dy, lz = dz;
        rotatePoint(lx, ly, lz);

        // Radial reach along offset direction to superellipsoid boundary:
        // |lx/a|^p + |ly/b|^p + |lz/c|^p = (1/r)^p => r = (sum_p)^(-1/p)
        double sum_p = std::pow(std::abs(lx / a), p)
                     + std::pow(std::abs(ly / b), p)
                     + std::pow(std::abs(lz / c), p);
        if (sum_p < 1e-30) continue;
        double r = 1.0 / std::pow(sum_p, 1.0 / p);

        // Lattice propagation velocity: r is R_boundary / length.
        // Effective CA rate = R_boundary / length^2 = r / length.
        raw_prob[o] = r / length;
        max_prob = std::max(max_prob, raw_prob[o]);
    }

    if (max_prob < 1e-30) max_prob = 1.0;

    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                this->probability[i][j][k] = 0.0;

    for (size_t o = 0; o < PROBABILITY_OFFSETS.size(); ++o) {
        int i = 1 + PROBABILITY_OFFSETS[o][0];
        int j = 1 + PROBABILITY_OFFSETS[o][1];
        int k = 1 + PROBABILITY_OFFSETS[o][2];
        double u_norm = raw_prob[o] / max_prob;
        this->probability[i][j][k] = std::pow(u_norm, gamma);
    }

    this->probability[1][1][1] = 0.0;
}

void Probability_Algorithm::calculateSurfaceFluxProbabilities()
{
    const uint64_t N_surface = 100000;
    const double a = std::max(0.01f, Parameters::halfaxis_a);
    const double b = std::max(0.01f, Parameters::halfaxis_b);
    const double c = std::max(0.01f, Parameters::halfaxis_c);
    const double p = std::max(0.1, Parameters::ellipse_order);
    const double gamma = 3.0;

    double sum_proj_t[26] = {0.0};
    uint64_t count[26] = {0};

    // Precalculate normalized neighbor offsets and Euclidean distances
    double norm_offsets[26][3];
    double lengths[26];
    for (size_t o = 0; o < 26; ++o) {
        double dx = PROBABILITY_OFFSETS[o][0];
        double dy = PROBABILITY_OFFSETS[o][1];
        double dz = PROBABILITY_OFFSETS[o][2];
        lengths[o] = std::sqrt(dx * dx + dy * dy + dz * dz);
        norm_offsets[o][0] = dx / lengths[o];
        norm_offsets[o][1] = dy / lengths[o];
        norm_offsets[o][2] = dz / lengths[o];
    }

    std::mt19937_64 rng(Parameters::seed);
    std::uniform_real_distribution<double> uni(-1.0, 1.0);

    for (uint64_t s = 0; s < N_surface; ++s) {
        double gx, gy, gz, len2;
        do {
            gx = uni(rng); gy = uni(rng); gz = uni(rng);
            len2 = gx * gx + gy * gy + gz * gz;
        } while (len2 < 1e-12 || len2 > 1.0);

        double inv_len = 1.0 / std::sqrt(len2);
        gx *= inv_len; gy *= inv_len; gz *= inv_len;

        // Transform global ray direction into the grain's local crystallographic frame
        double lx = gx, ly = gy, lz = gz;
        rotatePoint(lx, ly, lz);

        // Radial distance to the boundary of the superellipsoid along this ray
        double sum_p = std::pow(std::abs(lx / a), p)
                     + std::pow(std::abs(ly / b), p)
                     + std::pow(std::abs(lz / c), p);
        if (sum_p < 1e-30) continue;
        double t = std::pow(sum_p, -1.0 / p);

        // Find the neighbor offset in global space that aligns best with this ray (Voronoi cone partition)
        int best_idx = 0;
        double max_dot = -2.0;
        for (int o = 0; o < 26; ++o) {
            double dot = gx * norm_offsets[o][0] + gy * norm_offsets[o][1] + gz * norm_offsets[o][2];
            if (dot > max_dot) {
                max_dot = dot;
                best_idx = o;
            }
        }

        sum_proj_t[best_idx] += t * max_dot;
        count[best_idx]++;
    }

    double max_prob = 0.0;
    double raw_prob[26] = {0.0};
    for (size_t o = 0; o < 26; ++o) {
        if (count[o] > 0) {
            double avg_proj_t = sum_proj_t[o] / count[o];
            raw_prob[o] = avg_proj_t / (lengths[o] * lengths[o]);
            max_prob = std::max(max_prob, raw_prob[o]);
        }
    }

    if (max_prob < 1e-30) max_prob = 1.0;

    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                this->probability[i][j][k] = 0.0;

    for (size_t o = 0; o < 26; ++o) {
        int i = 1 + PROBABILITY_OFFSETS[o][0];
        int j = 1 + PROBABILITY_OFFSETS[o][1];
        int k = 1 + PROBABILITY_OFFSETS[o][2];
        double u_norm = raw_prob[o] / max_prob;
        this->probability[i][j][k] = std::pow(u_norm, gamma);
    }

    this->probability[1][1][1] = 0.0;
}

void Probability_Algorithm::processProbabilities(ProbabilityMode mode)
{
    if (mode == ProbabilityMode::SurfaceFlux) {
        calculateSurfaceFluxProbabilities();
    } else {
        calculateVolumeProbabilities();
    }
}

void Probability_Algorithm::Initialization(bool isWaveGeneration)
{
    flags.isPeriodicStructure = flags.isPeriodicStructure || Parameters::instance()->getIsPeriodic();
    flags.isWaveGeneration    = isWaveGeneration || Parameters::is_wave_generation;

    Parent_Algorithm::Initialization(flags.isWaveGeneration);
    run_start = std::chrono::steady_clock::now();
    m_history.clear();
    IterationNumber = 0;
    m_coolingPool = 0.0;
    total_nucleated_so_far = static_cast<int>(seedPoints.size());

    if (m_claimGrid) {
        Delete3D(m_claimGrid);
        m_claimGrid = nullptr;
    }
    m_claimGrid = Create3D<int32_t>(numCubes, numCubes, numCubes);
#pragma omp parallel for collapse(3)
    for (int i = 0; i < numCubes; ++i)
        for (int j = 0; j < numCubes; ++j)
            for (int k = 0; k < numCubes; ++k)
                m_claimGrid[i][j][k] = 0;

    const QString modeStr = Parameters::prob_matrix_mode.trimmed().toLower();
    if (modeStr == "surface flux" || modeStr == "surface" || modeStr == "surface_flux") {
        processProbabilities(ProbabilityMode::SurfaceFlux);
    } else {
        processProbabilities(ProbabilityMode::VolumeSampling);
    }
    printProbabilityKernel();

    qDebug().noquote()
        << QString("[Probability] %1^3 grid (%2 voxels), %3 initial seeds (target: %4), matrix: '%5', preset: '%6' (a=%7, b=%8, c=%9, order=%10, St=%11)%12%13")
               .arg(numCubes)
               .arg(static_cast<uint64_t>(numCubes) * numCubes * numCubes)
               .arg(seedPoints.size())
               .arg(numColors)
               .arg(Parameters::prob_matrix_mode)
               .arg(Parameters::prob_preset)
               .arg(Parameters::halfaxis_a, 0, 'g', 3)
               .arg(Parameters::halfaxis_b, 0, 'g', 3)
               .arg(Parameters::halfaxis_c, 0, 'g', 3)
               .arg(Parameters::ellipse_order, 0, 'g', 3)
               .arg(Parameters::stefan_number, 0, 'g', 3)
               .arg(flags.isPeriodicStructure ? ", periodic" : "")
               .arg(flags.isWaveGeneration ? QString(", wave nucl (peak=%1, end=%2)")
                                                 .arg(Parameters::wave_peak_fraction, 0, 'f', 2)
                                                 .arg(Parameters::wave_end_fraction, 0, 'f', 2) : "");
}

unsigned int Probability_Algorithm::computeThermodynamicCap(unsigned int counter_max)
{
    const float St = Parameters::stefan_number;
    if (St <= 0.0f) return counter_max;

    const double deltaQ = static_cast<double>(counter_max) / static_cast<double>(St);
    m_coolingPool += deltaQ;

    const unsigned int cap = static_cast<unsigned int>(
        std::max(1.0, std::floor(m_coolingPool)));
    return cap;
}

void Probability_Algorithm::partialShuffle(size_t active_size)
{
    const size_t fs = grains.size();
    if (fs <= 1 || active_size == 0) return;
    active_size = std::min(active_size, fs);

    std::mt19937 rng(Parameters::seed ^ (IterationNumber * 2246822519u));

    for (size_t i = 0; i < active_size; ++i)
    {
        const size_t range = fs - i;
        const size_t j     = i + (static_cast<size_t>(rng()) % range);
        if (i != j)
            std::swap(grains[i], grains[j]);
    }
}

unsigned int Probability_Algorithm::growFrontier(unsigned int maxCaptures, size_t active_size)
{
    Q_UNUSED(maxCaptures);
    const size_t frontier_size = grains.size();
    active_size = std::min(active_size, frontier_size);

    const int nthreads = std::max(1, omp_get_max_threads());

    std::vector<std::vector<Coordinate>> threadNewGrains(nthreads);
    std::vector<std::vector<Coordinate>> threadActiveGrains(nthreads);
    std::vector<unsigned int> threadCaptures(nthreads, 0);

    for (int t = 0; t < nthreads; ++t) {
        threadNewGrains[t].reserve(active_size * 4 / nthreads + 16);
        threadActiveGrains[t].reserve(active_size * 2 / nthreads + 16);
    }

#pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        unsigned int local_captures = 0;
        auto& newFrontier = threadNewGrains[tid];
        auto& keepActive  = threadActiveGrains[tid];

        // Phase 1: Record deterministic atomic claims on empty neighbour voxels
#pragma omp for schedule(static)
        for (size_t i = 0; i < active_size; i++)
        {
            const Coordinate& cell     = grains[i];
            const int32_t     x        = cell.x;
            const int32_t     y        = cell.y;
            const int32_t     z        = cell.z;
            const int32_t     grain_id = voxels[x][y][z];

            for (size_t off_idx = 0; off_idx < PROBABILITY_OFFSETS.size(); ++off_idx)
            {
                const auto& offset = PROBABILITY_OFFSETS[off_idx];
                int32_t nx = x + offset[0];
                int32_t ny = y + offset[1];
                int32_t nz = z + offset[2];

                if (flags.isPeriodicStructure)
                {
                    nx = (nx + numCubes) % numCubes;
                    ny = (ny + numCubes) % numCubes;
                    nz = (nz + numCubes) % numCubes;
                }

                if (nx < 0 || ny < 0 || nz < 0 ||
                    nx >= numCubes || ny >= numCubes || nz >= numCubes)
                    continue;

                if (voxels[nx][ny][nz] != 0)
                    continue;

                const double prob =
                    probability[1 + offset[0]][1 + offset[1]][1 + offset[2]];

                if (prob < 1e-12)
                    continue;

                const double roll = hashDice(Parameters::seed, IterationNumber, x, y, z, static_cast<int32_t>(off_idx));
                if (roll >= prob)
                    continue;

                // Deterministic lowest grain ID atomic resolution:
                int32_t cur = m_claimGrid[nx][ny][nz];
                while (cur == 0 || grain_id < cur)
                {
                    if (__sync_bool_compare_and_swap(&m_claimGrid[nx][ny][nz], cur, grain_id))
                        break;
                    cur = m_claimGrid[nx][ny][nz];
                }
            }
        }

#pragma omp barrier

        // Phase 2: Apply winning claims and assemble next active frontier
#pragma omp for schedule(static)
        for (size_t i = 0; i < active_size; i++)
        {
            const Coordinate& cell     = grains[i];
            const int32_t     x        = cell.x;
            const int32_t     y        = cell.y;
            const int32_t     z        = cell.z;
            const int32_t     grain_id = voxels[x][y][z];

            bool has_empty_neighbor = false;

            for (size_t off_idx = 0; off_idx < PROBABILITY_OFFSETS.size(); ++off_idx)
            {
                const auto& offset = PROBABILITY_OFFSETS[off_idx];
                int32_t nx = x + offset[0];
                int32_t ny = y + offset[1];
                int32_t nz = z + offset[2];

                if (flags.isPeriodicStructure)
                {
                    nx = (nx + numCubes) % numCubes;
                    ny = (ny + numCubes) % numCubes;
                    nz = (nz + numCubes) % numCubes;
                }

                if (nx < 0 || ny < 0 || nz < 0 ||
                    nx >= numCubes || ny >= numCubes || nz >= numCubes)
                    continue;

                if (voxels[nx][ny][nz] == 0)
                {
                    const int32_t winning_grain = m_claimGrid[nx][ny][nz];
                    if (winning_grain == 0)
                    {
                        has_empty_neighbor = true;
                    }
                    else if (winning_grain == grain_id)
                    {
                        if (__sync_bool_compare_and_swap(&voxels[nx][ny][nz], 0, winning_grain))
                        {
                            newFrontier.push_back({nx, ny, nz});
                            ++local_captures;
                        }
                    }
                }
            }

            if (has_empty_neighbor)
                keepActive.push_back(cell);
        }

#pragma omp barrier

        // Phase 3: Reset claim grid for next step
#pragma omp for schedule(static)
        for (size_t i = 0; i < active_size; i++)
        {
            const Coordinate& cell = grains[i];
            for (const auto& offset : PROBABILITY_OFFSETS)
            {
                int32_t nx = cell.x + offset[0];
                int32_t ny = cell.y + offset[1];
                int32_t nz = cell.z + offset[2];

                if (flags.isPeriodicStructure)
                {
                    nx = (nx + numCubes) % numCubes;
                    ny = (ny + numCubes) % numCubes;
                    nz = (nz + numCubes) % numCubes;
                }

                if (nx >= 0 && nx < numCubes &&
                    ny >= 0 && ny < numCubes &&
                    nz >= 0 && nz < numCubes)
                {
                    m_claimGrid[nx][ny][nz] = 0;
                }
            }
        }

        threadCaptures[tid] = local_captures;
    }

    std::vector<Coordinate> nextGrains;
    nextGrains.reserve(frontier_size * 2);

    unsigned int total_captured = 0;
    for (int t = 0; t < nthreads; ++t) {
        nextGrains.insert(nextGrains.end(),
                          std::make_move_iterator(threadNewGrains[t].begin()),
                          std::make_move_iterator(threadNewGrains[t].end()));
        nextGrains.insert(nextGrains.end(),
                          std::make_move_iterator(threadActiveGrains[t].begin()),
                          std::make_move_iterator(threadActiveGrains[t].end()));
        total_captured += threadCaptures[t];
    }

    // Retain passive (unprocessed) frontier cells when cap was applied
    for (size_t i = active_size; i < frontier_size; ++i) {
        nextGrains.push_back(grains[i]);
    }

    std::sort(nextGrains.begin(), nextGrains.end(), [](const Coordinate& a, const Coordinate& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    });
    nextGrains.erase(std::unique(nextGrains.begin(), nextGrains.end(), [](const Coordinate& a, const Coordinate& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }), nextGrains.end());

    grains = std::move(nextGrains);
    return total_captured;
}

void Probability_Algorithm::recordIteration(unsigned int counter_max,
                                            unsigned int cap,
                                            unsigned int captured,
                                            size_t       active_size,
                                            int          nucleated_this_iter,
                                            int          total_nucleated)
{
    CrystallizationRecord rec;
    rec.iteration           = this->IterationNumber;
    rec.fill_fraction       = static_cast<double>(filled_voxels) / counter_max;
    rec.captured            = captured;
    rec.cap                 = cap;
    rec.cap_utilization     = (cap > 0) ? (static_cast<float>(captured) / cap) : 1.0f;
    rec.frontier_size       = grains.size();
    rec.active_size         = active_size;
    rec.nucleated_this_iter = nucleated_this_iter;
    rec.total_nucleated     = total_nucleated;
    m_history.push_back(rec);
}

void Probability_Algorithm::fillIsolatedVoxels()
{
    unsigned int cleaned = 0;
    for (int32_t x = 0; x < numCubes; ++x)
    {
        for (int32_t y = 0; y < numCubes; ++y)
        {
            for (int32_t z = 0; z < numCubes; ++z)
            {
                if (voxels[x][y][z] != 0) continue;

                int32_t neighbor_grain = 0;
                for (const auto& off : PROBABILITY_OFFSETS)
                {
                    int32_t nx = x + off[0];
                    int32_t ny = y + off[1];
                    int32_t nz = z + off[2];

                    if (flags.isPeriodicStructure)
                    {
                        nx = (nx + numCubes) % numCubes;
                        ny = (ny + numCubes) % numCubes;
                        nz = (nz + numCubes) % numCubes;
                    }

                    if (nx >= 0 && nx < numCubes &&
                        ny >= 0 && ny < numCubes &&
                        nz >= 0 && nz < numCubes &&
                        voxels[nx][ny][nz] != 0)
                    {
                        neighbor_grain = voxels[nx][ny][nz];
                        break;
                    }
                }

                if (neighbor_grain != 0)
                {
                    voxels[x][y][z] = neighbor_grain;
                    ++cleaned;
                }
            }
        }
    }
    filled_voxels += cleaned;
}

void Probability_Algorithm::Next_Iteration()
{
    if (getDone()) return;

    const unsigned int counter_max =
        static_cast<unsigned int>(std::pow(numCubes, 3));

    if (total_nucleated_so_far == -1)
    {
        total_nucleated_so_far = static_cast<int>(grains.size());
        run_start = std::chrono::steady_clock::now();
    }

    const unsigned int cap = computeThermodynamicCap(counter_max);
    const size_t frontier_size = grains.size();

    // Option A: Active subset size selection when cap is smaller than frontier
    constexpr float alpha = 3.0f;
    const size_t active_size = (Parameters::stefan_number > 0.0f && cap < frontier_size / 4)
        ? std::min(frontier_size, static_cast<size_t>(std::ceil(alpha * cap)))
        : frontier_size;

    partialShuffle(active_size);

    const unsigned int captured = growFrontier(cap, active_size);
    filled_voxels += captured;

    // Option B: Deduct captured latent heat from enthalpy/cooling pool
    if (Parameters::stefan_number > 0.0f)
    {
        m_coolingPool = std::max(0.0, m_coolingPool - static_cast<double>(captured));
    }

    // Wave nucleation (transformation-fraction controlled)
    QString nucleationLog;
    int nucleated_this_iter = 0;
    if (flags.isWaveGeneration)
    {
        nucleated_this_iter = nucleateWave(counter_max, nucleationLog);
    }

    this->IterationNumber++;

    recordIteration(counter_max, cap, captured,
                    active_size, nucleated_this_iter, total_nucleated_so_far);

    const double fraction = (counter_max > 0) ? (static_cast<double>(filled_voxels) / counter_max) : 1.0;
    const double pct = fraction * 100.0;

    const bool isFinished = getDone();
    const bool shouldLog = isFinished ||
                           (IterationNumber <= 5) ||
                           (IterationNumber <= 50 && IterationNumber % 10 == 0) ||
                           (IterationNumber % 25 == 0) ||
                           (nucleated_this_iter > 0);

    if (shouldLog) {
        if (isFinished) {
            qDebug().noquote()
                << QString("[Probability] Step %1: filled %2/%3 (%4%) | grains: %5/%6 | growth complete%7")
                       .arg(IterationNumber, 2)
                       .arg(filled_voxels)
                       .arg(counter_max)
                       .arg(pct, 5, 'f', 1)
                       .arg(total_nucleated_so_far)
                       .arg(numColors)
                       .arg(nucleationLog);
        } else {
            qDebug().noquote()
                << QString("[Probability] Step %1: filled %2/%3 (%4%) | cap=%5, got=%6 | active front: %7/%8%9")
                       .arg(IterationNumber, 2)
                       .arg(filled_voxels)
                       .arg(counter_max)
                       .arg(pct, 5, 'f', 1)
                       .arg(cap)
                       .arg(captured)
                       .arg(active_size)
                       .arg(grains.size())
                       .arg(nucleationLog);
        }
    }

    if (grains.empty())
    {
        fillIsolatedVoxels();
    }
}

int Probability_Algorithm::nucleateWave(unsigned int counter_max, QString& logInfo)
{
    const int N_total = numColors;
    if (total_nucleated_so_far >= N_total || filled_voxels >= counter_max)
        return 0;

    const double X = (counter_max > 0) ? (static_cast<double>(filled_voxels) / counter_max) : 1.0;
    const double X_peak = std::clamp(static_cast<double>(Parameters::wave_peak_fraction), 0.01, 0.90);
    double X_end = std::clamp(static_cast<double>(Parameters::wave_end_fraction), 0.05, 0.95);
    if (X_end <= X_peak) {
        X_end = std::min(0.95, X_peak + 0.05);
    }

    const double sigma_X = std::max(0.01, (X_end - X_peak) / 2.5);

    double phi = 0.0;
    if (X >= X_end || X >= 0.90) {
        phi = 1.0;
    } else {
        auto g = [&](double x_val) {
            double arg = (x_val - X_peak) / (sigma_X * std::sqrt(2.0));
            return 0.5 * (1.0 + std::erf(arg));
        };
        double g0 = g(0.0);
        double gEnd = g(X_end);
        if (gEnd > g0) {
            phi = (g(X) - g0) / (gEnd - g0);
        } else {
            phi = 1.0;
        }
        phi = std::clamp(phi, 0.0, 1.0);
    }

    const int N_initial = std::clamp(Parameters::initial_nuclei_count, 1, N_total);
    const int target = N_initial + static_cast<int>(std::round(phi * (N_total - N_initial)));
    int toCreate = target - total_nucleated_so_far;

    // Safety emergency flush if domain is nearly full (>90%) but nuclei remain
    if (X >= 0.90 && toCreate <= 0 && total_nucleated_so_far < N_total) {
        toCreate = N_total - total_nucleated_so_far;
    }

    if (toCreate <= 0)
        return 0;

    toCreate = std::min(toCreate, N_total - total_nucleated_so_far);

    std::mt19937 rng(Parameters::seed ^ (IterationNumber * 2654435761u));
    std::uniform_int_distribution<int> dist(0, numCubes - 1);

    int placed = 0;
    for (int p = 0; p < toCreate; ++p)
    {
        bool success = false;
        for (int retry = 0; retry < 30; ++retry)
        {
            int rx = dist(rng);
            int ry = dist(rng);
            int rz = dist(rng);
            if (voxels[rx][ry][rz] == 0)
            {
                birthGrain(rx, ry, rz);
                placed++;
                success = true;
                break;
            }
        }
        if (!success)
        {
            // Gather all free coordinates
            std::vector<Coordinate> freeCoords;
            for (int x = 0; x < numCubes; ++x) {
                for (int y = 0; y < numCubes; ++y) {
                    for (int z = 0; z < numCubes; ++z) {
                        if (voxels[x][y][z] == 0)
                            freeCoords.push_back({x, y, z});
                    }
                }
            }
            if (freeCoords.empty()) {
                qWarning() << "[Probability] No free voxels left to place wave nuclei!";
                break;
            }
            std::shuffle(freeCoords.begin(), freeCoords.end(), rng);
            int left = std::min<int>(toCreate - placed, static_cast<int>(freeCoords.size()));
            for (int i = 0; i < left; ++i) {
                birthGrain(freeCoords[i].x, freeCoords[i].y, freeCoords[i].z);
                placed++;
            }
            break;
        }
    }

    total_nucleated_so_far += placed;

    logInfo = QString(" | [WaveNucl] X=%1 phi=%2 added=%3 tot=%4/%5")
                  .arg(X, 5, 'f', 3)
                  .arg(phi, 5, 'f', 3)
                  .arg(placed)
                  .arg(total_nucleated_so_far)
                  .arg(N_total);

    return placed;
}

bool Probability_Algorithm::getDone() const
{
    if (flags.isWaveGeneration && total_nucleated_so_far < numColors)
        return false;
    return (grains.empty() && IterationNumber > 0) || Parent_Algorithm::getDone();
}

void Probability_Algorithm::CleanUp()
{
    saveSeeds();
    const QString dir = Parameters::working_directory.isEmpty() ? "." : Parameters::working_directory;
    if (numCubes > 0 && filled_voxels > 0) {
        auto stats = GrainAnalyzer::analyze3D(voxels, numCubes);
        GrainAnalyzer::writeToCSV3D(stats, dir + "/grain_size_distribution.csv");
        writeHistoryToCSV(dir);
        writeProbabilitiesToCSV(dir);
    }
    if (m_claimGrid) {
        Delete3D(m_claimGrid);
        m_claimGrid = nullptr;
    }
    IterationNumber = 0;
    m_coolingPool = 0.0;
    m_history.clear();
    Parent_Algorithm::CleanUp();
}

void Probability_Algorithm::printProbabilityKernel() const
{
    qDebug().noquote() << "=== Probability Algorithm: 3x3x3 Growth Probability Kernel ===";
    for (int k = 0; k < 3; ++k)
    {
        const int dz = k - 1;
        const QString sliceName = (dz == -1) ? "Z = -1 (Bottom slice)" : (dz == 0) ? "Z =  0 (Center slice)" : "Z = +1 (Top slice)";
        qDebug().noquote() << QString("  --- %1 ---").arg(sliceName);

        for (int i = 0; i < 3; ++i)
        {
            QString row = "    [";
            for (int j = 0; j < 3; ++j)
            {
                row += QString(" %1").arg(probability[i][j][k], 6, 'f', 4);
            }
            row += " ]";
            qDebug().noquote() << row;
        }
    }
    qDebug().noquote() << "==============================================================";
}

void Probability_Algorithm::writeProbabilitiesToCSV(const QString& dirPath) const
{
    QDir dir(dirPath.isEmpty() ? "." : dirPath);
    QString fullPath = dir.filePath("probability_kernel.csv");

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << "dx,dy,dz,probability\n";

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                out << (i - 1) << ","
                    << (j - 1) << ","
                    << (k - 1) << ","
                    << QString::number(probability[i][j][k], 'g', 6) << "\n";
            }
        }
    }
    file.close();
}

void Probability_Algorithm::writeHistoryToCSV(const QString& dirPath) const
{
    if (m_history.empty()) return;

    QDir dir(dirPath.isEmpty() ? "." : dirPath);
    QString fullPath = dir.filePath("crystallization_history.csv");

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << "iteration,fill_fraction,captured,cap,cap_utilization,frontier_size,active_size,nucleated_this_iter,total_nucleated\n";

    for (const auto& r : m_history)
    {
        out << r.iteration           << ","
            << r.fill_fraction       << ","
            << r.captured            << ","
            << r.cap                 << ","
            << r.cap_utilization     << ","
            << r.frontier_size       << ","
            << r.active_size         << ","
            << r.nucleated_this_iter << ","
            << r.total_nucleated     << "\n";
    }
    file.close();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Plugin registration
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<ParamField> probabilitySchema()
{
    std::vector<ParamField> s = {
        { "size",        "Cube size",     ParamField::Int,        10, 1, 500, {}, "main" },
        { "points",      "Points",        ParamField::PointsMode, 10, 1, 100000,
         { "Size", "Concentration" }, "main" },
        { "is_periodic", "Periodic cell", ParamField::Bool,       true, {}, {}, {}, "main" },
        { "prob_matrix_mode",    "Matrix mode",   ParamField::Enum,       "Volume Sampling", {}, {},
         { "Volume Sampling", "Surface Flux" }, "main" },
        { "prob_preset", "Shape preset",  ParamField::Enum,       "Sphere (Circle)", {}, {},
         { "Sphere (Circle)", "Prolate (Needle)", "Oblate (Disc)", "Triaxial Ellipsoid", "Superellipsoid (Cube)", "Custom" },
         "main", /*invokeMethod=*/ "setProbPreset" },
        { "halfaxis_a",          "Half-axis a",           ParamField::Double, 1.5, 0.1, 100.0, {}, "main" },
        { "halfaxis_b",          "Half-axis b",           ParamField::Double, 1.5, 0.1, 100.0, {}, "main" },
        { "halfaxis_c",          "Half-axis c",           ParamField::Double, 1.5, 0.1, 100.0, {}, "main" },
        { "ellipse_order",       "Superellipsoid order",  ParamField::Double, 2.0, 0.5, 10.0,  {}, "main" },
        { "orientation_angle_a", "Angle X (deg)",         ParamField::Double, 0.0, 0.0, 360.0, {}, "main" },
        { "orientation_angle_b", "Angle Y (deg)",         ParamField::Double, 0.0, 0.0, 360.0, {}, "main" },
        { "orientation_angle_c", "Angle Z (deg)",         ParamField::Double, 0.0, 0.0, 360.0, {}, "main" },
        { "stefan_number",       "Stefan number (cooling)", ParamField::Double, 100.0, 1.0, 1000.0, {}, "main" },
        { "is_wave_generation",  "Wave nucleation",       ParamField::Bool,   false, {}, {}, {}, "main" },
        { "initial_nuclei_count","Initial nuclei",        ParamField::Int,    1, 1, 100000, {}, "main" },
        { "wave_peak_fraction",  "Nucl peak (solid frac)", ParamField::Double, 0.20, 0.01, 0.90, {}, "main" },
        { "wave_end_fraction",   "Nucl end (solid frac)",  ParamField::Double, 0.60, 0.05, 0.95, {}, "main" },
    };

    s.push_back(materialParamField());

    const std::vector<ParamField> tex = textureParamFields();
    s.insert(s.end(), tex.begin(), tex.end());

    return s;
}

MATVIZ_REGISTER_ALGORITHM(AlgorithmPlugin{
    "Probability",
    "Stochastic cellular automaton with anisotropic superellipsoidal shape kernels, "
    "thermodynamic Stefan cooling limit, periodic boundaries, material and texture selection.",
    /*order=*/ 4,
    probabilitySchema(),
    [](const Parameters& p) {
        return std::make_shared<Probability_Algorithm>(static_cast<short int>(p.getSize()), p.getPoints());
    }
});

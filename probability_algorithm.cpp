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

    double Rx[3][3] = {
        {1, 0, 0},
        {0, cos(rad_a), -sin(rad_a)},
        {0, sin(rad_a), cos(rad_a)}
    };

    double Ry[3][3] = {
        {cos(rad_b), 0, sin(rad_b)},
        {0, 1, 0},
        {-sin(rad_b), 0, cos(rad_b)}
    };

    double Rz[3][3] = {
        {cos(rad_c), -sin(rad_c), 0},
        {sin(rad_c), cos(rad_c), 0},
        {0, 0, 1}
    };

    double x1 = Rx[0][0] * x + Rx[0][1] * y + Rx[0][2] * z;
    double y1 = Rx[1][0] * x + Rx[1][1] * y + Rx[1][2] * z;
    double z1 = Rx[2][0] * x + Rx[2][1] * y + Rx[2][2] * z;

    double x2 = Ry[0][0] * x1 + Ry[0][1] * y1 + Ry[0][2] * z1;
    double y2 = Ry[1][0] * x1 + Ry[1][1] * y1 + Ry[1][2] * z1;
    double z2 = Ry[2][0] * x1 + Ry[2][1] * y1 + Ry[2][2] * z1;

    x = Rz[0][0] * x2 + Rz[0][1] * y2 + Rz[0][2] * z2;
    y = Rz[1][0] * x2 + Rz[1][1] * y2 + Rz[1][2] * z2;
    z = Rz[2][0] * x2 + Rz[2][1] * y2 + Rz[2][2] * z2;
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
    const uint64_t n = 60;
    const double step = 3.0 / n;
    const double num_points_per_voxel = std::pow(1.0 / step, 3);

    uint64_t filed_in_local[3][3][3] = {{{0}}};

    for (uint64_t i = 0; i < n; ++i) {
        for (uint64_t j = 0; j < n; ++j) {
            for (uint64_t k = 0; k < n; ++k) {
                double x = (i + 0.5) * step;
                double y = (j + 0.5) * step;
                double z = (k + 0.5) * step;

                if (isPointIn(x, y, z)) {
                    int k_voxel = std::clamp((int)floor(x), 0, 2);
                    int l_voxel = std::clamp((int)floor(y), 0, 2);
                    int m_voxel = std::clamp((int)floor(z), 0, 2);
                    filed_in_local[k_voxel][l_voxel][m_voxel]++;
                }
            }
        }
    }

    double maxProb = 0.0;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                this->probability[i][j][k] = static_cast<double>(filed_in_local[i][j][k]) / num_points_per_voxel;
                if (i != 1 || j != 1 || k != 1) {
                    maxProb = std::max(maxProb, this->probability[i][j][k]);
                }
            }
        }
    }

    if (maxProb > 0.0) {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                for (int k = 0; k < 3; k++)
                    this->probability[i][j][k] /= maxProb;
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

    double flux[3][3][3] = {{{0.0}}};
    std::mt19937_64 rng(Parameters::seed);
    std::uniform_real_distribution<double> uni(-1.0, 1.0);

    for (uint64_t s = 0; s < N_surface; ++s) {
        double ux, uy, uz, len2;
        do {
            ux = uni(rng); uy = uni(rng); uz = uni(rng);
            len2 = ux*ux + uy*uy + uz*uz;
        } while (len2 < 1e-12 || len2 > 1.0);

        double inv_len = 1.0 / std::sqrt(len2);
        ux *= inv_len; uy *= inv_len; uz *= inv_len;

        double sum_p = pow(std::abs(ux / a), p) + pow(std::abs(uy / b), p) + pow(std::abs(uz / c), p);
        if (sum_p < 1e-30) continue;

        double t = pow(sum_p, -1.0 / p);
        double sx = t * ux;
        double sy = t * uy;
        double sz = t * uz;

        double nx = (p / a) * pow(std::abs(sx / a), p - 1.0) * (sx >= 0 ? 1.0 : -1.0);
        double ny = (p / b) * pow(std::abs(sy / b), p - 1.0) * (sy >= 0 ? 1.0 : -1.0);
        double nz = (p / c) * pow(std::abs(sz / c), p - 1.0) * (sz >= 0 ? 1.0 : -1.0);

        double nlen = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (nlen < 1e-30) continue;
        nx /= nlen; ny /= nlen; nz /= nlen;

        rotatePoint(nx, ny, nz);

        for (const auto& offset : PROBABILITY_OFFSETS) {
            double dx = offset[0], dy = offset[1], dz = offset[2];
            double dlen = std::sqrt(dx*dx + dy*dy + dz*dz);
            double dot = (nx*dx + ny*dy + nz*dz) / dlen;
            if (dot > 0.0) {
                flux[1 + offset[0]][1 + offset[1]][1 + offset[2]] += dot;
            }
        }
    }

    double maxFlux = 0.0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++)
                maxFlux = std::max(maxFlux, flux[i][j][k]);

    if (maxFlux < 1e-30) maxFlux = 1.0;

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++)
                this->probability[i][j][k] = flux[i][j][k] / maxFlux;

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
    Parent_Algorithm::Initialization(isWaveGeneration);
    run_start = std::chrono::steady_clock::now();
    m_history.clear();
    IterationNumber = 0;

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

    calculateVolumeProbabilities();

    qDebug().noquote()
        << QString("[Probability] %1^3 grid (%2 voxels), %3 seeds, preset: '%4' (a=%5, b=%6, c=%7, order=%8, St=%9)%10")
               .arg(numCubes)
               .arg(static_cast<uint64_t>(numCubes) * numCubes * numCubes)
               .arg(seedPoints.size())
               .arg(Parameters::prob_preset)
               .arg(Parameters::halfaxis_a, 0, 'g', 3)
               .arg(Parameters::halfaxis_b, 0, 'g', 3)
               .arg(Parameters::halfaxis_c, 0, 'g', 3)
               .arg(Parameters::ellipse_order, 0, 'g', 3)
               .arg(Parameters::stefan_number, 0, 'g', 3)
               .arg(flags.isPeriodicStructure ? ", periodic" : "");
}

unsigned int Probability_Algorithm::computeThermodynamicCap(unsigned int counter_max) const
{
    const float St = Parameters::stefan_number;
    if (St <= 0.0f) return counter_max;

    const unsigned int cap = static_cast<unsigned int>(
        std::max(1.0f, std::floor(static_cast<float>(counter_max) / St)));
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

    partialShuffle(frontier_size);

    const unsigned int captured = growFrontier(cap, frontier_size);
    filled_voxels += captured;
    this->IterationNumber++;

    recordIteration(counter_max, cap, captured,
                    frontier_size, 0, total_nucleated_so_far);

    const double fraction = (counter_max > 0) ? (static_cast<double>(filled_voxels) / counter_max) : 1.0;
    const double pct = fraction * 100.0;

    const bool isFinished = getDone();
    const bool shouldLog = isFinished ||
                           (IterationNumber <= 5) ||
                           (IterationNumber <= 50 && IterationNumber % 10 == 0) ||
                           (IterationNumber % 25 == 0);

    if (shouldLog) {
        if (isFinished) {
            qDebug().noquote()
                << QString("[Probability] Step %1: filled %2/%3 (%4%) | growth complete")
                       .arg(IterationNumber, 2)
                       .arg(filled_voxels)
                       .arg(counter_max)
                       .arg(pct, 5, 'f', 1);
        } else {
            qDebug().noquote()
                << QString("[Probability] Step %1: filled %2/%3 (%4%) | active front: %5 voxels")
                       .arg(IterationNumber, 2)
                       .arg(filled_voxels)
                       .arg(counter_max)
                       .arg(pct, 5, 'f', 1)
                       .arg(grains.size());
        }
    }

    if (grains.empty())
    {
        fillIsolatedVoxels();
    }
}

bool Probability_Algorithm::getDone() const
{
    return (grains.empty() && IterationNumber > 0) || Parent_Algorithm::getDone();
}

void Probability_Algorithm::CleanUp()
{
    if (numCubes > 0 && filled_voxels > 0) {
        auto stats = GrainAnalyzer::analyze3D(voxels, numCubes);
        const QString dir = Parameters::working_directory.isEmpty() ? "." : Parameters::working_directory;
        GrainAnalyzer::writeToCSV3D(stats, dir + "/grain_size_distribution.csv");
    }
    if (m_claimGrid) {
        Delete3D(m_claimGrid);
        m_claimGrid = nullptr;
    }
    IterationNumber = 0;
    m_history.clear();
    Parent_Algorithm::CleanUp();
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
        { "seed",        "Random seed",   ParamField::Int,        0, 0, 2147483647, {}, "main" },
        { "is_periodic", "Periodic cell", ParamField::Bool,       true, {}, {}, {}, "main" },
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
    /*order=*/ 6,
    probabilitySchema(),
    [](const Parameters& p) {
        return std::make_shared<Probability_Algorithm>(static_cast<short int>(p.getSize()), p.getPoints());
    }
});

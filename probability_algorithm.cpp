#include "probability_algorithm.h"
#include "ui_probability_algorithm.h"
#include "parameters.h"
#include <random>
#include <cmath>
#include <omp.h>
#include <QString>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <chrono>

Probability_Algorithm::Probability_Algorithm(QWidget *parent) :
    QWidget(parent), Parent_Algorithm(),
    ui(new Ui::Probability_Algorithm)
{
    ui->setupUi(this);
    connect(ui->applyPushButton,&QPushButton::clicked,this,&Probability_Algorithm::setHalfAxis);
    connect(ui->applyPushButton,&QPushButton::clicked,this,&QWidget::close);
    connect(ui->cancelPushButton,&QPushButton::clicked,this,&QWidget::close);
}

Probability_Algorithm::Probability_Algorithm(short int numCubes, int numColors, QWidget *parent)
    : QWidget(parent), ui(new Ui::Probability_Algorithm)
{
    ui->setupUi(this);
    connect(ui->applyPushButton,&QPushButton::clicked,this,&Probability_Algorithm::setHalfAxis);
    connect(ui->applyPushButton,&QPushButton::clicked,this,&QWidget::close);
    connect(ui->cancelPushButton,&QPushButton::clicked,this,&QWidget::close);

    setNumCubes(numCubes);
    setNumColors(numColors);
    processProbabilities(ProbabilityMode::VolumeSampling);
}

Probability_Algorithm::~Probability_Algorithm()
{
    delete ui;
}

void Probability_Algorithm::setHalfAxis()
{
    Parameters::halfaxis_a = ui->axisALineEdit->text().toFloat();
    Parameters::halfaxis_b = ui->axisBLineEdit->text().toFloat();
    Parameters::halfaxis_c = ui->axisCLineEdit->text().toFloat();
    Parameters::orientation_angle_a = ui->orintationAngleLineEdit->text().toFloat();
    Parameters::orientation_angle_b = ui->lineEdit->text().toFloat();
    Parameters::orientation_angle_c = ui->lineEdit_2->text().toFloat();
}

bool Probability_Algorithm::isPointIn(double x, double y, double z)
{
    // Translate to origin first
    double cx = x - 1.5;
    double cy = y - 1.5;
    double cz = z - 1.5;
    // Rotate the centered offset, not the raw point
    rotatePoint(cx, cy, cz);
    return (pow(std::abs(cx) / Parameters::halfaxis_a, Parameters::ellipse_order)
            + pow(std::abs(cy) / Parameters::halfaxis_b, Parameters::ellipse_order)
            + pow(std::abs(cz) / Parameters::halfaxis_c, Parameters::ellipse_order)) <= 1.0;
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

void Probability_Algorithm::rotatePoint(double& x, double& y, double& z)
{
    const double Rx[3][3] = {
        {1, 0, 0},
        {0, cos(toRadians(Parameters::orientation_angle_a)), -sin(toRadians(Parameters::orientation_angle_a))},
        {0, sin(toRadians(Parameters::orientation_angle_a)), cos(toRadians(Parameters::orientation_angle_a))}
    };

    const double Ry[3][3] = {
        {cos(toRadians(Parameters::orientation_angle_b)), 0, sin(toRadians(Parameters::orientation_angle_b))},
        {0, 1, 0},
        {-sin(toRadians(Parameters::orientation_angle_b)), 0, cos(toRadians(Parameters::orientation_angle_b))}
    };

    const double Rz[3][3] = {
        {cos(toRadians(Parameters::orientation_angle_c)), -sin(toRadians(Parameters::orientation_angle_c)), 0},
        {sin(toRadians(Parameters::orientation_angle_c)), cos(toRadians(Parameters::orientation_angle_c)), 0},
        {0, 0, 1}
    };
    double x1 = Rx[0][0] * x + Rx[0][1] * y + Rx[0][2] * z;
    double y1 = Rx[1][0] * x + Rx[1][1] * y + Rx[1][2] * z;
    double z1 = Rx[2][0] * x + Rx[2][1] * y + Rx[2][2] * z;

    double x2 = Ry[0][0] * x1 + Ry[0][1] * y1 + Ry[0][2] * z1;
    double y2 = Ry[1][0] * x1 + Ry[1][1] * y1 + Ry[1][2] * z1;
    double z2 = Ry[2][0] * x1 + Ry[2][1] * y1 + Ry[2][2] * z1;

    double x3 = Rz[0][0] * x2 + Rz[0][1] * y2 + Rz[0][2] * z2;
    double y3 = Rz[1][0] * x2 + Rz[1][1] * y2 + Rz[1][2] * z2;
    double z3 = Rz[2][0] * x2 + Rz[2][1] * y2 + Rz[2][2] * z2;

    x = x3;
    y = y3;
    z = z3;
}

// Unified entry point to switch between algorithms
void Probability_Algorithm::processProbabilities(ProbabilityMode mode)
{
    if (mode == ProbabilityMode::VolumeSampling) {
        qDebug() << "Calculating probabilities proportional to VOLUME...";
        calculateVolumeProbabilities();
    } else {
        qDebug() << "Calculating probabilities proportional to SURFACE AREA (Lambertian)...";
        calculateSurfaceFluxProbabilities();
    }
}

// --- ALGORITHM A: VOLUMETRIC SAMPLING (Your original logic) ---
void Probability_Algorithm::calculateVolumeProbabilities()
{
    const uint64_t N = pow(102, 3);
    const uint64_t n = std::round(std::cbrt(N));
    const double step = 3.0 / n;
    const double num_points_per_voxel = pow(1.0 / step, 3);

    uint64_t fileld_in_local[3][3][3] = {{{0}}};

    for (uint64_t i = 0; i < n; ++i) {
        for (uint64_t j = 0; j < n; ++j) {
            for (uint64_t k = 0; k < n; ++k) {
                double x = (i + 0.5) * step;
                double y = (j + 0.5) * step;
                double z = (k + 0.5) * step;

                if (isPointIn(x, y, z)) {
                    int k_voxel = std::min(std::max((int)floor(x), 0), 2);
                    int l_voxel = std::min(std::max((int)floor(y), 0), 2);
                    int m_voxel = std::min(std::max((int)floor(z), 0), 2);
                    fileld_in_local[k_voxel][l_voxel][m_voxel]++;
                }
            }
        }
    }

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++)
                this->probability[i][j][k] = (double)fileld_in_local[i][j][k] / num_points_per_voxel;

    this->probability[1][1][1] = 0.0; // Ensure center is 0
    prettyPrint3DArray(this->probability);
    writeProbabilitiesToCSV(QCoreApplication::applicationDirPath(), N);
}

// --- ALGORITHM B: SURFACE FLUX SAMPLING (Pasted logic) ---
void Probability_Algorithm::calculateSurfaceFluxProbabilities()
{
    const uint64_t N_surface = 500000;
    const double a = Parameters::halfaxis_a;
    const double b = Parameters::halfaxis_b;
    const double c = Parameters::halfaxis_c;
    const double p = Parameters::ellipse_order;

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
    prettyPrint3DArray(this->probability);
    writeProbabilitiesToCSV(QCoreApplication::applicationDirPath(), N_surface);
}




void Probability_Algorithm::prettyPrint3DArray(double arr[3][3][3])
{
    QString output;

    // Header
    output += "Probability Array [3][3][3]:\n";
    output += "==================\n";

    // Print the 3D array
    for (int i = 0; i < 3; i++) {
        output += QString("Layer %1:\n").arg(i);
        for (int j = 0; j < 3; j++) {
            output += "  [";
            for (int k = 0; k < 3; k++) {
                output += QString("%1").arg(arr[i][j][k], 5, 'f', 3);
                if (k < 2) output += ", ";
            }
            output += "]\n";
        }
        if (i < 2) output += "\n";
    }

    // Calculate directional probabilities
    output += "\n==================\n";
    output += "Directional Probabilities:\n";
    output += "==================\n";

    output += QString("+X direction: %1\n").arg(arr[2][1][1], 6, 'f', 3);
    output += QString("-X direction: %1\n").arg(arr[0][1][1], 6, 'f', 3);
    output += QString("+Y direction: %1\n").arg(arr[1][2][1], 6, 'f', 3);
    output += QString("-Y direction: %1\n").arg(arr[1][0][1], 6, 'f', 3);
    output += QString("+Z direction: %1\n").arg(arr[1][1][2], 6, 'f', 3);
    output += QString("-Z direction: %1\n").arg(arr[1][1][0], 6, 'f', 3);

    // Single qDebug call at the end
    qDebug().noquote() << output;
}


// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

// --- Thermodynamic cap -------------------------------------------------
// Returns max voxels that may crystallize this iteration.
unsigned int Probability_Algorithm::computeThermodynamicCap(
    unsigned int counter_max) const
{
    // Constant cooling: external heat flux is fixed.
    // Stefan condition at the interface:
    //   ρ·L·v_n  =  k · ∇T  →  integrated over RVE:
    //   ΔN_max  =  Q_ext / (ρ·L·V_voxel)
    //           =  N_total · ΔT_per_iter · (cp/L)
    //           =  N_total / St
    //
    // N_total — cap is CONSTANT under constant flux BC.
    // It decreases only if the external flux decreases (e.g. convective
    // cooling where Q ~ T - T_env, not relevant for furnace cooling).
    //
    // St = L / (cp · ΔT_per_iteration)
    //   St >> 1  →  small cap  (slow solidification, high latent heat)
    //   St ~ 1   →  large cap  (fast quench, latent heat ~ sensible heat)
    //
    // Typical metals: St = 50–400.

    const float St = Parameters::stefan_number;
    if (St <= 0.0f) return counter_max;  // disabled

    // Optional sensible heat correction (negligible for most metals
    // since cp_s ≈ cp_l):
    //   ΔN_correction = (N_solid * (cp_s - cp_l) / L) * ΔT_per_iter
    // Omitted here — add if cp_s/cp_l differ significantly.

    const unsigned int cap = static_cast<unsigned int>(
        std::max(1.0f, std::floor(static_cast<float>(counter_max) / St)));

    return cap;  // constant every iteration
}

// --- CA growth step ----------------------------------------------------
// ---------------------------------------------------------------------------
// growFrontier
//
// Parameters:
//   maxCaptures  — thermodynamic cap: how many voxels may be crystallized
//                  in this iteration (N_total / St).
//   active_size  — how many frontier elements participate in growth.
//                  The remainder grains[active_size..end] is "passive": kept
//                  active automatically without any processing.
//
// Parallelism strategies:
//   1. Per-thread budget  — each thread receives a quota of cap/nthreads,
//      eliminating the hot global atomic capture counter.
//   2. Active subset      — when cap << frontier, only the first active_size
//      elements are processed (after shuffle they form a random sample).
//
// Returns: number of voxels actually captured this iteration.
// ---------------------------------------------------------------------------
unsigned int Probability_Algorithm::growFrontier(unsigned int maxCaptures,
                                                 size_t       active_size)
{
    // --- Active and passive partition sizes ----------------------------
    const size_t frontier_size = grains.size();
    active_size = std::min(active_size, frontier_size);
    const size_t passive_begin = active_size;   // [passive_begin..frontier_size) left untouched

    // --- Per-thread budget --------------------------------------------
    // Ceiling division: the sum of quotas slightly exceeds cap, which is
    // safe — the true total is bounded by the reduction over thread_captures.
    const int nthreads = std::max(1, omp_get_max_threads());
    const unsigned int per_thread_budget =
        (maxCaptures + static_cast<unsigned int>(nthreads) - 1)
        / static_cast<unsigned int>(nthreads);

    // --- Result buffer ------------------------------------------------
    // Final newGrains = [newly captured] + [keep_active from active]
    //                  + [passive, unchanged]
    std::vector<Coordinate> newGrains;
    newGrains.reserve(frontier_size * 2);

    unsigned int total_captured = 0;   // OMP reduction accumulator

// =================================================================
    #pragma omp parallel reduction(+:total_captured)
    {
        const int tid = omp_get_thread_num();

        // Thread-private RNG — no sharing between threads
        std::mt19937 rng(Parameters::seed
                         + static_cast<unsigned>(tid)
                         + IterationNumber * 1000003u);
        std::uniform_real_distribution<double> dice(0.0, 1.0);

        // Local capture counter for this thread (not atomic)
        unsigned int thread_captures = 0;

        std::vector<Coordinate> privateGrains;
        privateGrains.reserve(active_size * 2 / nthreads + 16);

        // ----- Active frontier slice [0 .. active_size) --------------
        #pragma omp for schedule(dynamic, 4) nowait
        for (size_t i = 0; i < active_size; i++)
        {
            const Coordinate& cell     = grains[i];
            const int32_t     x        = cell.x;
            const int32_t     y        = cell.y;
            const int32_t     z        = cell.z;
            const int32_t     grain_id = voxels[x][y][z];

            // Thread quota exhausted — keep grain active but skip the
            // neighbour loop entirely, no point iterating all 26 offsets.
            if (thread_captures >= per_thread_budget)
            {
                privateGrains.push_back(cell);
                continue;
            }

            bool keep_active = false;

            for (const auto& offset : PROBABILITY_OFFSETS)
            {
                // --- Neighbour coordinates ---
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
                    continue;   // already occupied

                const double prob =
                    probability[1 + offset[0]][1 + offset[1]][1 + offset[2]];

                // Zero-probability direction — does not affect grain liveness
                if (prob < 1e-12)
                    continue;

                // Quota just ran out: neighbour is reachable but cannot be
                // captured this iteration — mark grain active and stop scanning.
                if (thread_captures >= per_thread_budget)
                {
                    keep_active = true;
                    break;   // remaining neighbours would be skipped anyway
                }

                // --- Probability dice roll ---
                if (dice(rng) >= prob)
                {
                    // Capture attempt failed this iteration, but the neighbour
                    // is still empty — grain may capture it next iteration.
                    keep_active = true;
                    continue;
                }

                // --- Atomic capture attempt (CAS) ---------------------
                // Retries are needed only when two threads race on the same
                // voxel simultaneously.
                bool captured = false;
                for (int attempt = 0; attempt < 3; ++attempt)
                {
                    if (__sync_bool_compare_and_swap(
                            &voxels[nx][ny][nz], 0, grain_id))
                    {
                        captured = true;
                        break;
                    }
                    // Another thread already wrote here — stop retrying
                    if (voxels[nx][ny][nz] != 0)
                        break;
                }

                if (captured)
                {
                    // thread_captures is a local variable — no atomic needed
                    ++thread_captures;
                    total_captured++;   // reduced across threads at barrier
                    privateGrains.push_back({nx, ny, nz});
                }
                // CAS lost to another thread — neighbour is no longer empty,
                // so keep_active is not set for this neighbour.
            }

            if (keep_active)
                privateGrains.push_back(cell);
        }
        // ----- End of parallel loop over active frontier -------------

        // Merge per-thread buffers into the shared result vector
        #pragma omp critical
        {
            newGrains.insert(newGrains.end(),
                             std::make_move_iterator(privateGrains.begin()),
                             std::make_move_iterator(privateGrains.end()));
        }
    }
    // =================================================================

    // --- Passive slice [active_size .. frontier_size) ----------------
    // These grains did not participate in growth this iteration but remain
    // candidates for the next one — copy them as-is.
    if (passive_begin < frontier_size)
    {
        newGrains.insert(newGrains.end(),
                         grains.begin() + passive_begin,
                         grains.end());
    }

    grains = std::move(newGrains);
    return total_captured;
}

// ---------------------------------------------------------------------------
// partialShuffle
//
// Partial Fisher-Yates: moves `active_size` randomly-chosen elements to
// grains[0..active_size) in O(active_size) swaps.
//
// Replaces full O(N) shuffle — critical when cap << frontier (e.g. St=100
// means only 1% of the frontier needs to be touched each iteration).
//
// Seed is mixed with IterationNumber so successive iterations produce
// independent permutations even with the same Parameters::seed.
// ---------------------------------------------------------------------------
void Probability_Algorithm::partialShuffle(size_t active_size)
{
    const size_t fs = grains.size();
    if (active_size == 0 || fs <= 1)
        return;

    active_size = std::min(active_size, fs);

    std::mt19937 rng(Parameters::seed ^ (IterationNumber * 2246822519u));

    for (size_t i = 0; i < active_size; ++i)
    {
        // Uniform pick from [i, fs) without constructing a distribution
        // object each iteration. Slight modulo bias is negligible for CA.
        const size_t range = fs - i;
        const size_t j     = i + (static_cast<size_t>(rng()) % range);
        if (i != j)
            std::swap(grains[i], grains[j]);
    }
}

// ---------------------------------------------------------------------------
// Next_Iteration — partial Fisher-Yates replaces full shuffle (O(active) vs O(N))
// ---------------------------------------------------------------------------
void Probability_Algorithm::Next_Iteration(std::function<void()> callback)
{
    const unsigned int counter_max =
        static_cast<unsigned int>(std::pow(numCubes, 3));
    int total_nucleated = static_cast<int>(grains.size());

    // Timing state for rate / ETA computation
    using Clock = std::chrono::steady_clock;
    auto run_start = Clock::now();

    while (!grains.empty() || this->IterationNumber == 0)
    {
        QApplication::processEvents();

        auto iter_start = Clock::now();

        const unsigned int cap          = computeThermodynamicCap(counter_max);
        const size_t       frontier_size = grains.size();

        // --- Active subset size -----------------------------------------
        // alpha=3 gives enough headroom to reach cap even at low probability.
        constexpr float alpha = 3.0f;
        const size_t active_size =
            (cap < frontier_size / 4)
                ? std::min(frontier_size,
                           static_cast<size_t>(std::ceil(alpha * cap)))
                : frontier_size;

        // Shuffle only the active prefix — O(active_size), not O(frontier)
        partialShuffle(active_size);

        // --- Grow frontier ----------------------------------------------
        const unsigned int captured = growFrontier(cap, active_size);
        filled_voxels += captured;

        // Wave nucleation
        QString nucleationLog;
        int nucleated_this_iter = 0;
        if (flags.isWaveGeneration)
        {
            nucleated_this_iter  = nucleateWave(total_nucleated, nucleationLog);
            total_nucleated     += nucleated_this_iter;
        }
        this->IterationNumber++;

        // Record history entry for CSV export / statistics analysis
        recordIteration(counter_max, cap, captured,
                        active_size, nucleated_this_iter, total_nucleated);


        // --- Progress output --------------------------------------------
        const double iter_dt =
            std::chrono::duration<double>(Clock::now() - iter_start).count();
        const double elapsed =
            std::chrono::duration<double>(Clock::now() - run_start).count();

        const bool shouldLog =
            (this->IterationNumber <= 10) ||
            (this->IterationNumber <= 100 && this->IterationNumber % 5  == 0) ||
            (this->IterationNumber % 20 == 0);

        if (shouldLog)
            logIteration(counter_max, cap, captured, active_size,
                         frontier_size, iter_dt, elapsed, nucleationLog);

        if (flags.isAnimation)
            callback();
    }

    fillIsolatedVoxels();

}


void Probability_Algorithm::CleanUp()
{
    writeHistoryToCSV("./");
    IterationNumber = 0;
    m_history.clear();
    Parent_Algorithm::CleanUp();  // resets filled_voxels, isDone
}


// --- Wave nucleation step ----------------------------------------------
// Adds new nuclei according to the cumulative Gaussian schedule.
// Returns the number of new nuclei placed; appends a log token to `logInfo`.
int Probability_Algorithm::nucleateWave(int totalNucleatedSoFar, QString& logInfo)
{
    const int N_gr = numColors;

    if (totalNucleatedSoFar >= N_gr || filled_voxels >= static_cast<unsigned int>(std::pow(numCubes, 3)))
        return 0;

    // Cumulative Gaussian nucleation schedule
    double cumulative_fraction = 0.0;
    if (this->IterationNumber > 1)
    {
        const double arg = (IterationNumber - Parameters::wave_coefficient / 2.0)
        / (Parameters::wave_spread * std::sqrt(2.0));
        cumulative_fraction = 0.5 * (1.0 + std::erf(arg));
    }

    const int target    = Parameters::initial_nuclei_count
                       + static_cast<int>(std::floor(
                           cumulative_fraction * (N_gr - Parameters::initial_nuclei_count)));
    const int toCreate  = target - totalNucleatedSoFar;

    if (toCreate <= 0)
        return 0;

    // Use a seeded mt19937 drop std::rand() which is unseeded and not thread-safe.
    std::mt19937 rng(Parameters::seed ^ (IterationNumber * 2654435761u));
    std::uniform_int_distribution<int> coord(0, numCubes - 1);

    int placed = 0;
    for (int p = 0; p < toCreate; ++p)
    {
        bool success = false;
        for (int retry = 0; retry < 10; ++retry)
        {
            int rx = coord(rng), ry = coord(rng), rz = coord(rng);
            if (voxels[rx][ry][rz] == 0)
            {
                voxels[rx][ry][rz] = ++color;
                grains.push_back({rx, ry, rz});
                filled_voxels++;
                placed++;
                success = true;
                break;
            }
        }
        if (!success)
        {
            // Fallback to Add_New_Points for the remainder
            grains = Add_New_Points(grains, toCreate - placed);
            placed = toCreate;
            break;
        }
    }

    logInfo = QString(" | [Nucl] phi=%1 added=%2 tot=%3")
                  .arg(cumulative_fraction, 0, 'f', 4)
                  .arg(placed)
                  .arg(totalNucleatedSoFar + placed);
    return placed;
}

// --- Isolated voxel cleanup --------------------------------------------
// After the frontier collapses, flood-fills any remaining empty voxels
// by assigning them the color of the nearest crystallized neighbor.
// This can only leave voxels unfilled if probability = 0 in all directions
// (e.g. isolated pockets with the current ellipsoid settings).
void Probability_Algorithm::fillIsolatedVoxels()
{
    if (filled_voxels == static_cast<unsigned int>(std::pow(numCubes, 3)))
        return;

    int resolved = 0;
    for (int x = 0; x < numCubes; ++x)
        for (int y = 0; y < numCubes; ++y)
            for (int z = 0; z < numCubes; ++z)
            {
                if (voxels[x][y][z] != 0) continue;

                for (const auto& off : PROBABILITY_OFFSETS)
                {
                    int nx = x + off[0], ny = y + off[1], nz = z + off[2];
                    if (nx < 0 || ny < 0 || nz < 0 ||
                        nx >= numCubes || ny >= numCubes || nz >= numCubes)
                        continue;
                    if (voxels[nx][ny][nz] == 0) continue;

                    voxels[x][y][z] = voxels[nx][ny][nz];
                    filled_voxels++;
                    resolved++;
                    break;
                }
            }

    if (resolved > 0)
        qDebug() << "fillIsolatedVoxels: resolved" << resolved << "orphan voxels";


}

// ---------------------------------------------------------------------------
// logIteration — visual progress bar with capture efficiency, rate, ETA
// ---------------------------------------------------------------------------
// Example output:
//   iter=0042  [==========>         ]  45.32%  cap=512  got=498(97%)  front=28.4k  2.1kvx/s  ETA=38s
// ---------------------------------------------------------------------------
void Probability_Algorithm::logIteration(unsigned int counter_max,
                                         unsigned int cap,
                                         unsigned int captured,
                                         size_t       active_size,
                                         size_t       frontier_before,
                                         double       iter_dt_s,
                                         double       elapsed_s,
                                         const QString& extra) const
{
    const double fill     = static_cast<double>(filled_voxels) / counter_max;
    const double pct      = fill * 100.0;
    const unsigned remaining = counter_max - filled_voxels;

    // --- Visual bar [20 chars] ------------------------------------------
    constexpr int BAR_W = 20;
    const int filled_chars = static_cast<int>(fill * BAR_W);
    QString bar(filled_chars,      '=');
    if (filled_chars < BAR_W) bar += '>';
    bar = bar.leftJustified(BAR_W, ' ');

    // --- Capture efficiency ---------------------------------------------
    const int cap_pct = (cap > 0)
                            ? static_cast<int>(100.0 * captured / cap)
                            : 0;

    // --- Throughput & ETA -----------------------------------------------
    // Use this-iteration time so the number reacts immediately to speed changes.
    const double rate    = (iter_dt_s > 1e-9) ? captured / iter_dt_s : 0.0;
    const double eta_s   = (rate > 0.0)       ? remaining / rate     : 0.0;

    auto fmtK = [](double v) -> QString {
        if (v >= 1e6) return QString::number(v / 1e6, 'f', 1) + "M";
        if (v >= 1e3) return QString::number(v / 1e3, 'f', 1) + "k";
        return QString::number(static_cast<int>(v));
    };

    auto fmtTime = [](double s) -> QString {
        if (s <= 0) return "?";
        if (s <= 30) return QString::number(static_cast<int>(s * 1000)) + "ms";
        if (s < 60)  return QString::number(static_cast<int>(s)) + "s";
        if (s < 3600) return QString::number(static_cast<int>(s / 60)) + "m"
                   + QString::number(static_cast<int>(s) % 60) + "s";
        return QString::number(static_cast<int>(s / 3600)) + "h"
               + QString::number(static_cast<int>(s / 60) % 60) + "m";
    };

    QString line = QString(
                       "iter=%1  [%2]  %3%"
                       "  cap=%4  got=%5(%6%)"
                       "  front=%7  active=%8"
                       "  %9vx/s  ETA=%10  t=%11")
                       .arg(this->IterationNumber,  4, 10, QChar('0'))
                       .arg(bar)
                       .arg(pct,         5, 'f', 2)
                       .arg(cap)
                       .arg(captured)
                       .arg(cap_pct,     3)
                       .arg(fmtK(static_cast<double>(grains.size())))
                       .arg(fmtK(static_cast<double>(active_size)))
                       .arg(fmtK(rate))
                       .arg(fmtTime(eta_s))
                       .arg(fmtTime(elapsed_s));

    if (!extra.isEmpty())
        line += "  " + extra;

    qDebug().noquote() << line;
}

void Probability_Algorithm::writeProbabilitiesToCSV(const QString& filePath, uint64_t N)
{
    QString tempFileName = filePath + QDir::separator() + "temp_N_" + QString::number(N) + ".csv";

    QFile tempFile(tempFileName);

    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "Error opening a temporary file for writing.";
        return;
    }

    QTextStream out(&tempFile);
    out << "X,Y,Z,Probability\n";

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                out << i << "," << j << "," << k << "," << probability[i][j][k] << "\n";
            }
        }
    }

    tempFile.close();

    QString finalFileName = filePath + QDir::separator() + "N_" + QString::number(N) + ".csv";

    QFile::remove(finalFileName);

    if (QFile::rename(tempFileName, finalFileName))
    {
        qDebug() << "The file has been successfully written to: " << finalFileName;
    }
    else
    {
        qDebug() << "Temporary file renaming error.";
    }
}


// ---------------------------------------------------------------------------
// Crystallisation history — collection
// ---------------------------------------------------------------------------

void Probability_Algorithm::recordIteration(unsigned int counter_max,
                                            unsigned int cap,
                                            unsigned int captured,
                                            size_t       active_size,
                                            int          nucleated_this_iter,
                                            int          total_nucleated)
{
    CrystallizationRecord rec;
    rec.iteration           = IterationNumber;
    rec.fill_fraction       = static_cast<double>(filled_voxels) / counter_max;
    rec.captured            = captured;
    rec.cap                 = cap;
    rec.cap_utilization     = (cap > 0)
                              ? static_cast<float>(captured) / cap
                              : 0.0f;
    rec.frontier_size       = grains.size();
    rec.active_size         = active_size;
    rec.nucleated_this_iter = nucleated_this_iter;
    rec.total_nucleated     = total_nucleated;
    m_history.push_back(rec);
}

// ---------------------------------------------------------------------------
// Crystallisation history — CSV export
// ---------------------------------------------------------------------------

void Probability_Algorithm::writeHistoryToCSV(const QString& dirPath) const
{

    if (m_history.empty())
    {
        qWarning() << "writeHistoryToCSV: history is empty, nothing to write";
        return;
    }

    const QString path =
        dirPath + QDir::separator() + "crystallization_history.csv";

    qDebug() << "Write crystaliztion log to:" << path;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "writeHistoryToCSV: cannot open" << path;
        return;
    }

    QTextStream out(&f);
    out << "iteration;"
        << "fill_fraction;"
        << "captured;"
        << "cap;"
        << "cap_utilization;"
        << "frontier_size;"
        << "active_size;"
        << "nucleated_this_iter;"
        << "total_nucleated\n";

    for (const auto& r : m_history)
    {
        out << r.iteration           << ";"
            << r.fill_fraction       << ";"
            << r.captured            << ";"
            << r.cap                 << ";"
            << r.cap_utilization     << ";"
            << r.frontier_size       << ";"
            << r.active_size         << ";"
            << r.nucleated_this_iter << ";"
            << r.total_nucleated     << "\n";
    }

    f.close();
    qDebug() << "Crystallization history written to" << path
             << "(" << m_history.size() << "iterations)";
}

#include "dlca.h"
#include "algorithmplugin.h"
#include "parameters.h"
#include "lowdiscrepancy.hpp"
#include "periodic_wrap.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <numeric>
#include <random>
#include <QDebug>

using namespace std;

DLCA_Aggregate::DLCA_Aggregate(int32_t*** voxels, int cubeSize)
{
    this->cubeSize = cubeSize;
    this->voxels = voxels;
}

void DLCA_Aggregate::move_aggregate(int dx, int dy, int dz)
{
    for (size_t i = 0; i < this->aggr.size(); i++)
    {
        aggr[i].x = (aggr[i].x + dx + cubeSize) % cubeSize;
        aggr[i].y = (aggr[i].y + dy + cubeSize) % cubeSize;
        aggr[i].z = (aggr[i].z + dz + cubeSize) % cubeSize;
    }
}

bool DLCA_Aggregate::is_can_move_aggregate(int dx, int dy, int dz)
{
    for (size_t i = 0; i < this->aggr.size(); i++)
    {
        int32_t x = (aggr[i].x + dx + cubeSize) % cubeSize;
        int32_t y = (aggr[i].y + dy + cubeSize) % cubeSize;
        int32_t z = (aggr[i].z + dz + cubeSize) % cubeSize;

        if (voxels[x][y][z] != 0 && voxels[x][y][z] != this->id)
            return false;
    }
    return true;
}

void DLCA_Aggregate::shift_to_cube_center(int cubeSize)
{
    DLCA::Coordinate cm = calculate_center_of_mass();
    int dx = (cubeSize / 2 - cm.x + cubeSize) % cubeSize;
    int dy = (cubeSize / 2 - cm.y + cubeSize) % cubeSize;
    int dz = (cubeSize / 2 - cm.z + cubeSize) % cubeSize;
    move_aggregate(dx, dy, dz);
}

Parent_Algorithm::Coordinate DLCA_Aggregate::calculate_center_of_mass() const
{
    int64_t sum_x = 0, sum_y = 0, sum_z = 0;
    size_t N = aggr.size();
    if (N == 0) return {0, 0, 0};
    for (const auto &c : aggr) {
        sum_x += c.x;
        sum_y += c.y;
        sum_z += c.z;
    }
    DLCA::Coordinate cm;
    cm.x = static_cast<int32_t>(sum_x / N);
    cm.y = static_cast<int32_t>(sum_y / N);
    cm.z = static_cast<int32_t>(sum_z / N);
    return cm;
}

CoordinateDouble DLCA_Aggregate::calculate_exact_center_of_mass() const
{
    if (aggr.empty()) {
        return {0.0, 0.0, 0.0};
    }

    double ref_x = aggr[0].x;
    double ref_y = aggr[0].y;
    double ref_z = aggr[0].z;

    double sum_dx = 0.0;
    double sum_dy = 0.0;
    double sum_dz = 0.0;

    double halfCube = this->cubeSize / 2.0;

    for (const auto &c : aggr) {
        double dx = c.x - ref_x;
        double dy = c.y - ref_y;
        double dz = c.z - ref_z;

        if (dx > halfCube)  dx -= this->cubeSize;
        if (dx < -halfCube) dx += this->cubeSize;

        if (dy > halfCube)  dy -= this->cubeSize;
        if (dy < -halfCube) dy += this->cubeSize;

        if (dz > halfCube)  dz -= this->cubeSize;
        if (dz < -halfCube) dz += this->cubeSize;

        sum_dx += dx;
        sum_dy += dy;
        sum_dz += dz;
    }

    double N = static_cast<double>(aggr.size());

    CoordinateDouble cm;
    cm.x = ref_x + (sum_dx / N) + 0.5;
    cm.y = ref_y + (sum_dy / N) + 0.5;
    cm.z = ref_z + (sum_dz / N) + 0.5;

    if (cm.x < 0) cm.x += this->cubeSize; else if (cm.x >= this->cubeSize) cm.x -= this->cubeSize;
    if (cm.y < 0) cm.y += this->cubeSize; else if (cm.y >= this->cubeSize) cm.y -= this->cubeSize;
    if (cm.z < 0) cm.z += this->cubeSize; else if (cm.z >= this->cubeSize) cm.z -= this->cubeSize;

    return cm;
}

void DLCA_Aggregate::map_to_voxels()
{
    for (size_t i = 0; i < aggr.size(); i++)
    {
        DLCA::Coordinate c = aggr[i];
        voxels[c.x][c.y][c.z] = this->id;
    }
}

void DLCA_Aggregate::absorb(DLCA_Aggregate &other)
{
    // Only the absorbed voxels change owner, so the cost is the size of `other`
    if (voxels)
        for (const auto &c : other.aggr)
            voxels[c.x][c.y][c.z] = this->id;
    aggr.insert(aggr.end(), other.aggr.begin(), other.aggr.end());
    other.aggr.clear();
}

void DLCA::random_walk()
{
    // Choose a diffuse direction at random (26 choices)
    int dx, dy, dz;
    uniform_int_distribution<int> int_distro(-1, 1);
    for (size_t i = 0; i < this->aggregates.size(); i++)
    {
        do {
            dx = int_distro(m_rng);
            dy = int_distro(m_rng);
            dz = int_distro(m_rng);
        } while (dx == 0 && dy == 0 && dz == 0);

        if (!aggregates[i].is_can_move_aggregate(dx, dy, dz)) {
            dx = -dx;
            dy = -dy;
            dz = -dz;
            if (!aggregates[i].is_can_move_aggregate(dx, dy, dz))
                continue;
        }

        // Update the grid right away so the next aggregate's check sees this move
        for (const Coordinate& c : aggregates[i].aggr)
            voxels[c.x][c.y][c.z] = 0;
        aggregates[i].move_aggregate(dx, dy, dz);
        aggregates[i].map_to_voxels();
    }
}

DLCA::DLCA()
{
}

DLCA::DLCA(int cubeSize)
{
    this->cubeSize = cubeSize;
}

DLCA::DLCA(short int numCubes, int numColors)
{
    cubeSize = numCubes;
    this->numCubes = numCubes;
    this->numColors = numColors;
}

void DLCA::saveSeeds()
{
}

void DLCA::CleanUp()
{
    Parent_Algorithm::CleanUp();
    aggregates.clear();
    aggregates.shrink_to_fit();
}

void DLCA::Initialization(bool /*isWaveGeneration*/)
{
    m_rng.seed(Parameters::seed);

    aggregates.clear();
    seedPoints.clear();
    IterationNumber = 0;

    std::ofstream file("crystallization_seeds.csv");
    if (file.is_open()) {
        file << "x,y,z,color\n";
    }

    int successfully_placed = 0;

    // Sobol: point i seeds aggregate i + 1 (the points are unique voxels)
    std::vector<std::array<int, 3>> sobolPts;
    if (Parameters::seed_method == Parameters::Sobol) {
        try {
            sobolPts = qmc::seedVoxels(numColors, numCubes, Parameters::seed, Parameters::seed_sigma);
        } catch (const std::exception& e) {
            qWarning() << "Sobol seed generation failed:" << e.what();
        }
    }

    for (int i = 0; i < numColors; i++)
    {
        Coordinate a;
        if (i < static_cast<int>(sobolPts.size())) {
            a = { sobolPts[i][0], sobolPts[i][1], sobolPts[i][2] };
        } else {
            int num_tries = 10;
            do {
                a = randomCoord();
                num_tries--;
            } while (voxels[a.x][a.y][a.z] != 0 && num_tries > 0);
        }

        if (voxels[a.x][a.y][a.z] != 0) {
            bool found = false;
            for (int x = 0; x < numCubes && !found; ++x)
                for (int y = 0; y < numCubes && !found; ++y)
                    for (int z = 0; z < numCubes && !found; ++z)
                        if (voxels[x][y][z] == 0) {
                            a = {x, y, z};
                            found = true;
                        }
            if (!found) break;
        }

        voxels[a.x][a.y][a.z] = i + 1;
        seedPoints.push_back(a);
        if (file.is_open()) {
            file << a.x << "," << a.y << "," << a.z << "," << (i + 1) << "\n";
        }
        successfully_placed++;

        DLCA_Aggregate aggr(voxels, numCubes);
        aggr.id = i + 1;
        aggr.aggr.push_back(a);
        this->aggregates.push_back(aggr);
    }

    if (file.is_open()) {
        file.close();
    }

    m_prevClusters = aggregates.size();

    qDebug().noquote()
                << QString("[DLCA] %1^3 grid (%2 voxels), %3 of %4 initial particles placed")
               .arg(numCubes)
               .arg(static_cast<uint64_t>(numCubes) * numCubes * numCubes)
               .arg(aggregates.size())
               .arg(successfully_placed);

    // A single particle is already the final cluster: Next_Iteration never runs
    if (getDone())
        reportPercolation();
}

void DLCA::reportPercolation()
{
    const int rank = aggregates.empty() ? 0 : periodic::wrapRank6(aggregates[0].aggr, cubeSize);
    Parameters::percolation_rank6 = rank;

    qDebug().noquote() << QString("[DLCA] periodic wrap rank (6-conn) = %1").arg(rank);
    if (rank < 3)
        qWarning("[DLCA] cluster does not percolate in 3D: periodic wrap rank (6-conn) = %d, "
                 "load paths through the cell rely on edge/corner contacts", rank);
}

// Distance along one axis to the nearest periodic image
static inline int32_t min_image_dist(int32_t a, int32_t b, int32_t size) {
    const int32_t d = std::abs(a - b);
    return std::min(d, size - d);
}

// Pairwise reference test of a face contact between two aggregates, O(|a1| * |a2|).
// Generation finds contacts through the voxel grid instead (merge_touching_aggregates);
// this predicate is kept as the definition the grid search is checked against.
bool DLCA::check_collision(size_t _i, size_t _j)
{
    auto &a1 = this->aggregates[_i];
    auto &a2 = this->aggregates[_j];

    const size_t a1_size = a1.aggr.size();
    const size_t a2_size = a2.aggr.size();

    for (size_t i = 0; i < a1_size; i++)
        for (size_t j = 0; j < a2_size; j++)
        {
            const DLCA::Coordinate &c1 = a1.aggr[i];
            const DLCA::Coordinate &c2 = a2.aggr[j];

            int dist_ij = min_image_dist(c1.x, c2.x, cubeSize) +
                          min_image_dist(c1.y, c2.y, cubeSize) +
                          min_image_dist(c1.z, c2.z, cubeSize);

            if (dist_ij == 1)
                return true;
        }
    return false;
}

void DLCA::join_aggregates(size_t _i, size_t _j)
{
    auto& a1 = this->aggregates[_i];
    auto& a2 = this->aggregates[_j];

    // The smaller aggregate joins the larger one; on a tie the second one survives
    if (a1.aggr.size() > a2.aggr.size())
        a1.absorb(a2);
    else
        a2.absorb(a1);
}

// Union-find root lookup with path halving
static size_t dsu_find(std::vector<size_t> &parent, size_t x)
{
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
    }
    return x;
}

void DLCA::merge_touching_aggregates()
{
    const size_t K = aggregates.size();

    // Aggregate id -> position in `aggregates`
    std::vector<size_t> slot(static_cast<size_t>(numColors) + 1, 0);
    for (size_t k = 0; k < K; ++k)
        slot[static_cast<size_t>(aggregates[k].id)] = k;

    std::vector<size_t> parent(K);
    std::iota(parent.begin(), parent.end(), size_t(0));

    // Face contact = a +x, +y or +z neighbour owned by another aggregate. Every
    // face-adjacent voxel pair is seen exactly once from its lower side, periodic
    // images included, which is the contact rule of check_collision.
    const int L = cubeSize;
    for (size_t k = 0; k < K; ++k) {
        const int32_t own = aggregates[k].id;
        for (const Coordinate &c : aggregates[k].aggr) {
            const int32_t nb[3] = { voxels[(c.x + 1) % L][c.y][c.z],
                                    voxels[c.x][(c.y + 1) % L][c.z],
                                    voxels[c.x][c.y][(c.z + 1) % L] };
            for (int32_t other : nb) {
                if (other == 0 || other == own)
                    continue;
                const size_t a = dsu_find(parent, k);
                const size_t b = dsu_find(parent, slot[static_cast<size_t>(other)]);
                if (a != b)
                    parent[a] = b;
            }
        }
    }

    // Each connected group of touching aggregates becomes one aggregate: the
    // largest member of the group absorbs the others
    std::vector<size_t> target(K, K);
    for (size_t k = 0; k < K; ++k) {
        const size_t r = dsu_find(parent, k);
        if (target[r] == K || aggregates[k].aggr.size() > aggregates[target[r]].aggr.size())
            target[r] = k;
    }
    for (size_t k = 0; k < K; ++k) {
        const size_t t = target[dsu_find(parent, k)];
        if (t != k)
            aggregates[t].absorb(aggregates[k]);
    }

    aggregates.erase(std::remove_if(aggregates.begin(), aggregates.end(),
                                    [](const DLCA_Aggregate &a) { return a.aggr.empty(); }),
                     aggregates.end());
}

void DLCA::Next_Iteration()
{
    // 1. Every aggregate makes one lattice step; the grid is updated move by move
    random_walk();

    // 2. All aggregates in face contact after the step stick together irreversibly
    merge_touching_aggregates();

    IterationNumber++;
    const size_t currentClusters = aggregates.size();

    if (getDone()) {
        const size_t clusterSize = aggregates.empty() ? 0 : aggregates[0].aggr.size();
        qDebug().noquote()
            << QString("[DLCA] Step %1: 1 cluster remaining (%2 voxels) | aggregation complete")
                   .arg(IterationNumber, 2)
                   .arg(clusterSize);
        reportPercolation();
    } else if (currentClusters != m_prevClusters || IterationNumber % 50 == 0) {
        qDebug().noquote()
            << QString("[DLCA] Step %1: %2 clusters remaining")
                   .arg(IterationNumber, 2)
                   .arg(currentClusters);
        m_prevClusters = currentClusters;
    }
}

void DLCA::Generate_To_End()
{
    while (!this->getDone())
        this->Next_Iteration();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Plugin registration
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<ParamField> dlcaSchema()
{
    std::vector<ParamField> s = {
        { "size",   "Cube size", ParamField::Int, 10, 1, 500, {}, "main" },
        { "points", "Points",    ParamField::PointsMode, 10, 1, 100000,
         { "Size", "Concentration" }, "main" },
    };

    s.push_back(materialParamField());

    const std::vector<ParamField> tex = textureParamFields();
    s.insert(s.end(), tex.begin(), tex.end());

    return s;
}

MATVIZ_REGISTER_ALGORITHM(AlgorithmPlugin{
    "DLCA",
    "Diffusion-Limited Cluster Aggregation (DLCA) cluster growth, "
    "with material and texture selection.",
    /*order=*/ 3,
    dlcaSchema(),
    [](const Parameters& p) {
        return std::make_shared<DLCA>(static_cast<short int>(p.getSize()), p.getPoints());
    }
});




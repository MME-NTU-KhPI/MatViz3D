#include "dlca.h"
#include "algorithmplugin.h"
#include "parameters.h"
#include <fstream>
#include <random>
#include <QDebug>
#include <limits.h>

using namespace std;

struct HashGrid {
    int cellSize;
    std::unordered_map<int64_t, std::vector<size_t>> grid; // Map from hashed cell index to aggregate indices

    HashGrid(int cellSize) : cellSize(cellSize) {}

    // Compute hash for a 3D cell
    int64_t hash(int x, int y, int z) const {
        return (static_cast<int64_t>(x) << 40) | (static_cast<int64_t>(y) << 20) | static_cast<int64_t>(z);
    }

    // Add aggregate to the grid
    void insert(const DLCA_Aggregate &aggregate, size_t index) {
        for (const auto &coord : aggregate.aggr) {
            int cellX = coord.x;
            int cellY = coord.y;
            int cellZ = coord.z;
            int64_t h = hash(cellX, cellY, cellZ);
            grid[h].push_back(index);
        }
    }

    // Query neighboring cells for potential collisions
    std::vector<size_t> query(int x, int y, int z) const {
        std::vector<size_t> neighbors;
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    int64_t h = hash(x + dx, y + dy, z + dz);
                    if (grid.count(h)) {
                        neighbors.insert(neighbors.end(), grid.at(h).begin(), grid.at(h).end());
                    }
                }
            }
        }
        return neighbors;
    }

    // Clear the grid for the next update
    void clear() {
        grid.clear();
    }
};

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

        if (aggregates[i].is_can_move_aggregate(dx, dy, dz))
            aggregates[i].move_aggregate(dx, dy, dz);
        else if (aggregates[i].is_can_move_aggregate(-dx, -dy, -dz))
            aggregates[i].move_aggregate(-dx, -dy, -dz);
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

    for (int i = 0; i < numColors; i++)
    {
        Coordinate a;
        int num_tries = 10;
        do {
            a = randomCoord();
            num_tries--;
        } while (voxels[a.x][a.y][a.z] != 0 && num_tries > 0);

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
        << QString("[DLCA] %1^3 grid (%2 voxels), %3 initial particles")
               .arg(numCubes)
               .arg(static_cast<uint64_t>(numCubes) * numCubes * numCubes)
               .arg(aggregates.size());
}

static inline int32_t my_abs(int32_t a) {
    int32_t mask = (a >> (sizeof(int32_t) * CHAR_BIT - 1));
    return (a + mask) ^ mask;
}

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

            int dist_ij = my_abs(c1.x - c2.x) +
                          my_abs(c1.y - c2.y) +
                          my_abs(c1.z - c2.z);

            if (dist_ij == 1)
                return true;
        }
    return false;
}

void DLCA::join_aggregates(size_t _i, size_t _j)
{
    auto& a1 = this->aggregates[_i];
    auto& a2 = this->aggregates[_j];

    if (a1.aggr.size() > a2.aggr.size())
    {
        a1.aggr.insert(a1.aggr.end(), a2.aggr.begin(), a2.aggr.end());
        a2.aggr.clear();
    }
    else
    {
        a2.aggr.insert(a2.aggr.end(), a1.aggr.begin(), a1.aggr.end());
        a1.aggr.clear();
    }
}

void DLCA::Next_Iteration()
{
    this->random_walk();

    for (size_t i = 0; i < this->aggregates.size(); i++)
        for (size_t j = i + 1; j < this->aggregates.size(); j++)
        {
            if (check_collision(i, j))
            {
                join_aggregates(i, j);
            }
        }
    this->aggregates.erase(std::remove_if(this->aggregates.begin(), this->aggregates.end(),
                                          [](const DLCA_Aggregate& a)
                                          { return a.aggr.empty(); }), this->aggregates.end());

    // Clear and repopulate the voxel array
    #pragma omp parallel for collapse(3)
    for (int i = 0; i < cubeSize; ++i)
        for (int j = 0; j < cubeSize; ++j)
            for (int k = 0; k < cubeSize; ++k)
                voxels[i][j][k] = 0;

    for (size_t i = 0; i < this->aggregates.size(); i++)
    {
        this->aggregates[i].map_to_voxels();
    }

    IterationNumber++;
    const size_t currentClusters = aggregates.size();

    if (getDone()) {
        const size_t clusterSize = aggregates.empty() ? 0 : aggregates[0].aggr.size();
        qDebug().noquote()
            << QString("[DLCA] Step %1: 1 cluster remaining (%2 voxels) | aggregation complete")
                   .arg(IterationNumber, 2)
                   .arg(clusterSize);
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
    /*order=*/ 5,
    dlcaSchema(),
    [](const Parameters& p) {
        return std::make_shared<DLCA>(static_cast<short int>(p.getSize()), p.getPoints());
    }
});

#include "dlca.h"
#include <random>
#include <QDebug>

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
        //voxels[aggr[i].x][aggr[i].y][aggr[i].z] = 0;
        aggr[i].x = (aggr[i].x + dx + cubeSize) % cubeSize;
        aggr[i].y = (aggr[i].y + dy + cubeSize) % cubeSize;
        aggr[i].z = (aggr[i].z + dz + cubeSize) % cubeSize;
        //voxels[aggr[i].x][aggr[i].y][aggr[i].z] = this->id; // Update new position
    }
}

bool DLCA_Aggregate::is_can_move_aggregate(int dx, int dy, int dz)
{
    for (size_t i = 0; i < this->aggr.size(); i++)
    {
        int32_t x = (aggr[i].x + dx + cubeSize) % cubeSize; // get new x position
        int32_t y = (aggr[i].y + dy + cubeSize) % cubeSize; // get new y position
        int32_t z = (aggr[i].z + dz + cubeSize) % cubeSize; // get new z position

        if (voxels[x][y][z] != 0 && voxels[x][y][z] != this->id) // is this a free cell?
            return false;
    }
    return true;
}

void DLCA_Aggregate::map_to_voxels()
{
    // Clear and repopulate the voxel array
    #pragma omp parallel for collapse(3)
    for (int i = 0; i < cubeSize; ++i)
        for (int j = 0; j < cubeSize; ++j)
            for (int k = 0; k < cubeSize; ++k)
                voxels[i][j][k] = 0;

    for (size_t i = 0; i < aggr.size(); i++)
    {
        DLCA::Coordinate c = aggr[i];
        voxels[c.x][c.y][c.z] = this->id;
    }
}


void DLCA::random_walk()
{
    std::random_device rd;
    std::mt19937 rand_engine(rd());

    // Choose a diffuse direction at random (26 choices)
    int dx, dy, dz;
    uniform_int_distribution<int> int_distro(-1, 1);
    for (size_t i = 0; i < this->aggregates.size(); i++)
    {
        do {
            dx = int_distro(rand_engine);
            dy = int_distro(rand_engine);
            dz = int_distro(rand_engine);
        } while (dx == 0 && dy == 0 && dz == 0);

        if (aggregates[i].is_can_move_aggregate(dx, dy, dz))
            aggregates[i].move_aggregate(dx, dy, dz);
        else // check in reverse direction after collision
            if (aggregates[i].is_can_move_aggregate(-dx, -dy, -dz))
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

void DLCA::Initialization(bool isWaveGeneration)
{
    Q_UNUSED(isWaveGeneration); // this paramter actual only for polycrystall material
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, numCubes - 1);

    Coordinate a;

    for (int i = 0; i < numColors; i++)
    {
        int num_tries = 5;
        do
        {
            a.x = distribution(generator);
            a.y = distribution(generator);
            a.z = distribution(generator);
            num_tries--;
        }
        while (voxels[a.x][a.y][a.z] != 0 && num_tries > 0);

        voxels[a.x][a.y][a.z] = i + 1; // add new cell to voxel array

        if (num_tries == 0) //possible no free space, skip iteration
        {
            qDebug() << "Error: cannot find free cell to add new one";
            qDebug() << a.x << a.y << a.z << i;
            continue;
        }
        DLCA_Aggregate aggr(voxels, numCubes);
        aggr.id = i + 1;
        aggr.aggr.push_back(a);
        this->aggregates.push_back(aggr);
    }
}

#include <limits.h>

int32_t my_abs(int32_t a) {
    int32_t mask = (a >> (sizeof(int32_t) * CHAR_BIT - 1));
    return (a + mask) ^ mask;
}

bool DLCA::check_collision(size_t _i, size_t _j)
{
    auto &a1 = this->aggregates[_i];
    auto &a2 = this->aggregates[_j];

    int dist = cubeSize;
    int dist_ij = 0;
    const size_t a1_size = a1.aggr.size();
    const size_t a2_size = a2.aggr.size();

    for (size_t i = 0; i < a1_size; i++)
        for (size_t j = 0; j < a2_size; j++)
        {
            const DLCA::Coordinate &c1 = a1.aggr[i];
            const DLCA::Coordinate &c2 = a2.aggr[j];

            dist_ij = my_abs(c1.x - c2.x) +
                      my_abs(c1.y - c2.y) +
                      my_abs(c1.z - c2.z);

            dist = min(dist, dist_ij);
        }
    return dist == 1;
}

void DLCA::join_aggregates(size_t _i, size_t _j)
{
    auto& a1 = this->aggregates[_i];
    auto& a2 = this->aggregates[_j];

    if (a1.aggr.size() > a2.aggr.size()) // which aggregate bigger?
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

void DLCA::Next_Iteration(std::function<void()> callback)
{
    this->Generate_Filling_With_Spatial_Hashing();
    if (flags.isAnimation)
    {
        callback();
    }
    return;
    /*
    if (this->aggregates.size() > 1)
    {
        grains.push_back({0,0,0});
        // qDebug() << this->aggregates.size();
        if (this->aggregates.size() < 5)
            for (size_t i = 0; i < this->aggregates.size(); i++)
                qDebug() << i << this->aggregates[i].id << this->aggregates[i].aggr.size();
    }
    this->random_walk();

    for (size_t i = 0; i < this->aggregates.size(); i++)
        for (size_t j = i + 1; j < this->aggregates.size(); j++)
        {
            bool status = check_collision(i, j);
            if (status == true)
            {
                qDebug() << "Collision detected" << i << j;
                join_aggregates(i, j);
            }
        }
    this->aggregates.erase(std::remove_if(this->aggregates.begin(), this->aggregates.end(),
                                          [](const DLCA_Aggregate a)
                                          { return a.aggr.size()==0; }), this->aggregates.end());

    for (int i = 0; i < cubeSize; i++)
        for (int j = 0; j < cubeSize; j++)
            for (int k = 0; k < cubeSize; k++)
                voxels[i][j][k] = 0;

    for (size_t i = 0; i < this->aggregates.size(); i++)
    {
        aggregates[i].map_to_voxels();
    }
*/
}


void DLCA::Generate_Filling_With_Spatial_Hashing()
{
    HashGrid hashGrid(cubeSize);

    // Populate the spatial hash grid with aggregates
    for (size_t i = 0; i < aggregates.size(); ++i) {
        hashGrid.insert(aggregates[i], i);
    }

    // Random walk for aggregates
    this->random_walk();

    // Check collisions using spatial hashing
    std::vector<bool> joined(aggregates.size(), false);
    for (size_t i = 0; i < aggregates.size(); ++i) {
        if (joined[i]) continue;
        for (const auto &neighborIndex : hashGrid.query(aggregates[i].aggr[0].x,
                                                        aggregates[i].aggr[0].y,
                                                        aggregates[i].aggr[0].z)) {
            if (i != neighborIndex && !joined[neighborIndex] && check_collision(i, neighborIndex)) {
                join_aggregates(i, neighborIndex);
                joined[neighborIndex] = true;
            }
        }
    }

    // Clean up empty aggregates
    this->aggregates.erase(std::remove_if(this->aggregates.begin(), this->aggregates.end(),
                                          [](const DLCA_Aggregate &a) { return a.aggr.empty(); }), this->aggregates.end());


    for (auto &aggregate : aggregates) {
        aggregate.map_to_voxels();
    }

    if (aggregates.size() == 1)
        grains.clear();

    hashGrid.clear();
}

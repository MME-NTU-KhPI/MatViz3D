#include "dlca.h"
#include <random>
#include <QDebug>

using namespace std;

int periodic(int coord, int cubeSize) {
    // Assume x and y are not too far away for the grid
    if (coord < 0) {
        return coord + cubeSize;
    }
    else if (coord >= cubeSize) {
        return coord - cubeSize;
    }
    else {
        return coord;
    }
}

DLCA_Aggregate::DLCA_Aggregate(int16_t*** voxels, int cubeSize)
{
    this->cubeSize = cubeSize;
    this->voxels = voxels;
}


void DLCA_Aggregate::move_aggregate(int dx, int dy, int dz)
{
    for (size_t i = 0; i < this->aggr.size(); i++)
    {
        // Update particle position
        //voxels[aggr[i].x][aggr[i].y][aggr[i].z] = 0;
        aggr[i].x = (aggr[i].x + dx + cubeSize) % cubeSize; // Update x position
        aggr[i].y = (aggr[i].y + dy + cubeSize) % cubeSize; // Update y position
        aggr[i].z = (aggr[i].z + dz + cubeSize) % cubeSize; // Update z position
        //voxels[aggr[i].x][aggr[i].y][aggr[i].z] = this->id; // Update new position
    }
}
void DLCA_Aggregate::map_to_voxels(int16_t*** voxels, short int cubeSize)
{

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
        aggregates[i].move_aggregate(dx, dy, dz);
    }


}

DLCA::DLCA()
{

}
DLCA::DLCA(int cubeSize)
{
    this->cubeSize = cubeSize;
}

std::vector<Parent_Algorithm::Coordinate> DLCA::Generate_Random_Starting_Points(int16_t*** voxels,short int numCubes, int numColors)
{
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, numCubes - 1);

    Coordinate a;

    std::vector<Coordinate> grains;
    grains.push_back({0,0,0});
    for (int i = 0; i < numColors; i++)
    {
        a.x = distribution(generator);
        a.y = distribution(generator);
        a.z = distribution(generator);
        DLCA_Aggregate aggr(voxels, numCubes);
        aggr.id = i + 1;
        aggr.aggr.push_back(a);
        this->aggregates.push_back(aggr);
    }
    return grains;
}

bool DLCA::check_collision(size_t _i, size_t _j)
{
    auto a1 = this->aggregates[_i];
    auto a2 = this->aggregates[_j];

    int dist = cubeSize;

    for (size_t i = 0; i < a1.aggr.size(); i++)
        for (size_t j = 0; j < a2.aggr.size(); j++)
        {
            int dist_ij = abs(a1.aggr[i].x - a2.aggr[j].x) +
                          abs(a1.aggr[i].y - a2.aggr[j].y) +
                          abs(a1.aggr[i].z - a2.aggr[j].z);

            dist = min(dist, dist_ij);
        }
    return dist == 1;
}

void DLCA::join_aggregates(size_t _i, size_t _j)
{
    auto& a1 = this->aggregates[_i];
    auto& a2 = this->aggregates[_j];
    for (size_t j = 0; j < a2.aggr.size(); j++)
    {
        a1.aggr.push_back(a2.aggr[j]);
    }
    a2.aggr.clear();
}

std::vector<Parent_Algorithm::Coordinate> DLCA::Generate_Filling(int16_t*** voxels, short int numCubes,int n,std::vector<Coordinate> grains)
{
    std::vector<Coordinate> v;
    if (this->aggregates.size() > 1)
    {
        v.push_back({0,0,0});
        qDebug() << this->aggregates.size();
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
        aggregates[i].map_to_voxels(voxels, cubeSize);
    }

    return v;
}
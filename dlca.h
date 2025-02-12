#ifndef DLCA_H
#define DLCA_H

#include <vector>
#include "parent_algorithm.h"

class DLCA_Aggregate
{

    int cubeSize;
    int32_t*** voxels;
public:
    int id;
    std::vector <Parent_Algorithm::Coordinate> aggr;
    void move_aggregate(int dx, int dy, int dz);
    void map_to_voxels();
    bool is_can_move_aggregate(int dx, int dy, int dz);
    DLCA_Aggregate(int32_t*** voxels, int cubeSize);
};

class DLCA : public Parent_Algorithm {
    DLCA();
    int cubeSize;
public:
    DLCA(short int numCubes, int numColors);
    DLCA(int cubeSize);
    void Next_Iteration() override;
    void Initialization(bool isWaveGeneration);
    void random_walk();
    std::vector<DLCA_Aggregate> aggregates;
    bool check_collision(size_t i, size_t j);
    void join_aggregates(size_t _i, size_t _j);

};

#endif // DLCA_H

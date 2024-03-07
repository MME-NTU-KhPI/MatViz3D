#ifndef DLCA_H
#define DLCA_H

#include <vector>
#include "parent_algorithm.h"

class DLCA_Aggregate
{

    int cubeSize;
    int16_t*** voxels;
public:
    int id;
    std::vector <Parent_Algorithm::Coordinate> aggr;
    void move_aggregate(int dx, int dy, int dz);
    void map_to_voxels(int16_t*** voxels, short int numCubes);

    DLCA_Aggregate(int16_t*** voxels, int cubeSize);
};

class DLCA : public Parent_Algorithm {
    DLCA();
    int cubeSize;
public:

    DLCA(int cubeSize);
    std::vector<Coordinate> Generate_Filling(int16_t*** voxels, short int numCubes,int n,std::vector<Coordinate> grains);
    std::vector<Coordinate> Generate_Random_Starting_Points(int16_t*** voxels,short int numCubes, int numColors);
    void random_walk();
    std::vector<DLCA_Aggregate> aggregates;
    bool check_collision(size_t i, size_t j);
    void join_aggregates(size_t _i, size_t _j);

};

#endif // DLCA_H

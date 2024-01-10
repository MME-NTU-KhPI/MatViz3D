
#ifndef RADIAL_H
#define RADIAL_H

#include "parent_algorithm.h"



class Radial : public Parent_Algorithm
{
public:
    Radial();
    std::vector<Coordinate> Generate_Filling(int16_t*** voxels, short int numCubes,int n, std::vector<Coordinate> grains);
};

#endif // RADIAL_H

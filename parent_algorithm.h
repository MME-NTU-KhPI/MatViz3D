
#ifndef PARENT_ALGORITHM_H
#define PARENT_ALGORITHM_H

#include <random>
#include <cstdint>

//class Probability_Circle;

class Parent_Algorithm
{
public:
    Parent_Algorithm();
    virtual bool Generate_Filling(int16_t*** voxels, short int numCubes,int n, std::vector<int16_t> grains) = 0;
    int16_t*** Generate_Initial_Cube(short int numCubes);
    std::vector<int16_t> Generate_Random_Starting_Points(int16_t*** voxels, short int numCubes, int numColors);
    bool Check(int16_t*** voxels,short int numCubes, bool answer,int n);
};

#endif // PARENT_ALGORITHM_H

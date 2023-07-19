
#ifndef PARENT_ALGORITHM_H
#define PARENT_ALGORITHM_H

#include <random>
#include <cstdint>

//class Probability_Circle;

class Parent_Algorithm
{
public:
    Parent_Algorithm();
    virtual bool Generate_Filling(int16_t*** voxels, short int numCubes,int n) = 0;
    int16_t*** Generate_Initial_Cube(short int numCubes, int numColors);
};

#endif // PARENT_ALGORITHM_H

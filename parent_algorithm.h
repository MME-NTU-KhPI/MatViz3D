
#ifndef PARENT_ALGORITHM_H
#define PARENT_ALGORITHM_H

#include "probability_circle.h"
#include <cstdint>

//class Probability_Circle;

class Parent_Algorithm
{
public:
    Parent_Algorithm();
    virtual void Generate_Filling(int16_t*** voxels, short int numCubes) = 0;
    int16_t*** Generate_Initial_Cube(short int numCubes, int numColors);
};

#endif // PARENT_ALGORITHM_H
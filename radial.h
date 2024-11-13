
#ifndef RADIAL_H
#define RADIAL_H

#include "parent_algorithm.h"
#include "parameters.h"



class Radial : public Parent_Algorithm
{
public:
    Radial();
    Radial(short int numCubes, int numColors);
    void Generate_Filling(int isAnimation, int isWaveGeneration);
};

#endif // RADIAL_H

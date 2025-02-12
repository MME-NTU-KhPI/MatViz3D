
#ifndef RADIAL_H
#define RADIAL_H

#include "parent_algorithm.h"



class Radial : public Parent_Algorithm
{
public:
    Radial();
    Radial(short int numCubes, int numColors);
    void Next_Iteration() override;
};

#endif // RADIAL_H

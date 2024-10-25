#ifndef COMPOSITE_H
#define COMPOSITE_H

#include <parent_algorithm.h>

class Composite : public Parent_Algorithm
{
public:
    Composite();
    Composite(short int numCubes, int numColors);
    void FillWithCylinder(int isAnimation, int isWaveGeneration, short int radius);
    void Generate_Filling(int isAnimation, int isWaveGeneration);
};

#endif // COMPOSITE_H

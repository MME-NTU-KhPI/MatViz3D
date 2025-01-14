#ifndef COMPOSITE_H
#define COMPOSITE_H

#include <parent_algorithm.h>

class Composite : public Parent_Algorithm
{
public:
    Composite();
    Composite(short int numCubes, int numColors);
    void setRadius(short int radius);
    void FillWithCylinder(bool isAnimation, bool isWaveGeneration);
    void FillWithTetra(bool isAnimation, bool isWaveGeneration, short int offsetX, short int offsetY, short int offsetZ);
    void FillWithHexa(bool isAnimation, bool isWaveGeneration, short int offsetX, short int offsetY, short int offsetZ);
    void Generate_Filling(bool isAnimation, bool isWaveGeneration, bool isPeriodicStructure);
private:
    short int radius;
};

#endif // COMPOSITE_H

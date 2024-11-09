#ifndef COMPOSITE_H
#define COMPOSITE_H

#include <parent_algorithm.h>

class Composite : public Parent_Algorithm
{
public:
    Composite();
    Composite(short int numCubes, int numColors);
    void setRadius(short int radius);
    void FillWithCylinder(int isAnimation, int isWaveGeneration);
    void FillWithTetra(int isAnimation, int isWaveGeneration, short int offsetX, short int offsetY, short int offsetZ);
    void FillWithHexa(int isAnimation, int isWaveGeneration, short int offsetX, short int offsetY, short int offsetZ);
    void Generate_Filling(int isAnimation, int isWaveGeneration);
private:
    short int radius;
};

#endif // COMPOSITE_H

#ifndef COMPOSITE_H
#define COMPOSITE_H

#include <parent_algorithm.h>

class Composite : public Parent_Algorithm
{
public:
    Composite();
    Composite(short int numCubes, int numColors);
    void setRadius(short int radius);
    void FillWithCylinder();
    void FillWithTetra();
    void FillWithHexa();
    void Generate_Filling();
private:
    short int radius;
};

#endif // COMPOSITE_H

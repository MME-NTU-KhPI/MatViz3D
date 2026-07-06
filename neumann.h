
#ifndef NEUMANN_H
#define NEUMANN_H

#include <QtOpenGLWidgets/QOpenGLWidget>
#include "parent_algorithm.h"



class Neumann : public Parent_Algorithm
{
public:
    Neumann();
    Neumann(short int numCubes, int numColors);
    void Next_Iteration() override;
    bool getDone() const override;
};

#endif // NEUMANN_H

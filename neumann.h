
#ifndef NEUMANN_H
#define NEUMANN_H

#include "myglwidget.h"
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <mainwindow.h>
#include "parent_algorithm.h"



class Neumann : public Parent_Algorithm
{
public:
    Neumann();
    Neumann(short int numCubes, int numColors);
    void Next_Iteration() override;
};

#endif // NEUMANN_H

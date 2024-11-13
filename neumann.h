
#ifndef NEUMANN_H
#define NEUMANN_H

#include "myglwidget.h"
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <mainwindow.h>
#include "parent_algorithm.h"
#include "parameters.h"



class Neumann : public Parent_Algorithm
{
public:
    Neumann();
    Neumann(short int numCubes, int numColors);
    void Generate_Filling(int isAnimation, int isWaveGeneration);
};

#endif // NEUMANN_H

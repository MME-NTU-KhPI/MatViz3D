
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
    bool Generate_Filling(int16_t*** voxels, short int numCubes);
};

#endif // NEUMANN_H

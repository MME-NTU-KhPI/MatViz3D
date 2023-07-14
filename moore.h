
#ifndef MOORE_H
#define MOORE_H

#include "myglwidget.h"
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <mainwindow.h>
#include "parent_algorithm.h"



class Moore : public Parent_Algorithm {

public:
    Moore();
    bool Generate_Filling(int16_t*** voxels, short int numCubes,int n);
};

#endif // MOORE_H

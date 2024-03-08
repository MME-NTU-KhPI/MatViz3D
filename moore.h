
#ifndef MOORE_H
#define MOORE_H

#include "myglwidget.h"
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <mainwindow.h>
#include "parent_algorithm.h"



class Moore : public Parent_Algorithm {

public:
    Moore();
    Moore(short int numCubes, int numColors);
    void Generate_Filling(int isAnimation, int isWaveGeneration);
};

#endif // MOORE_H

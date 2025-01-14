
#ifndef MOORE_H
#define MOORE_H

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <mainwindow.h>
#include "parent_algorithm.h"



class Moore : public Parent_Algorithm {

public:
    Moore();
    Moore(short int numCubes, int numColors);
    void Generate_Filling(bool isAnimation, bool isWaveGeneration, bool isPeriodicStructure);
};

#endif // MOORE_H

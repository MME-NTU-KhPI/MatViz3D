
#ifndef PROBABILITY_CIRCLE_H
#define PROBABILITY_CIRCLE_H

#include "myglwidget.h"
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <mainwindow.h>
#include "probability_ellipse.h"
#include "parent_algorithm.h"



class Probability_Circle : public Parent_Algorithm
{
public:
    Probability_Circle();

    //uint16_t*** Generate_Initial_Cube(short int numCubes, int numColors);
    //void Check(uint16_t*** voxels, short int numCubes);
    void Generate_Filling(uint16_t*** voxels, short int numCubes);

private:

};

#endif // PROBABILITY_CIRCLE_H

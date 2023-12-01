

#ifndef PROBABILITY_ELLIPSE_H
#define PROBABILITY_ELLIPSE_H

#include "myglwidget.h"
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <mainwindow.h>
#include "parent_algorithm.h"

class Probability_Ellipse : public Parent_Algorithm
{
public:
    Probability_Ellipse();
    std::vector<Coordinate> Generate_Filling(int16_t*** voxels, short int numCubes,int n, std::vector<Coordinate> grains);
};

#endif // PROBABILITY_ELLIPSE_H

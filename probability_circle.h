
#ifndef PROBABILITY_CIRCLE_H
#define PROBABILITY_CIRCLE_H

#include "myglwidget.h"
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <mainwindow.h>
#include "parent_algorithm.h"



class Probability_Circle : public Parent_Algorithm
{
public:
    Probability_Circle();
    std::vector<Coordinate> Generate_Filling(int16_t*** voxels, short int numCubes,int n, std::vector<Coordinate> grains);
    std::vector<Coordinate> Check (int16_t*** voxels, std::vector<Coordinate> grains, size_t i);

private:

};

#endif // PROBABILITY_CIRCLE_H

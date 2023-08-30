
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
    bool Generate_Filling(int16_t*** voxels, short int numCubes,int n, std::vector<int16_t> grains);

private:

};

#endif // PROBABILITY_CIRCLE_H


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
    void Generate_Filling(int16_t*** voxels, short int numCubes, MyGLWidget* myglwidget);

private:

};

#endif // PROBABILITY_CIRCLE_H

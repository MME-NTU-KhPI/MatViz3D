
#ifndef PROBABILITY_CIRCLE_H
#define PROBABILITY_CIRCLE_H

#include "myglwidget.h"
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <mainwindow.h>



class Probability_Circle
{
public:
    Probability_Circle();

    uint16_t*** Generate_Initial_Cube(short int numCubes);
    //void Check(uint16_t*** voxels, short int numCubes);
    void Generate_Filling(uint16_t*** voxels, short int numCubes);

private:

};

#endif // PROBABILITY_CIRCLE_H



#ifndef PROBABILITY_ELLIPSE_H
#define PROBABILITY_ELLIPSE_H

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <mainwindow.h>
#include "parent_algorithm.h"

class Probability_Ellipse : public Parent_Algorithm
{
public:
    Probability_Ellipse();
    Probability_Ellipse(short int numCubes, int numColors);
    void Generate_Filling(bool isAnimation, bool isWaveGeneration, bool isPeriodicStructure);
};

#endif // PROBABILITY_ELLIPSE_H

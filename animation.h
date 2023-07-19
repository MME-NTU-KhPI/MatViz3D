#ifndef ANIMATION_H
#define ANIMATION_H

#include <QThread>
#include <myglwidget.h>
#include "parent_algorithm.h"



class Animation : public QThread
{
public:
    Animation(bool& answer, int16_t*** voxels, int numCubes, int n, Parent_Algorithm* start, MyGLWidget* myGLWidget): answer(answer), voxels(voxels), numCubes(numCubes), n(n), begin(start), myGLWidget(myGLWidget){};
    void run() override;
private:
    bool& answer;
    int16_t*** voxels;
    int numCubes;
    int n;
    Parent_Algorithm* begin;
    MyGLWidget* myGLWidget;
};

#endif // ANIMATION_H


#include "animation.h"
#include <QApplication>

Animation::Animation()
{

}

Animation::Animation(int16_t*** a,Parent_Algorithm* b,MyGLWidget* c,int d,short int f,bool e,std::vector<Parent_Algorithm::Coordinate> grains)
{
    voxels = a;
    begin = b;
    myglwidget = c;
    n = d;
    numCubes = f;
    answer = e;
    this->grains = grains;
}

void Animation::animate()
{
    while (!grains.empty())
    {
        grains = begin->Generate_Filling(voxels,numCubes,n,grains);
        QApplication::processEvents();
        emit updateRequested(voxels,numCubes);
    }
}

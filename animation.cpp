
#include "animation.h"

Animation::Animation()
{

}

Animation::Animation(int16_t*** a,Parent_Algorithm* b,MyGLWidget* c,int d,short int f,bool e)
{
    voxels = a;
    begin = b;
    myglwidget = c;
    n = d;
    numCubes = f;
    answer = e;
}

void Animation::animate()
{
    while (answer)
    {
        answer = begin->Generate_Filling(voxels,numCubes,n);
        emit updateRequested(voxels,numCubes);
    }
}


#include "animation.h"
#include <QApplication>

Animation::Animation()
{

}

Animation::Animation(Parent_Algorithm* b,int d)
{
    begin = b;
    n = d;
}

void Animation::animate()
{
//    while (!begin->grains.empty())
//    {
//        begin->Generate_Filling(n);
//        QApplication::processEvents();
//        emit updateRequested(begin->voxels,begin->numCubes);
//    }
}


#include "animation.h"


void Animation::run()
{
    while (answer)
    {
        answer = begin->Generate_Filling(voxels, numCubes, n);
            QMetaObject::invokeMethod(myGLWidget, [=]() {
            myGLWidget->setVoxels(voxels, numCubes);
            myGLWidget->update();
        });
        QThread::msleep(1000);
    }
}

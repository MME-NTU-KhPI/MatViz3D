
#ifndef VONNEUMANN_H
#define VONNEUMANN_H

#include "probability_circle.h"
#include "myglwidget.h"
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <mainwindow.h>

//#include <QMainWindow>


class VonNeumann : public Probability_Circle
{
    Q_OBJECT
public:
    VonNeumann();

private:
    void Generate_Filling(uint16_t*** voxels, short int numCubes);
};



#endif // VONNEUMANN_H

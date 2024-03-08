
#ifndef ANIMATION_H
#define ANIMATION_H


#include <QObject>
#include <QTimer>
#include <QCoreApplication>
#include <myglwidget.h>
#include "parent_algorithm.h"


class Animation : public QObject
{
    Q_OBJECT
public:
    Animation();
    Animation(Parent_Algorithm* b,int d);
    void animate();
public slots:
private:
    Parent_Algorithm* begin;
    int n;
signals:
    void updateRequested(int16_t*** voxels, short int numCubes);
};

#endif // ANIMATION_H

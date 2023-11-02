
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
    Animation(int16_t*** a,Parent_Algorithm* b,MyGLWidget* c,int d,short int e,bool f,std::vector<Parent_Algorithm::Coordinate> grains);
    void animate();
public slots:
private:
    int16_t*** voxels;
    Parent_Algorithm* begin;
    MyGLWidget* myglwidget;
    int n;
    short int numCubes;
    bool answer;
    std::vector<Parent_Algorithm::Coordinate> grains;
signals:
    void updateRequested(int16_t*** voxels, short int numCubes);
};

#endif // ANIMATION_H

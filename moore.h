
#ifndef MOORE_H
#define MOORE_H

#include <QtOpenGLWidgets/QOpenGLWidget>
#include "parent_algorithm.h"



class Moore : public Parent_Algorithm {

public:
    Moore();
    Moore(short int numCubes, int numColors);
    void Next_Iteration(std::function<void()> callback) override;
};

#endif // MOORE_H

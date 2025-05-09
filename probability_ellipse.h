

#ifndef PROBABILITY_ELLIPSE_H
#define PROBABILITY_ELLIPSE_H

#include "parent_algorithm.h"

class Probability_Ellipse : public Parent_Algorithm
{
public:
    Probability_Ellipse();
    Probability_Ellipse(short int numCubes, int numColors);
    void Next_Iteration(std::function<void()> callback) override;
};

#endif // PROBABILITY_ELLIPSE_H

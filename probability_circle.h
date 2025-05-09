
#ifndef PROBABILITY_CIRCLE_H
#define PROBABILITY_CIRCLE_H

#include "parent_algorithm.h"



class Probability_Circle : public Parent_Algorithm
{
public:
    Probability_Circle();
    Probability_Circle(short int numCubes, int numColors);
    void Next_Iteration(std::function<void()> callback) override;
    std::vector<Coordinate> Check (int32_t*** voxels, std::vector<Coordinate> grains, size_t i);

private:

};

#endif // PROBABILITY_CIRCLE_H

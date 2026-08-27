
#ifndef RADIAL_H
#define RADIAL_H

#include "parent_algorithm.h"

/**
 * @brief Cellular automaton grain growth using 18-neighbor neighborhood.
 */
class Radial : public Parent_Algorithm
{
public:
    Radial();
    Radial(short int numCubes, int numColors);
    void Initialization(bool isWaveGeneration) override;
    void Next_Iteration() override;
    bool getDone() const override;
};

#endif // RADIAL_H

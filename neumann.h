
#ifndef NEUMANN_H
#define NEUMANN_H

#include "parent_algorithm.h"

/**
 * @brief Cellular automaton grain growth using 6-neighbor von Neumann neighborhood.
 */
class Neumann : public Parent_Algorithm
{
public:
    Neumann();
    Neumann(short int numCubes, int numColors);
    void Initialization(bool isWaveGeneration) override;
    void Next_Iteration() override;
    bool getDone() const override;
};

#endif // NEUMANN_H


#ifndef MOORE_H
#define MOORE_H

#include "parent_algorithm.h"

/**
 * @brief Cellular automaton grain growth using 26-neighbor Moore neighborhood.
 */
class Moore : public Parent_Algorithm {
public:
    Moore();
    Moore(short int numCubes, int numColors);
    void Initialization(bool isWaveGeneration) override;
    void Next_Iteration() override;
    bool getDone() const override;
};

#endif // MOORE_H

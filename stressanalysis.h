#ifndef STRESSANALYSIS_H
#define STRESSANALYSIS_H

#include <cstdint>
class StressAnalysis
{
public:
    static void estimateStressWithANSYS(short int numCubes, int16_t ***voxels);
};

#endif // STRESSANALYSIS_H

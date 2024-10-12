#ifndef STRESSANALYSIS_H
#define STRESSANALYSIS_H

#include <cstdint>
#include <ansysWrapper.h>

class StressAnalysis
{
public:
    void estimateStressWithANSYS(short int numCubes, short int numPoints, int32_t ***voxels);
    ansysWrapper* wr;
};

#endif // STRESSANALYSIS_H

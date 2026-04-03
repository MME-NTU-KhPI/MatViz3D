#ifndef STRESSANALYSIS_H
#define STRESSANALYSIS_H

#include <cstdint>
#include "ansyswrapper.h"

class StressAnalysis
{
public:
    void estimateStressWithANSYS(short int numCubes, short int numPoints, int32_t ***voxels);
    ansysWrapper* wr;
private:
    std::vector<std::vector<double>> generateDirectStrainLoads(int num_samples, double strain_val);
    std::vector<std::vector<double>> generateSmartStressLoads(int num_samples, double strain_val, short int numCubes, short int numPoints, int32_t ***voxels);
};

#endif // STRESSANALYSIS_H

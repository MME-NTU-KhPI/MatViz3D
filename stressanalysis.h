#ifndef STRESSANALYSIS_H
#define STRESSANALYSIS_H

#include <cstdint>
#include "ansyswrapper.h"
#include "hillcriterion.h"

class StressAnalysis
{
public:
    void estimateStressWithANSYS(short int numCubes, short int numPoints, int32_t ***voxels);
    ansysWrapper* wr;
private:
    HillCriterion m_hill;

    bool computeSMatrix(short int numCubes, short int numPoints, int32_t ***voxels,
                        double strain_val, double S_out[6][6]);

    bool calibrateHillMatrix(short int numCubes, short int numPoints, int32_t ***voxels,
                             double strain_val, const double S_matrix[6][6]);
};

#endif // STRESSANALYSIS_H

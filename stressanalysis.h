#ifndef STRESSANALYSIS_H
#define STRESSANALYSIS_H

#include <cstdint>
#include "ansyswrapper.h"
#include "hillcriterion.h"
#include "stressresult.h"

class StressAnalysis
{
public:
    void estimateStressWithANSYS(short int numCubes, short int numPoints, int32_t ***voxels);

    // Solve a single known load case (macro strain, pipeline order
    // [exx,eyy,ezz,exy,eyz,exz], tensor shear) and return the macro stress --
    // no HDF5 output, no Hill calibration.  Used by the single-shot UI.
    SingleShotResult solveSingleLoadCase(short int numCubes, short int numPoints,
                                         int32_t ***voxels, const double eps[6]);

    ansysWrapper* wr;

    // Dataset-build controls (phase 1.5 / 2.0), editable from the UI.
    int    num_samples = 300;   // final load cases in phase 2.0
    int    num_calib   = 150;   // calibration load cases in phase 1.5
    double strain_val  = 1e-04; // strain amplitude for all load cases

private:
    HillCriterion m_hill;

    bool computeSMatrix(short int numCubes, short int numPoints, int32_t ***voxels,
                        double strain_val, double S_out[6][6]);

    bool calibrateHillMatrix(short int numCubes, short int numPoints, int32_t ***voxels,
                             double strain_val, const double S_matrix[6][6]);
};

#endif // STRESSANALYSIS_H

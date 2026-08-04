#ifndef STRESSANALYSIS_FFT_H
#define STRESSANALYSIS_FFT_H

#include <cstdint>
#include <vector>
#include <array>
#include "hillcriterion.h"
#include "stressresult.h"

// FFT counterpart of StressAnalysis (which drives ANSYS).  Runs the same three
// phases -- elastic compliance, Hill calibration, main sampling -- but solves
// every load case in memory with the Moulinec-Suquet FFT homogenizer instead of
// launching ANSYS.  Output HDF5 layout is kept identical so the existing
// LoadStepManager / viewers work unchanged.
//
// Material: cubic single crystal, defaults Cu/Fe-like in Pa (168.4/121.4/75.4 GPa).
class StressAnalysisFFT
{
public:
    // Same entry-point signature as StressAnalysis::estimateStressWithANSYS.
    void estimateStressWithFFT(short int numCubes, short int numPoints, int32_t ***voxels);

    // Solve a single known load case (macro strain, pipeline order
    // [exx,eyy,ezz,exy,eyz,exz], tensor shear) and return the macro stress --
    // no HDF5 output, no Hill calibration.  Used by the single-shot UI.
    SingleShotResult solveSingleLoadCase(short int numCubes, short int numPoints,
                                         int32_t ***voxels, const double eps[6]);

    // Single-crystal constants (Pa) and grid controls (optional overrides).
    double C11 = 168.4e9, C12 = 121.4e9, C44 = 75.4e9;
    double fft_tol      = 1e-5;
    int    fft_max_iter = 1000;

    // Dataset-build controls (phase 1.5 / 2.0), editable from the UI.
    int    num_samples = 300;   // final load cases in phase 2.0
    int    num_calib   = 150;   // calibration load cases in phase 1.5
    double strain_val  = 1e-04; // strain amplitude for all load cases

private:
    HillCriterion m_hill;

    // Flatten int32_t*** voxels[ix][iy][iz] -> x-fastest grain field and report
    // the highest grain id encountered.
    static std::vector<int> buildGrainField(int N, int32_t ***voxels, int& nGrainsOut);
};

#endif // STRESSANALYSIS_FFT_H

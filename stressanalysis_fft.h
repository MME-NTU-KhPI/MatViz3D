#ifndef STRESSANALYSIS_FFT_H
#define STRESSANALYSIS_FFT_H

#include <cstdint>
#include <vector>
#include <array>
#include <functional>
#include "hillcriterion.h"
#include "stressresult.h"

// FFT counterpart of StressAnalysis (which drives ANSYS).  Runs the same three
// phases -- elastic compliance, Hill calibration, main sampling -- but solves
// every load case in memory with the Moulinec-Suquet FFT homogenizer instead of
// launching ANSYS.  Output HDF5 layout is kept identical so the existing
// LoadStepManager / viewers work unchanged.
//
// Material: cubic single crystal. C11/C12/C44 default to the constants of the
// material selected from material_properties.db (Cu-like 168.4/121.4/75.4 GPa
// when nothing is selected) and can still be overridden per instance.
class StressAnalysisFFT
{
public:
    StressAnalysisFFT();

    // Same entry-point signature as StressAnalysis::estimateStressWithANSYS.
    void estimateStressWithFFT(short int numCubes, short int numPoints, int32_t ***voxels);

    // Solve a single known load case (macro strain, pipeline order
    // [exx,eyy,ezz,exy,eyz,exz], tensor shear) and return the macro stress --
    // no HDF5 output, no Hill calibration.  Used by the single-shot UI.
    // onIter, if set, is called with (iteration, equilibrium_error) after every
    // FFT iteration -- lets the UI stream a live convergence plot.
    SingleShotResult solveSingleLoadCase(short int numCubes, short int numPoints,
                                         int32_t ***voxels, const double eps[6],
                                         const std::function<void(int, double)>& onIter = nullptr);

    // Quick-test elastic stiffness: the same 6 canonical unit-strain solves as
    // Phase 1.0 of estimateStressWithFFT(), but standalone -- no Hill
    // calibration, no 300-sample main run, no HDF5 output. Used by the
    // "Stiffness matrix" UI mode so S/C/P can be inspected after a few
    // seconds instead of a full dataset build.
    // onIter, if set, is called with (loadIndex 0..5, iteration, error) for
    // every iteration of every one of the 6 solves.
    StiffnessMatrixResult computeStiffnessMatrix(short int numCubes, short int numPoints,
                                                 int32_t ***voxels,
                                                 const std::function<void(int, int, double)>& onIter = nullptr);

    // Single-crystal constants (Pa) and grid controls (optional overrides).
    // Seeded from Parameters by the constructor.
    double C11, C12, C44;
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

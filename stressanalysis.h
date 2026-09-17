#ifndef STRESSANALYSIS_H
#define STRESSANALYSIS_H

#include <cstdint>
#include "ansyswrapper.h"
#include "hillcriterion.h"
#include "stressresult.h"
#include "parameters.h"

class StressAnalysis
{
public:
    void estimateStressWithANSYS(short int numCubes, short int numPoints, int32_t ***voxels);

    // Solve a single known load case (macro strain, pipeline order
    // [exx,eyy,ezz,exy,eyz,exz], tensor shear) and return the macro stress --
    // no HDF5 output, no Hill calibration.  Used by the single-shot UI.
    SingleShotResult solveSingleLoadCase(short int numCubes, short int numPoints,
                                         int32_t ***voxels, const double eps[6]);

    // Quick-test elastic stiffness: same 6-canonical-load ANSYS runs as
    // Phase 1.0 of estimateStressWithANSYS() (via computeElasticProperties()),
    // standalone -- no Hill calibration, no 300-sample main run, no HDF5
    // output. Used by the "Stiffness matrix" UI mode. No per-iteration
    // convergence data (ANSYS is a direct FE solve, not iterative), so
    // StiffnessMatrixResult::loads[].iterations/error are left at 0.
    StiffnessMatrixResult computeStiffnessMatrix(short int numCubes, short int numPoints,
                                                 int32_t ***voxels, double strain_val = 1e-04);

    ansysWrapper* wr;

    // computeStiffnessMatrix(): also keep the per-voxel fields of the six
    // solves in StiffnessMatrixResult::fields (mirrors StressAnalysisFFT).
    // Seeded from --save_fields.
    bool   keep_fields = Parameters::instance()->getSaveFields();

    // Dataset-build controls (phase 1.5 / 2.0), editable from the UI.
    int    num_samples = 300;   // final load cases in phase 2.0
    int    num_calib   = 150;   // calibration load cases in phase 1.5
    double strain_val  = 1e-04; // strain amplitude for all load cases

private:
    HillCriterion m_hill;

    // Shared by computeSMatrix() and computeStiffnessMatrix(): builds a temp
    // ansysWrapper, applies the 6 canonical unit-strain loads, runs ANSYS,
    // and reads back S/C/P via ansysWrapper::calculateElasticProperties().
    // If `fields` is given, the per-node result tables of the six loads are
    // read back (element-averaged correction applied) into fields->fields
    // and the solver's local_cs into fields->local_cs.
    bool computeElasticProperties(short int numCubes, short int numPoints, int32_t ***voxels,
                                  double strain_val, ansysWrapper::ElasticProperties& out,
                                  StiffnessMatrixResult* fields = nullptr);

    bool computeSMatrix(short int numCubes, short int numPoints, int32_t ***voxels,
                        double strain_val, double S_out[6][6]);

    bool calibrateHillMatrix(short int numCubes, short int numPoints, int32_t ***voxels,
                             double strain_val, const double S_matrix[6][6]);
};

#endif // STRESSANALYSIS_H

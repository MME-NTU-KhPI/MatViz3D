#ifndef HILLCRITERION_H
#define HILLCRITERION_H
#include <optional>
#include <vector>
#include <array>
#include "hdf5wrapper.h"
#include "ansyswrapper.h"

class HillCriterion {
public:
    // CRSS constant for BCC material (Pa)
    static constexpr double CRSS = 60e6;

    std::vector<std::array<double,6>> computeYieldPoints(
        ansysWrapper* wr,
        const std::vector<std::vector<float>>& local_cs,
        short int N,
        int32_t ***voxels
        );

    bool fit(const std::vector<std::array<double,6>>& yield_points);

    std::vector<std::vector<double>> generateLoads(
        int num_samples,
        double strain_val,
        const double S[6][6]
        );

    void saveToHDF5(HDF5Wrapper& hdf5, const std::string& prefix);
    bool loadFromHDF5(HDF5Wrapper& hdf5, const std::string& prefix);

    bool isValid() const { return m_isValid; }

    // Resolved settings (see Parameters::resolveYieldSettings). Defaults are
    // schmid + deviatoric5d, i.e. the historical behaviour.
    void setYieldSettings(const QString& localCriterion, const QString& hillSpace, double yieldStress);
    // Active local criterion and its threshold, for logs, e.g. "von Mises, yield_stress = 1.2000e+08 Pa"
    QString criterionDescription() const {
        return m_vonMises ? QString("von Mises, yield_stress = %1 Pa").arg(m_yieldStress, 0, 'e', 4)
                          : QString("Schmid, CRSS = %1 Pa").arg(CRSS, 0, 'e', 4);
    }

    std::optional<std::array<double,6>> computeYieldPointForStep(
        const std::vector<std::array<double,6>>& voxel_stress,
        const std::vector<int>&                  voxel_grain,
        const std::array<double,6>&              macro_stress,
        const std::vector<std::vector<float>>&   local_cs);

private:
    friend class TestHillCriterion;

    double m_P_Hill[6][6]    = {0}; // 6x6 matrix for HDF5 export
    double m_P_Hill_5D[5][5] = {0}; // 5x5 working matrix in deviatoric space
    double m_L_5D[5][5]      = {0}; // Holetskiy decomposition P_5D = L * L^T
    double m_Linv_5D[5][5]   = {0}; // Inverse of L
    bool   m_isValid         = false;

    bool   m_vonMises        = false;  // local criterion: von Mises instead of Schmid
    bool   m_full6D          = false;  // Hill fit space: full 6D instead of 5D deviatoric
    double m_yieldStress     = 120e6;  // von Mises threshold (Pa)
    bool   m_fittedFull6D    = false;  // space actually used by the last successful fit

    static double vonMisesStress(const double s[6]);
    bool fitFull6D(const std::vector<std::array<double,6>>& yield_points, QString* reason);

    void eulerToBungeMatrix(double phi1, double Phi, double phi2, double R[3][3]);
    double resolvedShearStress(const double stress[3][3], const double n[3], const double b[3]);

    // 5D deviatoric space mathematics
    void sigma6D_to_v5D(const double s[6], double v[5]);
    void v5D_to_sigma6D(const double v[5], double s[6]);
    void project5Dto6D_Matrix();
    void project6Dto5D_Matrix();

    bool computeCholesky5D();
    bool solveSystem15x15(double A[15][15], double b[15], double x[15]);
    bool solveSystem21x21(double A[21][21], double b[21], double x[21]);

    // 48 slip systems for BCC
    static const int NUM_SLIP_SYSTEMS = 48;
    static const double SLIP_NORMALS[48][3];
    static const double SLIP_DIRECTIONS[48][3];
};

#endif

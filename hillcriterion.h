#ifndef HILLCRITERION_H
#define HILLCRITERION_H

#include <vector>
#include <array>
#include "hdf5wrapper.h"
#include "ansyswrapper.h"

class HillCriterion {
public:
    //CRSS for cooper BCC
    static constexpr double CRSS = 60e6; // Па

    std::vector<std::array<double,6>> computeYieldPoints(
        ansysWrapper* wr,
        const std::vector<std::vector<float>>& local_cs
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

private:
    double m_P_Hill[6][6] = {0};
    double m_L[6][6]      = {0}; // P_Hill = L * L^T
    double m_Linv[6][6]   = {0}; // L^{-1}
    bool   m_isValid       = false;

    void eulerToBungeMatrix(double phi1, double Phi, double phi2, double R[3][3]);
    double resolvedShearStress(const double stress[3][3], const double n[3], const double b[3]);
    bool computeCholesky();

    bool solveSystem21x21(double A[21][21], double b[21], double x[21]);

    static const int NUM_SLIP_SYSTEMS = 12;
    static const double SLIP_NORMALS[NUM_SLIP_SYSTEMS][3];
    static const double SLIP_DIRECTIONS[NUM_SLIP_SYSTEMS][3];
};

#endif

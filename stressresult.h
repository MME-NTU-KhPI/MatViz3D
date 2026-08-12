#ifndef STRESSRESULT_H
#define STRESSRESULT_H

#include <QString>
#include <QStringList>
#include <cmath>
#include <vector>
#include <array>
#include <random>
#include <memory>
#include <QDebug>
#include "ansyswrapper.h"   // tensor_components enum, ansysWrapper (kept alive for field visualization)

// Per-voxel field data produced by a single-shot FFT solve, dense over the
// full N^3 grid -- index (z*N+y)*N+x, matching
// StressAnalysisFFT::buildGrainField()'s layout and
// OpenGLWidgetQML::calculateScene()'s voxels[k][i][j] loop (k=x, i=y, j=z) --
// so the renderer can look a voxel's field value up with the same indexing
// it already uses for grain id.
//
// Indexed by tensor_components (ansyswrapper.h). The FFT solver has no nodal
// displacement field, so only SX..SXZ, SEQV, EpsX..EpsXZ, EpsEQV are
// populated; componentValid[component] is false for the unused UX/UY/UZ/USUM
// slots (and for ID/X/Y/Z, which aren't results).
struct FieldVisualizationData
{
    int numCubes = 0;

    std::array<bool, EpsEQV + 1>               componentValid{};
    std::array<std::vector<float>, EpsEQV + 1> perVoxel;      // [component][denseIdx], size numCubes^3 when valid
    std::array<float, EpsEQV + 1>              componentMin{};
    std::array<float, EpsEQV + 1>              componentMax{};

    double macroStrain[6] = {0};   // pipeline tensor strain (exx,eyy,ezz,exy,eyz,exz) -- drives the affine deformed view

    int denseIndex(int x, int y, int z) const { return (z * numCubes + y) * numCubes + x; }
};

// Gauss-Jordan 6x6 inverse (partial pivot), plain doubles. Returns false if
// singular. Shared by StressAnalysis::computeStiffnessMatrix() (ANSYS) and
// StressAnalysisFFT::computeStiffnessMatrix() (FFT) so both quick-test paths
// invert C -> S (or S -> C) the same way, independent of the internal Mandel
// types either solver uses.
inline bool invert6x6(const double A[6][6], double out[6][6])
{
    double M[6][12];
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) { M[i][j] = A[i][j]; M[i][j + 6] = (i == j) ? 1.0 : 0.0; }
    }
    for (int c = 0; c < 6; ++c) {
        int piv = c;
        for (int r = c + 1; r < 6; ++r) if (std::abs(M[r][c]) > std::abs(M[piv][c])) piv = r;
        if (std::abs(M[piv][c]) < 1e-300) return false;
        for (int k = 0; k < 12; ++k) std::swap(M[c][k], M[piv][k]);
        const double d = M[c][c];
        for (int k = 0; k < 12; ++k) M[c][k] /= d;
        for (int r = 0; r < 6; ++r) {
            if (r == c) continue;
            const double f = M[r][c];
            for (int k = 0; k < 12; ++k) M[r][k] -= f * M[c][k];
        }
    }
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) out[i][j] = M[i][j + 6];
    return true;
}

// Result of a "quick test" elastic-stiffness computation (S, C, P + effective
// moduli), shared by StressAnalysisFFT::computeStiffnessMatrix() and
// StressAnalysis::computeStiffnessMatrix() (ANSYS) -- the UI's "Stiffness
// matrix" mode uses whichever one ran, via StressAnalysisController.
//
// Unlike the S_matrix/C_matrix/P_matrix written into the dataset-mode HDF5
// output (write-only, buried in the file), this is meant to be read back and
// displayed immediately, and to carry enough per-load debug info (FFT only)
// to sanity-check the six canonical solves that built it.
struct StiffnessMatrixResult
{
    bool    ok = false;
    QString errorMessage;

    double S[6][6] = {{0}};   // compliance, 1/Pa
    double C[6][6] = {{0}};   // stiffness,  Pa
    double P[6][6] = {{0}};   // P[i][j] = -S[i][j]/S[j][j]
    double moduli[6] = {0};   // 1/S[i][i], Pa

    bool isFFT           = false;
    int  totalIterations = 0;   // FFT only; 0 for ANSYS

    struct LoadDebug {
        int    iterations = 0;    // FFT only
        double error       = 0.0; // FFT only, final equilibrium error
        double macroStress[6] = {0};
    };
    std::array<LoadDebug, 6> loads;   // one per canonical unit-strain direction
};

// Persist a stiffness-matrix result into a fresh auto-incremented /<n>/ group
// -- S_matrix / C_matrix / P_matrix / Effective_Moduli / seed / solver (plus
// iterations_total for FFT) -- the same schema and group convention dataset
// mode writes, so existing readers need no special case.
//
// Defined in hdf5wrapper.cpp (all HDF5 I/O lives there). Shared by the
// headless CLI path (MainWindowAlgorithmHandler::runStressCalculation) and the
// GUI/controller path (StressAnalysisController::saveStiffnessResult) so the
// two cannot drift apart. Returns the group name ("/3"), empty on refusal.
QString saveStiffnessMatrixToHDF5(const QString& filename,
                                  const StiffnessMatrixResult& r,
                                  const QString& solver,
                                  unsigned int seed);

// Result of a single load-case solve, shared by StressAnalysisFFT and
// StressAnalysis (ANSYS) so the controller can treat both solvers the same way.
struct SingleShotResult
{
    bool    ok = false;
    QString errorMessage;

    double macro_stress[6] = {0};   // pipeline order: sx,sy,sz,sxy,syz,sxz (Pa)
    double von_mises        = 0.0;
    int    iterations       = 0;    // FFT iteration count; 0 for ANSYS
    double error             = 0.0; // FFT equilibrium error; 0 for ANSYS

    // Populated only by the matching solver, for 3D field visualization
    // (OpenGLWidgetQML::showAnsysField()/showFFTField()). Null when that
    // solver wasn't used or the solve failed.
    std::shared_ptr<ansysWrapper>           ansysField;
    std::shared_ptr<FieldVisualizationData> fftField;
};

// Von Mises stress from pipeline stress [sx,sy,sz,sxy,syz,sxz].
inline double vonMisesPipeline(const double s[6])
{
    const double a = s[0] - s[1], b = s[1] - s[2], c = s[2] - s[0];
    return std::sqrt(0.5 * (a * a + b * b + c * c) + 3.0 * (s[3] * s[3] + s[4] * s[4] + s[5] * s[5]));
}

// Equivalent (von Mises) strain from engineering strain [ex,ey,ez,gxy,gyz,gxz]
// (gamma = 2*eps shear convention -- matches both ANSYS's nodal EPTO output
// and FFTSolverSession::solveLoadCaseFull()'s voxel_strain_eng output).
// Shared by ansysWrapper::load_loadstep() and StressAnalysisFFT so both
// solvers derive the same quantity the same way.
inline double eqvStrainPipeline(const double e[6])
{
    const double a = e[0] - e[1], b = e[1] - e[2], c = e[2] - e[0];
    return std::sqrt(2.0 / 9.0 * (a * a + b * b + c * c) + 1.0 / 3.0 * (e[3] * e[3] + e[4] * e[4] + e[5] * e[5]));
}

// Uniform-random SO(3) orientations as Bunge ZXZ Euler angles (radians), one
// per grain id (index 0 is the unused void slot), size = nGrains+1.
//
// Shared by StressAnalysisFFT and StressAnalysis (ANSYS) so a given seed
// produces the *exact same* microstructure realization for both solvers --
// previously each drew its own independent random orientations (different
// RNG, different convention), which made side-by-side single-shot runs
// statistically incomparable rather than a true apples-to-apples check.
// forceOrientation, if non-null, gives every grain the SAME fixed Bunge ZXZ
// orientation instead of a random one -- collapses the polycrystal into a
// single effective crystal (mechanically no grain boundaries), which is
// useful as a controlled A/B test: with orientation-ensemble noise removed,
// ANSYS and FFT should agree closely if the ANSYS-side Bunge->LOCAL angle
// conversion is convention-correct.
inline std::vector<std::array<double,3>> buildGrainOrientations(
    int nGrains, unsigned int seed,
    const std::array<double,3>* forceOrientation = nullptr,
    const std::vector<TextureLibrary::Component>& texture = {})
{
    std::vector<std::array<double,3>> orient(static_cast<size_t>(nGrains) + 1, std::array<double,3>{0.0, 0.0, 0.0});
    if (forceOrientation) {
        for (int g = 1; g <= nGrains; ++g) orient[g] = *forceOrientation;
        return orient;
    }
    // Texture components set from the Texture Editor or --texture; falls back
    // to the uniform-random SO(3) sampling this function used to do inline.
    TextureLibrary lib(seed);
    if (!texture.empty()) lib.setComponents(texture);
    else                  lib.setMode(TextureLibrary::Mode::Random);

    for (int g = 1; g <= nGrains; ++g) {
        double b[3];
        lib.sampleNextBunge(b, /*in_deg=*/false);          // radians, Bunge ZXZ
        orient[g] = { b[0], b[1], b[2] };
    }
    return orient;
}

// Convert a Bunge ZXZ orientation (phi1,Phi,phi2, radians) -- the same
// convention/matrix as matviz_homog.hpp's bunge_zxz(), g = Rz(phi2) Rx(Phi)
// Rz(phi1) -- into the (THXY,THYZ,THZX) intrinsic Z-X-Y Euler angles
// (radians) that ANSYS's LOCAL command expects, so the FE material axes get
// the identical rotation matrix `g` the FFT solver uses for that grain.
inline void bungeZXZtoAnsysZXY(double phi1, double Phi, double phi2,
                                double& thxy, double& thyz, double& thzx)
{
    const double c1 = std::cos(phi1), s1 = std::sin(phi1);
    const double c  = std::cos(Phi);
    const double s  = std::sin(Phi);
    const double c2 = std::cos(phi2), s2 = std::sin(phi2);

    // g = Rz(phi2) Rx(Phi) Rz(phi1) -- only the elements the ZXY extraction
    // below needs (mirrors mvh::bunge_zxz() in matviz_homog.hpp exactly).
    const double g01 =  s1 * c2 + c1 * s2 * c;
    const double g11 = -s1 * s2 + c1 * c2 * c;
    const double g21 = -c1 * s;
    const double g20 =  s1 * s;
    const double g22 =  c;

    thxy = std::atan2(-g01, g11);

    double m32 = g21;
    if (m32 > 1.0)  m32 = 1.0;
    if (m32 < -1.0) m32 = -1.0;
    const double cos_thyz = std::sqrt(1.0 - m32 * m32);

    if (cos_thyz < 1e-9) {
        // Gimbal lock (Phi ~ 0 or pi) -- measure-zero for a continuous random
        // Phi, so a coarse fallback is fine here.
        thyz = (m32 > 0) ? (M_PI / 2.0) : (-M_PI / 2.0);
        thxy = 0.0;
        thzx = 0.0;
    } else {
        thyz = std::atan2(m32, cos_thyz);
        thzx = std::atan2(-g20, g22);
    }
}

// Debug/diagnostic only: reads MATVIZ_FORCE_ORIENT_DEG="phi1,Phi,phi2" (Bunge
// ZXZ, degrees) from the environment. When set, both solvers give EVERY
// grain that exact same orientation instead of a random one -- a controlled
// single-effective-crystal test that removes orientation-ensemble noise, so
// ANSYS vs FFT agreement (or disagreement, e.g. a sign-flipped shear stress)
// isolates whether the ANSYS-side Bunge->LOCAL angle conversion has a real
// convention bug (see bungeZXZtoAnsysZXY above) rather than just comparing
// two different random microstructure realizations.
inline bool getForcedOrientationDebugOverride(std::array<double,3>& out)
{
    qDebug() << "MATVIZ_FORCE_ORIENT_DEG = " << qEnvironmentVariableIsSet("MATVIZ_FORCE_ORIENT_DEG");
    if (!qEnvironmentVariableIsSet("MATVIZ_FORCE_ORIENT_DEG")) return false;
    const QString raw = qEnvironmentVariable("MATVIZ_FORCE_ORIENT_DEG");
    const QStringList parts = raw.split(',');
    if (parts.size() != 3) return false;
    bool ok1 = false, ok2 = false, ok3 = false;
    const double d2r = M_PI / 180.0;
    const double phi1 = parts[0].toDouble(&ok1) * d2r;
    const double Phi  = parts[1].toDouble(&ok2) * d2r;
    const double phi2 = parts[2].toDouble(&ok3) * d2r;
    if (!ok1 || !ok2 || !ok3) return false;
    out = { phi1, Phi, phi2 };
    return true;
}

#endif // STRESSRESULT_H

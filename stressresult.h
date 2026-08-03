#ifndef STRESSRESULT_H
#define STRESSRESULT_H

#include <QString>
#include <QStringList>
#include <cmath>
#include <vector>
#include <array>
#include <random>
#include <QDebug>

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
};

// Von Mises stress from pipeline stress [sx,sy,sz,sxy,syz,sxz].
inline double vonMisesPipeline(const double s[6])
{
    const double a = s[0] - s[1], b = s[1] - s[2], c = s[2] - s[0];
    return std::sqrt(0.5 * (a * a + b * b + c * c) + 3.0 * (s[3] * s[3] + s[4] * s[4] + s[5] * s[5]));
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
inline std::vector<std::array<double,3>> buildGrainOrientations(int nGrains, unsigned int seed,
                                                                  const std::array<double,3>* forceOrientation = nullptr)
{
    std::vector<std::array<double,3>> orient(static_cast<size_t>(nGrains) + 1, std::array<double,3>{0.0, 0.0, 0.0});
    if (forceOrientation) {
        for (int g = 1; g <= nGrains; ++g) orient[g] = *forceOrientation;
        return orient;
    }
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    const double TWO_PI = 2.0 * M_PI;
    for (int g = 1; g <= nGrains; ++g) {
        const double phi1 = TWO_PI * u01(gen);
        const double Phi  = std::acos(2.0 * u01(gen) - 1.0);
        const double phi2 = TWO_PI * u01(gen);
        orient[g] = { phi1, Phi, phi2 };   // radians, Bunge ZXZ
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

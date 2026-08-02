#ifndef STRESSRESULT_H
#define STRESSRESULT_H

#include <QString>
#include <cmath>

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

#endif // STRESSRESULT_H

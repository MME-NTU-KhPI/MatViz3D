#ifndef PHASEMATERIAL_H
#define PHASEMATERIAL_H

#include <QString>
#include <array>
#include <cmath>
#include <vector>

/**
 * @brief One constituent material, as a stiffness in its OWN axes.
 *
 * Voigt order is 11, 22, 33, 23, 13, 12 -- the same pairing
 * `mvh::voigt_to_C4()` reads and the same one the material database's
 * c11..c66 columns are written in. Units are Pa (the database stores GPa;
 * `DBManager::stiffnessMatrix()` does the conversion).
 *
 * Nothing here assumes cubic symmetry. A transversely isotropic fiber uses
 * axis 3 as its distinguished (fiber) axis, which is the standard convention
 * and the one `Composite` aligns its fibers to.
 */
struct PhaseMaterial {
    QString name;
    double  C[6][6] = {{0}};

    /// True when C is (numerically) isotropic, i.e. rotating it is a no-op.
    /// Reporting only -- the solvers rotate unconditionally.
    bool isIsotropic(double tol = 1e-6) const
    {
        const double c11 = C[0][0], c12 = C[0][1], c44 = C[3][3];
        if (c11 <= 0.0) return false;
        const double expect44 = 0.5 * (c11 - c12);
        // Not called `near`: <windows.h> defines that as a macro.
        auto sameAs = [&](double a, double b) {
            return std::abs(a - b) <= tol * std::max(1.0, std::abs(c11));
        };
        return sameAs(C[1][1], c11) && sameAs(C[2][2], c11) &&
               sameAs(C[0][2], c12) && sameAs(C[1][2], c12) &&
               sameAs(C[4][4], c44) && sameAs(C[5][5], c44) &&
               sameAs(c44, expect44);
    }
};

/**
 * @brief The grain -> (material, orientation) table both stress solvers read.
 *
 * This is the multi-phase analogue of `Parameters::textureComponents`: an
 * algorithm that knows its structure has more than one constituent (Composite,
 * so far) publishes one of these into `Parameters::phaseAssignment`, and both
 * `StressAnalysis` (ANSYS) and `StressAnalysisFFT` pick it up through the same
 * accessors, so the two backends cannot drift apart on what they are solving.
 *
 * An **empty** assignment means "single material, orientations sampled from the
 * texture" -- exactly the behaviour every algorithm had before phases existed,
 * so nothing else has to change. Always go through `isUsable()` rather than
 * testing the vectors directly.
 *
 * `grainPhase` and `grainOrientation` are indexed by grain id, so index 0 is
 * the unused void slot and the vectors are sized nGrains + 1 -- the same
 * convention `buildGrainOrientations()` already uses.
 */
struct PhaseAssignment {
    std::vector<PhaseMaterial>        materials;
    std::vector<int>                  grainPhase;
    std::vector<std::array<double,3>> grainOrientation;   ///< Bunge ZXZ, radians

    void clear()
    {
        materials.clear();
        grainPhase.clear();
        grainOrientation.clear();
    }

    /**
     * @brief Whether this assignment can describe a structure with grain ids
     *        1..nGrains. A stale assignment -- one left over from a previous
     *        run with a different structure -- fails this and the caller falls
     *        back to the single-material path rather than indexing off the end.
     */
    bool isUsable(int nGrains) const
    {
        if (materials.empty() || nGrains < 1) return false;
        if (static_cast<int>(grainPhase.size()) < nGrains + 1) return false;
        if (!grainOrientation.empty() &&
            static_cast<int>(grainOrientation.size()) < nGrains + 1) return false;
        for (int g = 1; g <= nGrains; ++g) {
            const int p = grainPhase[static_cast<size_t>(g)];
            if (p < 0 || p >= static_cast<int>(materials.size())) return false;
        }
        return true;
    }

    int phaseOf(int grain) const
    {
        return (grain >= 0 && grain < static_cast<int>(grainPhase.size()))
                   ? grainPhase[static_cast<size_t>(grain)]
                   : 0;
    }
};

/// Cubic stiffness as a Voigt 6x6. The shape every material_properties.db row
/// with only c11/c12/c44 filled in really means.
inline void cubicVoigt(double c11, double c12, double c44, double C[6][6])
{
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) C[i][j] = 0.0;

    C[0][0] = C[1][1] = C[2][2] = c11;
    C[0][1] = C[1][0] = C[0][2] = C[2][0] = C[1][2] = C[2][1] = c12;
    C[3][3] = C[4][4] = C[5][5] = c44;
}

/// Isotropic stiffness from engineering constants, as a Voigt 6x6.
inline void isotropicVoigt(double E, double nu, double C[6][6])
{
    const double lambda = E * nu / ((1.0 + nu) * (1.0 - 2.0 * nu));
    const double mu     = E / (2.0 * (1.0 + nu));
    cubicVoigt(lambda + 2.0 * mu, lambda, mu, C);
}

#endif // PHASEMATERIAL_H

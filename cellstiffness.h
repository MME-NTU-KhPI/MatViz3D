#ifndef CELLSTIFFNESS_H
#define CELLSTIFFNESS_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "propertysearch.h"

/**
 * The properties of one solid / void voxel cell, solved the way the cells of
 * dataset v1 were, and their comparison with what was asked for: the
 * "obtained" side of the Neural tab.
 *
 * The network was trained on properties computed with fixed solver settings,
 * so a generated cell is solved with the same ones whatever the Stress Analysis
 * panel is set to: conjugate gradients, Willot's rotated scheme, tolerance
 * 1e-5, void stiffness 1e-4 of the solid's C11, and a second solve with 1e-6
 * when the solid holds less than 98.5 % of the strain energy in some of the six
 * unit loads (the void is then propping the cell up). The material is the law
 * of the dataset (Parameters::lawStiffnessPa). The six loads and the symmetrised
 * matrix are those of StressAnalysisFFT::computeStiffnessMatrix.
 *
 * Pure logic on top of FFTSolverSession, no GUI and no global state: the cell
 * is passed as a copy, so a solve does not care what the 3D view shows by the
 * time it ends.
 */
namespace cellstiff {

struct Settings {
    double tol = 1e-5;
    int maxIter = 10000;
    double voidEta = 1e-4;           ///< void stiffness, as a share of the solid's C11
    double voidEtaFallback = 1e-6;   ///< of the second solve (0 = never solve again)
    double minSolidShare = 0.985;    ///< second solve when the solid holds less of the energy in some load
    int threads = 0;                 ///< OpenMP threads of the calling thread (0 = leave as they are)
};

/// Below this share of the strain energy in some load there is no solid path through
/// the cell in that direction: the void holds it up and the dataset rejects the cell.
constexpr double kMinLoadPathShare = 0.9;

struct Solution {
    bool ok = false;          ///< C is a solved stiffness (of the first solve when the second was stopped)
    bool cancelled = false;   ///< stopped before the rule above was through
    std::string error;        ///< why there is no stiffness (empty when stopped)
    propsearch::Mat6 C{};     ///< Pa, order xx, yy, zz, xy, yz, xz, engineering shear: what propsearch::moduli takes
    double density = 0.0;     ///< solid share of the voxels
    int passes = 0;           ///< solves that C went through: 1, or 2 after the softer void
    double voidEta = 0.0;     ///< of the solve C comes from
    double share = 0.0;       ///< smallest share of the strain energy the solid holds over the six loads, in that solve
    double shareFirst = 0.0;  ///< the same in the first solve
    int iterations = 0;       ///< all loads of all solves
    bool converged = true;    ///< every load of the solve C comes from reached the tolerance
    double seconds = 0.0;
    std::string backend;      ///< empty: the program's own solver on the CPU; else the device of the GPU solver (cellstiffness_gpu.h)
};

/// pass 0 or 1, load 0..5, iteration from 1, the solver's error measure. Called from the solving thread.
/// The GPU solver runs the six loads together: load = -(1 + loads that have converged), iteration = its steps.
using Progress = std::function<void(int pass, int load, int iteration, double error)>;
/// The first solve, handed over when a second one follows (its values can be shown meanwhile).
using FirstPass = std::function<void(const Solution& first)>;

/// The solid / void field solve() takes (index (iz * n + iy) * n + ix, nonzero = solid) of
/// a voxel array of the program, voxels[ix][iy][iz] with 0 = void: the layout of
/// StressAnalysisFFT::buildGrainField.
std::vector<uint8_t> occupancy(int32_t*** voxels, int n);

/**
 * Solves the cell. `cancel`, when it turns true, stops the solve within one
 * solver iteration: the result is then `cancelled`, and still `ok` with the
 * first solve's stiffness when it was the second one that was stopped.
 */
Solution solve(const std::vector<uint8_t>& solid, int n, const propsearch::Law& material,
               const Settings& settings = Settings(), const Progress& progress = Progress(),
               const FirstPass& firstPass = FirstPass(), const std::atomic<bool>* cancel = nullptr);

/// One line of the comparison, in the units of the Neural form (moduli in MPa).
struct Row {
    std::string key;           ///< "Ex" ... "nuMin" (propsearch::targetKey) or "density"
    bool modulus = false;      ///< a stiffness, shown in MPa
    bool relative = true;      ///< deviation in percent of the asked value; false: obtained minus asked
    double asked = 0.0;
    double obtained = 0.0;     ///< NaN until the cell is solved (the density is known at once)
    double deviation = 0.0;    ///< NaN when it cannot be told
};

/**
 * What was asked next to what the cell has. `asked`: the targets of the form by
 * their keys (moduli in MPa), keys that are not targets are skipped;
 * `askedDensity` NaN when the density was not a target. `obtained`: the values
 * of propsearch::moduli for the solved cell (Pa), null before the solve.
 * Rows come in the order of propsearch::Target, the density last.
 *
 * Moduli, the anisotropy ratios and the density deviate in percent; Poisson's
 * ratios and A^U, which pass through 0, by their difference.
 */
std::vector<Row> compare(const std::map<std::string, double>& asked, double askedDensity,
                         const propsearch::Values* obtained, double density);

} // namespace cellstiff

#endif // CELLSTIFFNESS_H

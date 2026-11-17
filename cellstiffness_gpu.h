#ifndef CELLSTIFFNESS_GPU_H
#define CELLSTIFFNESS_GPU_H

#include <QString>

#include "cellstiffness.h"

/**
 * The solve of cellstiffness.h on the GPU, for the Neural tab's "Solve and compare".
 *
 * The solver is tools/neural/gpu_stiffness.py (PyTorch): the same equations and
 * settings as cellstiff::solve() - six unit loads, conjugate gradients on the
 * projection form, Willot's rotated scheme, tolerance 1e-5, void 1e-4 and a
 * second solve with 1e-6 by the same rule - with the six loads as one batch on
 * the GPU. The program starts it as a separate process and reads its lines, so
 * the program itself links no ML library. Stored dataset answers are reproduced
 * to 0.004 % of E (median of 31 cells; 0.003 % for cells with the 1e-6 void, where
 * the residual is checked in double precision on the CPU) in about a fifteenth
 * of the CPU time.
 *
 * It needs an interpreter with PyTorch and a GPU. When none is found, or the
 * process fails, the caller solves on the CPU as before.
 */
namespace cellstiff {

struct GpuSolver {
    QString python;   ///< the interpreter that has PyTorch with a GPU
    QString script;   ///< gpu_stiffness.py
    QString device;   ///< "mps", "cuda"
    QString reason;   ///< why there is none (empty when available)
    bool available() const { return !python.isEmpty() && !script.isEmpty(); }
};

/**
 * Looked up at the first call (the probe imports PyTorch: a few seconds), then
 * remembered for the run; safe to call from any thread. MATVIZ_GPU_SOLVER=0
 * turns the GPU solve off, MATVIZ_GPU_PYTHON names the interpreter (then only it
 * is tried), MATVIZ_GPU_SCRIPT the script.
 */
GpuSolver gpuSolver();

/**
 * cellstiff::solve() on the GPU, same arguments and the same rules for `cancel`
 * and `firstPass`. The six loads run together, so `progress` gets
 * load = -(1 + loads that have converged) and the solver steps of that solve.
 * `ok` false with an `error` (and not `cancelled`) when the process could not
 * give an answer: solve on the CPU then. Solution::backend is the device.
 */
Solution solveOnGpu(const GpuSolver& gpu, const std::vector<uint8_t>& solid, int n, const propsearch::Law& material,
                    const Settings& settings = Settings(), const Progress& progress = Progress(),
                    const FirstPass& firstPass = FirstPass(), const std::atomic<bool>* cancel = nullptr);

} // namespace cellstiff

#endif // CELLSTIFFNESS_GPU_H

#include "stressanalysis_fft.h"
#include "fft_solver_session.hpp"
#include "hdf5wrapper.h"
#include "parameters.h"
#include "loadstepmanager.h"

#include <QDebug>
#include <QString>
#include <QFile>
#include <QApplication>
#include <random>
#include <cmath>
#include <optional>

using fftsa::FFTSolverSession;
using fftsa::Vec6;
using fftsa::ResCol;
using namespace fftsa;
// ─────────────────────────────────────────────────────────────────────────────
//  helpers
// ─────────────────────────────────────────────────────────────────────────────
std::vector<int> StressAnalysisFFT::buildGrainField(int N, int32_t ***voxels, int& nGrainsOut)
{
    std::vector<int> field(static_cast<size_t>(N) * N * N, 0);
    int maxGrain = 0;
    for (int ix = 0; ix < N; ++ix)
        for (int iy = 0; iy < N; ++iy)
            for (int iz = 0; iz < N; ++iz) {
                const int g = voxels[ix][iy][iz];
                // x-fastest layout, identical to ansysWrapper's original_1d_index
                const size_t idx = (static_cast<size_t>(iz) * N + iy) * N + ix;
                field[idx] = g;
                if (g > maxGrain) maxGrain = g;
            }
    nGrainsOut = maxGrain;                 // highest grain id (ids are 1..maxGrain)
    return field;
}

std::vector<std::array<double,3>> StressAnalysisFFT::buildOrientations(int nGrains, unsigned int seed)
{
    // Uniform SO(3) directly in Bunge ZXZ:
    //   phi1 ~ U(0,2pi),  cos(Phi) ~ U(-1,1),  phi2 ~ U(0,2pi).
    std::vector<std::array<double,3>> orient(static_cast<size_t>(nGrains) + 1, {0,0,0});
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

// von Mises stress from pipeline stress [sx,sy,sz,sxy,syz,sxz] -- shared with
// StressAnalysis (ANSYS) and the controller via stressresult.h.
static inline double vonMises(const Vec6& s) { return vonMisesPipeline(s.data()); }
// equivalent (von Mises) strain from engineering strain [ex,ey,ez,gxy,gyz,gxz]
static inline double eqvStrain(const Vec6& e) {
    const double a = e[0]-e[1], b = e[1]-e[2], c = e[2]-e[0];
    return std::sqrt(2.0/9.0*(a*a + b*b + c*c) + 1.0/3.0*(e[3]*e[3] + e[4]*e[4] + e[5]*e[5]));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main: three-phase stress estimation via the FFT solver
// ─────────────────────────────────────────────────────────────────────────────
void StressAnalysisFFT::estimateStressWithFFT(short int numCubes, short int numPoints, int32_t ***voxels)
{
    qDebug() << "\n[StressAnalysisFFT] ████████████████████████████████████████████████";
    qDebug() << "[StressAnalysisFFT] START estimateStressWithFFT";
    qDebug() << "[StressAnalysisFFT]   numCubes  =" << numCubes;
    qDebug() << "[StressAnalysisFFT]   numPoints =" << numPoints;
    qDebug() << "[StressAnalysisFFT] ████████████████████████████████████████████████\n";

    const int N = numCubes;
    // strain_val / num_samples / num_calib are member fields (editable from the UI).

    // ── Build the microstructure inputs once ────────────────────────────────
    int nGrains = 0;
    std::vector<int> grain_field = buildGrainField(N, voxels, nGrains);
    if (nGrains < 1) { qCritical() << "[StressAnalysisFFT] x no grains in voxel field"; return; }

    // NOTE: orientations here are freshly generated (uniform SO(3)).  If MatViz3D
    // already stores per-grain Euler angles, feed those instead (radians, Bunge
    // ZXZ, index == grain id) so the FFT run matches your microstructure.
    std::vector<std::array<double,3>> orient = buildOrientations(nGrains, Parameters::seed);

    FFTSolverSession session(N, N, N, grain_field, orient, C11, C12, C44);
    session.set_tolerance(fft_tol);
    session.set_max_iters(fft_max_iter);
    qDebug() << "[StressAnalysisFFT] grains =" << nGrains
             << " solid fraction =" << session.solid_fraction();

    // ── PHASE 1.0 : elastic compliance S ────────────────────────────────────
    qDebug() << "\n[StressAnalysisFFT] ╔══════════════════════════════════════════════╗";
    qDebug() << "[StressAnalysisFFT] ║  PHASE 1.0 — Elastic compliance matrix S     ║";
    qDebug() << "[StressAnalysisFFT] ╚══════════════════════════════════════════════╝";

    double S_matrix[6][6] = {{0}};
    double C_matrix[6][6] = {{0}};
    int    s_iters = 0;
    if (!session.computeCompliance(S_matrix, C_matrix, &s_iters)) {
        qCritical() << "[StressAnalysisFFT] x PHASE 1.0 FAILED: stiffness singular. Exiting.";
        return;
    }
    qDebug() << "[StressAnalysisFFT] v PHASE 1.0 DONE (" << s_iters << "iters). S diagonal [1/Pa]:";
    for (int i = 0; i < 6; ++i)
        qDebug() << QString("    S[%1][%1] = %2").arg(i).arg(S_matrix[i][i], 0, 'e', 4);

    // ── PHASE 1.5 : Hill ellipsoid calibration ──────────────────────────────
    qDebug() << "[StressAnalysisFFT] ╔══════════════════════════════════════════════╗";
    qDebug() << "[StressAnalysisFFT] ║  PHASE 1.5 — Hill ellipsoid calibration      ║";
    qDebug() << "[StressAnalysisFFT] ║  150 random loads -> LS fit -> P_Hill        ║";
    qDebug() << "[StressAnalysisFFT] ╚══════════════════════════════════════════════╝";

    {
        std::vector<std::array<double,6>> yield_points;
        std::mt19937 gen(Parameters::seed + 1);
        std::normal_distribution<double> dist(0.0, 1.0);

        int solved = 0, skipped = 0;
        for (int n = 0; n < num_calib; ++n) {
            // random unit stress direction in 6D (uniform on the sphere)
            double d_sigma[6], norm_sq = 0.0;
            for (int i = 0; i < 6; ++i) { d_sigma[i] = dist(gen); norm_sq += d_sigma[i]*d_sigma[i]; }
            const double norm = std::sqrt(norm_sq);
            if (norm < 1e-12) { --n; continue; }
            for (int i = 0; i < 6; ++i) d_sigma[i] /= norm;

            // stress dir -> strain dir via S, renormalised to strain_val
            Vec6 eps{}; double eps_ns = 0.0;
            for (int i = 0; i < 6; ++i) {
                double acc = 0.0;
                for (int j = 0; j < 6; ++j) acc += S_matrix[i][j] * d_sigma[j];
                eps[i] = acc; eps_ns += acc*acc;
            }
            const double en = std::sqrt(eps_ns);
            if (en < 1e-30) { ++skipped; continue; }
            for (int i = 0; i < 6; ++i) eps[i] = eps[i] / en * strain_val;

            auto r = session.solveLoadCase(eps);
            ++solved;

            auto yp = m_hill.computeYieldPointForStep(
                r.voxel_stress, r.voxel_grain, r.macro_stress, session.local_cs());
            if (yp) yield_points.push_back(*yp);
        }

        qDebug() << "[StressAnalysisFFT]   calibration solves:" << solved
                 << " yield points:" << (int)yield_points.size();
        if ((int)yield_points.size() < 21) {
            qCritical() << "[StressAnalysisFFT] x PHASE 1.5 FAILED: too few yield points ("
                        << (int)yield_points.size() << "< 21). Exiting.";
            return;
        }
        if (!m_hill.fit(yield_points)) {
            qCritical() << "[StressAnalysisFFT] x PHASE 1.5 FAILED: Hill fit failed. Exiting.";
            return;
        }
        qDebug() << "[StressAnalysisFFT] v PHASE 1.5 DONE. Hill criterion fitted.";
    }

    // ── PHASE 2.0 : main simulation on the Hill ellipsoid ───────────────────
    qDebug() << "\n[StressAnalysisFFT] ╔══════════════════════════════════════════════╗";
    qDebug() << "[StressAnalysisFFT] ║  PHASE 2.0 — Main simulation (300 load cases)║";
    qDebug() << "[StressAnalysisFFT] ╚══════════════════════════════════════════════╝";

    std::vector<std::vector<double>> load_cases =
        m_hill.generateLoads(num_samples, strain_val, S_matrix);
    if (load_cases.empty()) {
        qCritical() << "[StressAnalysisFFT] x PHASE 2.0: generateLoads returned empty. Exiting.";
        return;
    }
    qDebug() << "[StressAnalysisFFT]   load cases:" << (int)load_cases.size();

    // Output file (mirror StressAnalysis)
    QString filename = Parameters::filename.length() ? Parameters::filename : "current_ls.hdf5";
    if (!Parameters::filename.length() && QFile::exists(filename) && QFile::remove(filename))
        qDebug() << "[StressAnalysisFFT]   removed existing" << filename;
    qDebug() << "[StressAnalysisFFT] Saving results to HDF5:" << filename;

    std::vector<std::array<double,6>> final_yield_points;

    {
        HDF5Wrapper hdf5(filename.toStdString());

        int last_set = hdf5.readInt("/", "last_set");
        if (last_set == -1) { last_set = 1; hdf5.write("/", "last_set", last_set); }
        else                { last_set += 1; hdf5.update("/", "last_set", last_set); }
        const std::string prefix = ("/" + QString::number(last_set)).toStdString();
        qDebug() << "[StressAnalysisFFT] HDF5 prefix:" << QString::fromStdString(prefix);

        // dataset-level metadata (same keys as the ANSYS path)
        hdf5.write(prefix, "voxels",    Parameters::voxels, Parameters::instance()->getSize());
        hdf5.write(prefix, "cubeSize",  Parameters::instance()->getSize());
        hdf5.write(prefix, "numPoints", Parameters::instance()->getPoints());
        hdf5.write(prefix, "local_cs",  session.local_cs());

        // per-load-step solve + write + yield extraction (streamed, low memory)
        for (size_t ls = 1; ls <= load_cases.size(); ++ls) {
            Vec6 eps{}; for (int i = 0; i < 6; ++i) eps[i] = load_cases[ls-1][i];

            std::vector<Vec6> vstrain_eng;
            auto r = session.solveLoadCaseFull(eps, vstrain_eng);

            const int nv = (int)r.voxel_idx.size();

            // build the 22-column per-voxel result table
            std::vector<std::vector<float>> results(nv, std::vector<float>(ResCol::R_NCOLS, 0.0f));
            std::vector<float> avg(ResCol::R_NCOLS, 0.0f);
            std::vector<float> mx (ResCol::R_NCOLS, -3.0e38f);
            std::vector<float> mn (ResCol::R_NCOLS,  3.0e38f);

            for (int e = 0; e < nv; ++e) {
                const int idx = r.voxel_idx[e];
                const int iz = idx / (N*N), iy = (idx / N) % N, ix = idx % N;
                const Vec6& s = r.voxel_stress[e];
                const Vec6& g = vstrain_eng[e];

                auto& row = results[e];
                row[R_ID]  = float(idx + 1);
                row[R_X]   = float(ix); row[R_Y] = float(iy); row[R_Z] = float(iz);
                // R_UX..R_UZ left 0 (FFT yields fields, not nodal displacements)
                row[R_SX]  = float(s[0]); row[R_SY]  = float(s[1]); row[R_SZ]  = float(s[2]);
                row[R_SXY] = float(s[3]); row[R_SYZ] = float(s[4]); row[R_SXZ] = float(s[5]);
                row[R_EX]  = float(g[0]); row[R_EY]  = float(g[1]); row[R_EZ]  = float(g[2]);
                row[R_EXY] = float(g[3]); row[R_EYZ] = float(g[4]); row[R_EXZ] = float(g[5]);
                row[R_SEQV]= float(vonMises(s));
                row[R_EEQV]= float(eqvStrain(g));

                for (int c = 0; c < ResCol::R_NCOLS; ++c) {
                    avg[c] += row[c];
                    mx[c]  = std::max(mx[c], row[c]);
                    mn[c]  = std::min(mn[c], row[c]);
                }
            }
            if (nv > 0) for (int c = 0; c < ResCol::R_NCOLS; ++c) avg[c] /= float(nv);

            std::vector<float> eps_load(6);
            for (int i = 0; i < 6; ++i) eps_load[i] = float(eps[i]);

            const std::string ls_str = prefix + "/ls_" + std::to_string(ls);
            hdf5.write(ls_str, "results",        results);
            hdf5.write(ls_str, "results_avg",    avg);
            hdf5.write(ls_str, "results_max",    mx);
            hdf5.write(ls_str, "results_min",    mn);
            hdf5.write(ls_str, "eps_as_loading", eps_load);

            // collect the yield point for the final P_Hill refit
            auto yp = m_hill.computeYieldPointForStep(
                r.voxel_stress, r.voxel_grain, r.macro_stress, session.local_cs());
            if (yp) final_yield_points.push_back(*yp);

            if (ls <= 3 || ls == load_cases.size() || ls % 50 == 0)
                qDebug() << QString("  ls_%1: iters=%2 err=%3 |sigma|_vm(avg)=%4 Pa")
                                .arg(ls).arg(r.iterations).arg(r.error, 0, 'e', 2)
                                .arg(vonMises(r.macro_stress), 0, 'e', 3);
        }

        // effective S / C / P + moduli
        std::vector<std::vector<float>> mat_S(6, std::vector<float>(6));
        std::vector<std::vector<float>> mat_C(6, std::vector<float>(6));
        std::vector<std::vector<float>> mat_P(6, std::vector<float>(6));
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j) {
                mat_S[i][j] = float(S_matrix[i][j]);
                mat_C[i][j] = float(C_matrix[i][j]);
                // Poisson-ratio-style matrix from compliance: nu_ij = -S_ij / S_jj
                mat_P[i][j] = (std::abs(S_matrix[j][j]) > 1e-30)
                                  ? float(-S_matrix[i][j] / S_matrix[j][j]) : 0.0f;
            }
        hdf5.write(prefix, "S_matrix", mat_S);
        hdf5.write(prefix, "C_matrix", mat_C);
        hdf5.write(prefix, "P_matrix", mat_P);

        auto safe_inv = [](double v){ return (std::abs(v) > 1e-20) ? float(1.0/v) : 0.0f; };
        std::vector<float> moduli = {
            safe_inv(S_matrix[0][0]), safe_inv(S_matrix[1][1]), safe_inv(S_matrix[2][2]),
            safe_inv(S_matrix[3][3]), safe_inv(S_matrix[4][4]), safe_inv(S_matrix[5][5])
        };
        hdf5.write(prefix, "Effective_Moduli", moduli);
        qDebug() << QString("[StressAnalysisFFT] Effective moduli (1/Sii) [Pa]: "
                            "Ex=%1 Ey=%2 Ez=%3  (shear entries are 2G under tensor-strain S)")
                        .arg(moduli[0],0,'e',3).arg(moduli[1],0,'e',3).arg(moduli[2],0,'e',3);

        // final P_Hill from the main-run yield points
        qDebug() << "[StressAnalysisFFT] final yield points:" << (int)final_yield_points.size();
        if (!final_yield_points.empty() && m_hill.fit(final_yield_points)) {
            m_hill.saveToHDF5(hdf5, prefix);
            qDebug() << "[StressAnalysisFFT] v P_Hill fitted and saved.";
        } else {
            qWarning() << "[StressAnalysisFFT] x final P_Hill fit failed.";
        }
    }

    qDebug() << "[StressAnalysisFFT] Loading results into LoadStepManager...";
    LoadStepManager::getInstance().LoadFromHDF5(filename);

    qDebug() << "[StressAnalysisFFT] ████████████████████████████████████████████████";
    qDebug() << "[StressAnalysisFFT] estimateStressWithFFT FINISHED";
    qDebug() << "[StressAnalysisFFT] ████████████████████████████████████████████████\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Single known load case: no Hill calibration, no HDF5 output. Used by the
//  single-shot / verification UI.
// ─────────────────────────────────────────────────────────────────────────────
SingleShotResult StressAnalysisFFT::solveSingleLoadCase(short int numCubes, short int numPoints,
                                                         int32_t ***voxels, const double eps[6])
{
    Q_UNUSED(numPoints);
    SingleShotResult out;

    qDebug() << "[StressAnalysisFFT] solveSingleLoadCase: numCubes =" << numCubes
             << " eps =" << eps[0] << eps[1] << eps[2] << eps[3] << eps[4] << eps[5];

    const int N = numCubes;
    int nGrains = 0;
    std::vector<int> grain_field = buildGrainField(N, voxels, nGrains);
    if (nGrains < 1) {
        out.errorMessage = QObject::tr("No grains in voxel field");
        qCritical() << "[StressAnalysisFFT] x solveSingleLoadCase:" << out.errorMessage;
        return out;
    }

    std::vector<std::array<double,3>> orient = buildOrientations(nGrains, Parameters::seed);

    FFTSolverSession session(N, N, N, grain_field, orient, C11, C12, C44);
    session.set_tolerance(fft_tol);
    session.set_max_iters(fft_max_iter);

    Vec6 e{}; for (int i = 0; i < 6; ++i) e[i] = eps[i];
    auto r = session.solveLoadCase(e);

    for (int i = 0; i < 6; ++i) out.macro_stress[i] = r.macro_stress[i];
    out.von_mises  = vonMisesPipeline(out.macro_stress);
    out.iterations = r.iterations;
    out.error      = r.error;
    out.ok         = true;

    qDebug() << "[StressAnalysisFFT] v solveSingleLoadCase done: iters =" << r.iterations
             << " err =" << r.error << " von_mises =" << out.von_mises;
    return out;
}

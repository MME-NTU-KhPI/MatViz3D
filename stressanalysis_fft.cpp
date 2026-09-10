#include "stressanalysis_fft.h"
#include "fft_solver_session.hpp"
#include "hdf5wrapper.h"
#include "parameters.h"

#include <QDebug>
#include <QString>
#include <QFile>
#include <QApplication>
#include <random>
#include <cmath>
#include <optional>
#include <memory>
#include <limits>
#include <algorithm>
#ifdef _OPENMP
#include <omp.h>
#endif

using fftsa::FFTSolverSession;
using fftsa::Vec6;
using fftsa::ResCol;
using namespace fftsa;

// Resolve what every grain is made of and how it is oriented, then build the
// session around it. The multi-phase and single-material paths differ only in
// which FFTSolverSession constructor runs, so keeping the choice in one place
// means the three call sites below cannot disagree about it.
//
// `gm` is an out-parameter because callers still want the orientations (for
// debug logging) after the session owns its copy.
static FFTSolverSession makeSession(int N, const std::vector<int>& grain_field,
                                    int nGrains,
                                    const std::array<double,3>* forced,
                                    double C11, double C12, double C44,
                                    GrainMaterials& gm)
{
    gm = resolveGrainMaterials(nGrains, Parameters::seed, forced,
                               Parameters::textureComponents,
                               Parameters::phaseAssignment,
                               C11, C12, C44);

    if (!gm.multiPhase)
        return FFTSolverSession(N, N, N, grain_field, gm.orientation, C11, C12, C44);

    // Count the grains per phase so the log says what was actually solved
    // rather than just what was configured.
    std::vector<int> perPhase(gm.phaseNames.size(), 0);
    for (int g = 1; g <= nGrains; ++g)
        ++perPhase[static_cast<size_t>(gm.phaseOfGrain[static_cast<size_t>(g)])];

    QStringList parts;
    for (size_t p = 0; p < gm.phaseNames.size(); ++p)
        parts << QString("%1 x %2").arg(perPhase[p]).arg(gm.phaseNames[p]);
    qDebug().noquote() << "[StressAnalysisFFT] multi-phase:" << parts.join(", ");

    return FFTSolverSession(N, N, N, grain_field, gm.orientation, gm.voigtPa, gm.maxC11);
}
// The constants come from the material picked in the UI/CLI rather than from a
// literal here, so the ANSYS and FFT backends are guaranteed to be solving the
// same material. Callers that want something else (solver_compare) can still
// assign C11/C12/C44 after construction.
StressAnalysisFFT::StressAnalysisFFT()
{
    Parameters::cubicConstantsPa(C11, C12, C44);
}

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

// von Mises stress / equivalent strain -- shared with StressAnalysis (ANSYS)
// and the controller via stressresult.h.
static inline double vonMises(const Vec6& s) { return vonMisesPipeline(s.data()); }
static inline double eqvStrain(const Vec6& e) { return eqvStrainPipeline(e.data()); }

// --np reached only ANSYS (ansysWrapper::setNP); the OpenMP loops in
// fft_homog.hpp used the runtime default and ignored it. Parameters::num_threads
// is always set -- to the physical core count when --np is absent -- so
// applying it here honours the option without quietly serialising the solver.
// Called from every FFT entry point because each may run on its own worker
// thread, and omp_set_num_threads() is per-thread state.
static void applyThreadLimit(const char* where)
{
#ifdef _OPENMP
    const int np = Parameters::num_threads;
    if (np > 0) {
        omp_set_num_threads(np);
        qDebug() << "[StressAnalysisFFT::" << where << "] OpenMP threads =" << np;
    }
#else
    Q_UNUSED(where);
#endif
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

    applyThreadLimit("estimateStressWithFFT");

    const int N = numCubes;
    // strain_val / num_samples / num_calib are member fields (editable from the UI).

    // ── Build the microstructure inputs once ────────────────────────────────
    int nGrains = 0;
    std::vector<int> grain_field = buildGrainField(N, voxels, nGrains);
    if (nGrains < 1) { qCritical() << "[StressAnalysisFFT] x no grains in voxel field"; return; }

    // resolveGrainMaterials() is shared with StressAnalysis (ANSYS) -- for a
    // given Parameters::seed both solvers see the exact same per-grain Bunge
    // ZXZ orientations and the same per-grain constituent, not two independent
    // draws.
    GrainMaterials gm;
    FFTSolverSession session = makeSession(N, grain_field, nGrains, nullptr,
                                           C11, C12, C44, gm);
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
        hdf5.write(prefix, "voxels",    voxels ? voxels : Parameters::voxels, N);
        hdf5.write(prefix, "cubeSize",  N);
        hdf5.write(prefix, "numPoints", numPoints);
        hdf5.write(prefix, "local_cs",  session.local_cs());
        hdf5.write(prefix, "seed",      int(Parameters::seed));
        hdf5.write(prefix, "solver",    QStringLiteral("fft"));
        hdf5.write(prefix, "num_samples", num_samples);
        hdf5.write(prefix, "num_calib",   num_calib);
        hdf5.write(prefix, "strain_val",  float(strain_val));
        saveGeometryMetadataToHDF5(hdf5, prefix, QStringLiteral("fft"));

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

    // NOTE: LoadStepManager is a plain, unsynchronized singleton -- reloading
    // it here would race if this function ever runs off the main thread (it
    // does, via StressAnalysisController::runDataset()'s background solve).
    // The caller reloads it on the main thread once this function returns.

    qDebug() << "[StressAnalysisFFT] ████████████████████████████████████████████████";
    qDebug() << "[StressAnalysisFFT] estimateStressWithFFT FINISHED";
    qDebug() << "[StressAnalysisFFT] ████████████████████████████████████████████████\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Single known load case: no Hill calibration, no HDF5 output. Used by the
//  single-shot / verification UI.
// ─────────────────────────────────────────────────────────────────────────────
SingleShotResult StressAnalysisFFT::solveSingleLoadCase(short int numCubes, short int numPoints,
                                                         int32_t ***voxels, const double eps[6],
                                                         const std::function<void(int, double)>& onIter)
{
    Q_UNUSED(numPoints);
    SingleShotResult out;

    qDebug() << "[StressAnalysisFFT] solveSingleLoadCase: numCubes =" << numCubes
             << " eps =" << eps[0] << eps[1] << eps[2] << eps[3] << eps[4] << eps[5];

    applyThreadLimit("solveSingleLoadCase");

    const int N = numCubes;
    int nGrains = 0;
    std::vector<int> grain_field = buildGrainField(N, voxels, nGrains);
    if (nGrains < 1) {
        out.errorMessage = QObject::tr("No grains in voxel field");
        qCritical() << "[StressAnalysisFFT] x solveSingleLoadCase:" << out.errorMessage;
        return out;
    }

    std::array<double,3> forcedOrient;
    const bool useForced = getForcedOrientationDebugOverride(forcedOrient);

    GrainMaterials gm;
    FFTSolverSession session = makeSession(N, grain_field, nGrains,
                                           useForced ? &forcedOrient : nullptr,
                                           C11, C12, C44, gm);
    const std::vector<std::array<double,3>>& orient = gm.orientation;
    if (useForced) {
        const double r2d = 180.0 / M_PI;
        qDebug() << "[StressAnalysisFFT]   [DEBUG] MATVIZ_FORCE_ORIENT_DEG active: every grain forced to"
                 << forcedOrient[0]*r2d << forcedOrient[1]*r2d << forcedOrient[2]*r2d << "(Bunge ZXZ, deg)";
    }

    qDebug() << "[StressAnalysisFFT]   [DEBUG] seed =" << Parameters::seed
             << " C11 =" << C11 << " C12 =" << C12 << " C44 =" << C44;
    qDebug() << "[StressAnalysisFFT]   [DEBUG] numCubes =" << numCubes << " nGrains =" << nGrains;
    if (nGrains >= 1) {
        const double r2d = 180.0 / M_PI;
        const auto& g1 = orient[1]; // grain id 1, radians, Bunge ZXZ (phi1,Phi,phi2)
        qDebug() << "[StressAnalysisFFT]   [DEBUG] grain#1 orientation (Bunge ZXZ, deg) ="
                 << g1[0]*r2d << g1[1]*r2d << g1[2]*r2d;
    }

    session.set_tolerance(fft_tol);
    session.set_max_iters(fft_max_iter);
    qDebug() << "[StressAnalysisFFT]   [DEBUG] solid_fraction =" << session.solid_fraction()
             << " tol =" << fft_tol << " max_iter =" << fft_max_iter;

    Vec6 e{}; for (int i = 0; i < 6; ++i) e[i] = eps[i];
    std::vector<Vec6> voxel_strain_eng;
    auto r = session.solveLoadCaseFull(e, voxel_strain_eng, onIter);

    for (int i = 0; i < 6; ++i) out.macro_stress[i] = r.macro_stress[i];
    out.von_mises  = vonMisesPipeline(out.macro_stress);
    out.iterations = r.iterations;
    out.error      = r.error;
    out.ok         = true;

    // ── Per-voxel field, for 3D visualization ───────────────────────────────
    // Dense N^3 arrays indexed the same way buildGrainField() laid out
    // grain_field / voxel_idx: (iz*N+iy)*N+ix -- matches
    // FieldVisualizationData::denseIndex() and OpenGLWidgetQML's
    // voxels[k][i][j] loop (k=x, i=y, j=z).
    {
        static const int stressComp[6] = {SX, SY, SZ, SXY, SYZ, SXZ};
        static const int strainComp[6] = {EpsX, EpsY, EpsZ, EpsXY, EpsYZ, EpsXZ};

        auto field = std::make_shared<FieldVisualizationData>();
        field->numCubes = N;
        const size_t denseN = static_cast<size_t>(N) * N * N;

        auto initComp = [&](int comp) {
            field->componentValid[comp] = true;
            field->perVoxel[comp].assign(denseN, 0.0f);
            field->componentMin[comp] = std::numeric_limits<float>::max();
            field->componentMax[comp] = -std::numeric_limits<float>::max();
        };
        for (int c : stressComp) initComp(c);
        for (int c : strainComp) initComp(c);
        initComp(SEQV);
        initComp(EpsEQV);

        auto setVal = [&](int comp, int denseIdx, float val) {
            field->perVoxel[comp][denseIdx] = val;
            field->componentMin[comp] = std::min(field->componentMin[comp], val);
            field->componentMax[comp] = std::max(field->componentMax[comp], val);
        };

        for (size_t vi = 0; vi < r.voxel_idx.size(); ++vi) {
            const int denseIdx = r.voxel_idx[vi];
            const Vec6& s   = r.voxel_stress[vi];
            const Vec6& eng = voxel_strain_eng[vi];
            for (int c = 0; c < 6; ++c) setVal(stressComp[c], denseIdx, float(s[c]));
            for (int c = 0; c < 6; ++c) setVal(strainComp[c], denseIdx, float(eng[c]));
            setVal(SEQV,   denseIdx, float(vonMisesPipeline(s.data())));
            setVal(EpsEQV, denseIdx, float(eqvStrainPipeline(eng.data())));
        }
        for (int i = 0; i < 6; ++i) field->macroStrain[i] = r.macro_strain[i];

        out.fftField = field;
    }

    qDebug() << "[StressAnalysisFFT]   [DEBUG] macro_stress (Pa) sx,sy,sz,sxy,syz,sxz ="
             << out.macro_stress[0] << out.macro_stress[1] << out.macro_stress[2]
             << out.macro_stress[3] << out.macro_stress[4] << out.macro_stress[5];

    // Same 3x3 layout as ansysWrapper::loadElementAveragedResults()'s
    // NODE/ELEM comparison print, so all three (NODE, ELEM, FFT) can be
    // compared directly.
    qDebug() << "  S tensor   [FFT]  (Pa): " << r.macro_stress[0] << r.macro_stress[3] << r.macro_stress[5];
    qDebug() << "                          " << r.macro_stress[3] << r.macro_stress[1] << r.macro_stress[4];
    qDebug() << "                          " << r.macro_stress[5] << r.macro_stress[4] << r.macro_stress[2];
    qDebug() << "  EPS tensor [FFT]:       " << r.macro_strain[0] << r.macro_strain[3] << r.macro_strain[5];
    qDebug() << "                          " << r.macro_strain[3] << r.macro_strain[1] << r.macro_strain[4];
    qDebug() << "                          " << r.macro_strain[5] << r.macro_strain[4] << r.macro_strain[2];

    qDebug() << "[StressAnalysisFFT] v solveSingleLoadCase done: iters =" << r.iterations
             << " err =" << r.error << " von_mises =" << out.von_mises;
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Quick-test stiffness: 6 canonical unit-strain solves -> C, S = C^-1, P.
//  Same math as Phase 1.0 of estimateStressWithFFT() / FFTSolverSession::
//  computeCompliance(), but done directly here (rather than via
//  computeCompliance()) so each of the 6 solves can be tagged with its load
//  index for the caller's onIter callback, and so every step can be logged.
// ─────────────────────────────────────────────────────────────────────────────
StiffnessMatrixResult StressAnalysisFFT::computeStiffnessMatrix(short int numCubes, short int numPoints,
                                                                 int32_t ***voxels,
                                                                 const std::function<void(int, int, double)>& onIter)
{
    Q_UNUSED(numPoints);
    StiffnessMatrixResult r;
    r.isFFT = true;

    qDebug() << "\n[StressAnalysisFFT::computeStiffnessMatrix] ────────────────────────────────";
    qDebug() << "[StressAnalysisFFT::computeStiffnessMatrix] 6 canonical unit-strain solves -> C, S, P";

    applyThreadLimit("computeStiffnessMatrix");

    const int N = numCubes;
    int nGrains = 0;
    std::vector<int> grain_field = buildGrainField(N, voxels, nGrains);
    if (nGrains < 1) {
        r.errorMessage = QObject::tr("No grains in voxel field");
        qCritical() << "[StressAnalysisFFT::computeStiffnessMatrix] x" << r.errorMessage;
        return r;
    }

    GrainMaterials gm;
    FFTSolverSession session = makeSession(N, grain_field, nGrains, nullptr,
                                           C11, C12, C44, gm);
    session.set_tolerance(fft_tol);
    session.set_max_iters(fft_max_iter);
    qDebug() << "[StressAnalysisFFT::computeStiffnessMatrix]   grains =" << nGrains
             << " solid fraction =" << session.solid_fraction()
             << " tol =" << fft_tol << " max_iter =" << fft_max_iter
             << " textureComponents =" << (int)Parameters::textureComponents.size();

    static const char* comp_names[6] = {"exx", "eyy", "ezz", "exy(tensor)", "eyz(tensor)", "exz(tensor)"};
    double C[6][6] = {{0}};
    int totalIters = 0;

    for (int j = 0; j < 6; ++j) {
        Vec6 e{}; e.fill(0.0); e[j] = 1.0;
        qDebug() << QString("[StressAnalysisFFT::computeStiffnessMatrix]   Load #%1: %2 = 1.0 (unit tensor strain)")
                        .arg(j + 1).arg(comp_names[j]);

        auto cb = onIter;
        std::function<void(int, double)> perLoadCb = cb
            ? std::function<void(int, double)>([cb, j](int it, double err) { cb(j, it, err); })
            : std::function<void(int, double)>();

        auto step = session.solveLoadCase(e, perLoadCb);
        totalIters += step.iterations;
        for (int i = 0; i < 6; ++i) C[i][j] = step.macro_stress[i];

        r.loads[j].iterations = step.iterations;
        r.loads[j].error      = step.error;
        for (int i = 0; i < 6; ++i) r.loads[j].macroStress[i] = step.macro_stress[i];

        qDebug() << QString("      -> iterations=%1  error=%2  stress=[%3, %4, %5, %6, %7, %8] Pa")
                        .arg(step.iterations).arg(step.error, 0, 'e', 3)
                        .arg(step.macro_stress[0], 0, 'e', 3).arg(step.macro_stress[1], 0, 'e', 3)
                        .arg(step.macro_stress[2], 0, 'e', 3).arg(step.macro_stress[3], 0, 'e', 3)
                        .arg(step.macro_stress[4], 0, 'e', 3).arg(step.macro_stress[5], 0, 'e', 3);
        if (step.error >= fft_tol)
            qWarning() << "      ! load" << (j + 1) << "did NOT converge (err" << step.error
                       << ">= tol" << fft_tol << ") -- C/S may be inaccurate for this column";
    }

    // Symmetrize. NOT a plain average: C here is pipeline-basis, built from
    // unit TENSOR strains, so its normal<->shear coupling block satisfies
    // C[i][j] == 2*C[j][i] by construction and a plain average would destroy
    // both entries. See symmetrizePipelineC() in stressresult.h.
    symmetrizePipelineC(C);

    if (!invert6x6(C, r.S)) {
        r.errorMessage = QObject::tr("Stiffness matrix C is singular; cannot invert to S");
        qCritical() << "[StressAnalysisFFT::computeStiffnessMatrix] x" << r.errorMessage;
        return r;
    }
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) r.C[i][j] = C[i][j];

    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            r.P[i][j] = (std::abs(r.S[j][j]) > 1e-30) ? -r.S[i][j] / r.S[j][j] : 0.0;
    for (int i = 0; i < 6; ++i)
        r.moduli[i] = (std::abs(r.S[i][i]) > 1e-20) ? 1.0 / r.S[i][i] : 0.0;

    r.totalIterations = totalIters;
    r.ok = true;

    qDebug() << "[StressAnalysisFFT::computeStiffnessMatrix] v DONE (" << totalIters << "total iters).";
    qDebug() << "  C matrix (6x6) [Pa]:";
    for (int i = 0; i < 6; ++i) {
        QString row;
        for (int j = 0; j < 6; ++j) row += QString("%1 ").arg(r.C[i][j], 12, 'e', 3);
        qDebug().noquote() << QString("    Row[%1]: [ ").arg(i) + row + "]";
    }
    qDebug() << "  S matrix (6x6) [1/Pa]:";
    for (int i = 0; i < 6; ++i) {
        QString row;
        for (int j = 0; j < 6; ++j) row += QString("%1 ").arg(r.S[i][j], 12, 'e', 3);
        qDebug().noquote() << QString("    Row[%1]: [ ").arg(i) + row + "]";
    }
    qDebug() << QString("  Effective moduli (1/Sii) [Pa]: Ex=%1 Ey=%2 Ez=%3 Gxy=%4 Gyz=%5 Gxz=%6")
                    .arg(r.moduli[0], 0, 'e', 3).arg(r.moduli[1], 0, 'e', 3).arg(r.moduli[2], 0, 'e', 3)
                    .arg(r.moduli[3], 0, 'e', 3).arg(r.moduli[4], 0, 'e', 3).arg(r.moduli[5], 0, 'e', 3);
    qDebug() << "[StressAnalysisFFT::computeStiffnessMatrix] ────────────────────────────────\n";

    return r;
}

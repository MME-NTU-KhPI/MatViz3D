#include "stressanalysis.h"
#include "ansyswrapper.h"
#include "hdf5wrapper.h"
#include "parameters.h"
#include "loadstepmanager.h"
#include <random>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Main method: three-phase stress estimation via ANSYS
//
//  PHASE 1.0: 6 canonical loads  -> elastic compliance matrix S
//  PHASE 1.5: 150 random loads   -> fit Hill ellipsoid (P_Hill)
//  PHASE 2.0: 300 Hill loads     -> main stress field computation
// ─────────────────────────────────────────────────────────────────────────────
void StressAnalysis::estimateStressWithANSYS(short int numCubes, short int numPoints, int32_t ***voxels)
{
    qDebug() << "\n[StressAnalysis] ████████████████████████████████████████████████";
    qDebug() << "[StressAnalysis] START estimateStressWithANSYS";
    qDebug() << "[StressAnalysis]   numCubes  =" << numCubes;
    qDebug() << "[StressAnalysis]   numPoints =" << numPoints;
    qDebug() << "[StressAnalysis] ████████████████████████████████████████████████\n";

    const auto N = numCubes;
    // strain_val / num_samples are member fields (editable from the UI).

    qDebug() << "[StressAnalysis] Run parameters:";
    qDebug() << "  strain_val  =" << strain_val  << "(strain amplitude for all load cases)";
    qDebug() << "  num_samples =" << num_samples << "(final load cases in phase 2.0)";

    // ─── PHASE 1.0 ──────────────────────────────────────────────────────────
    double S_matrix[6][6] = {0};
    qDebug() << "\n[StressAnalysis] ╔══════════════════════════════════════════════╗";
    qDebug() << "[StressAnalysis] ║  PHASE 1.0 — Elastic compliance matrix S     ║";
    qDebug() << "[StressAnalysis] ║  6 canonical loads: ex, ey, ez, gxy, gyz, gxz║";
    qDebug() << "[StressAnalysis] ╚══════════════════════════════════════════════╝";

    if (!computeSMatrix(numCubes, numPoints, voxels, strain_val, S_matrix)) {
        qCritical() << "[StressAnalysis] x PHASE 1.0 FAILED: computeSMatrix returned false. Exiting.";
        return;
    }

    qDebug() << "[StressAnalysis] v PHASE 1.0 DONE. S matrix (6x6) [1/Pa]:";
    for (int i = 0; i < 6; ++i) {
        QString row;
        for (int j = 0; j < 6; ++j) {
            row += QString("%1").arg(S_matrix[i][j], 12, 'e', 3) + " ";
        }
        qDebug().noquote() << QString("  Row[%1]: [").arg(i) + row + "]";
    }

    // ─── PHASE 1.5 ──────────────────────────────────────────────────────────
    qDebug() << "\n[StressAnalysis] ╔══════════════════════════════════════════════╗";
    qDebug() << "[StressAnalysis] ║  PHASE 1.5 — Hill ellipsoid calibration      ║";
    qDebug() << "[StressAnalysis] ║  150 random loads -> LS fit -> P_Hill         ║";
    qDebug() << "[StressAnalysis] ╚══════════════════════════════════════════════╝";

    if (!calibrateHillMatrix(numCubes, numPoints, voxels, strain_val, S_matrix)) {
        qCritical() << "[StressAnalysis] x PHASE 1.5 FAILED: calibrateHillMatrix returned false. Exiting.";
        return;
    }
    qDebug() << "[StressAnalysis] v PHASE 1.5 DONE. Hill criterion fitted.";

    // ─── PHASE 2.0 ──────────────────────────────────────────────────────────
    qDebug() << "\n[StressAnalysis] ╔══════════════════════════════════════════════╗";
    qDebug() << "[StressAnalysis] ║  PHASE 2.0 — Main simulation (300 load cases)║";
    qDebug() << "[StressAnalysis] ║  Loads sampled on the Hill ellipsoid surface  ║";
    qDebug() << "[StressAnalysis] ╚══════════════════════════════════════════════╝";

    qDebug() << "[StressAnalysis] Generating" << num_samples << "load cases via HillCriterion::generateLoads...";
    std::vector<std::vector<double>> load_cases = m_hill.generateLoads(num_samples, strain_val, S_matrix);

    if (load_cases.empty()) {
        qCritical() << "[StressAnalysis] x PHASE 2.0: generateLoads returned empty list. Exiting.";
        return;
    }
    qDebug() << "[StressAnalysis]   Load cases received:" << (int)load_cases.size();

    // ANSYS setup
    qDebug() << "[StressAnalysis] Initialising ansysWrapper for phase 2.0...";
    wr = new ansysWrapper(true);
    if (!Parameters::working_directory.isEmpty())
        wr->setWorkingDirectory(Parameters::working_directory);
    wr->setSeed(Parameters::seed);
    wr->setNP(Parameters::num_threads);
    qDebug() << "  working_directory :" << Parameters::working_directory;
    qDebug() << "  seed              :" << Parameters::seed;
    qDebug() << "  num_threads       :" << Parameters::num_threads;

    // BCC material constants (Fe-like)
    double c11 = 168.40e9, c12 = 121.40e9, c44 = 75.40e9;
    qDebug() << "[StressAnalysis] Material (BCC anisotropic):";
    qDebug() << "  C11 =" << c11 << "Pa  C12 =" << c12 << "Pa  C44 =" << c44 << "Pa";
    wr->setAnisoMaterial(c11, c12, c12, c11, c12, c11, c44, c44, c44);
    wr->setElemByNum(185);

    qDebug() << "[StressAnalysis] Building FE mesh (8-node hexahedra, SOLID185)...";
    wr->createFEfromArray8Node(voxels, N, numPoints, true);

    qDebug() << "[StressAnalysis] Applying" << (int)load_cases.size() << "load cases...";
    for (const auto& load : load_cases) {
        // applyComplexLoads() takes (eps_xy, eps_xz, eps_yz) in that param order,
        // but load[] is [ex,ey,ez,gxy,gyz,gxz] -- swap the last two to match.
        wr->applyComplexLoads(0, 0, 0, N, N, N, load[0], load[1], load[2], load[3], load[5], load[4]);
    }

    qDebug() << "[StressAnalysis] Writing load steps (1 .." << (int)load_cases.size() << ")...";
    wr->solveLS(1, (int)load_cases.size());
    wr->saveAll();

    qDebug() << "[StressAnalysis] Launching ANSYS (phase 2.0)...";
    if (!wr->run()) {
        qCritical() << "[StressAnalysis] x ANSYS run failed. Exiting.";
        delete wr;
        return;
    }
    qDebug() << "[StressAnalysis] v ANSYS finished successfully.";

    // ─── Write results to HDF5 ──────────────────────────────────────────────
    QString filename = Parameters::filename.length() ? Parameters::filename : "current_ls.hdf5";
    qDebug() << "\n[StressAnalysis] Saving results to HDF5:" << filename;

    if (!Parameters::filename.length()) {
        qDebug() << "  (no -o flag supplied, using" << filename << ")";
        if (QFile::exists(filename) && QFile::remove(filename))
            qDebug() << "  Existing file removed.";
    }

    {
        HDF5Wrapper hdf5(filename.toStdString());

        int last_set = hdf5.readInt("/", "last_set");
        if (last_set == -1) {
            last_set = 1;
            hdf5.write("/", "last_set", last_set);
            qDebug() << "[StressAnalysis] HDF5: created last_set = 1";
        } else {
            last_set += 1;
            hdf5.update("/", "last_set", last_set);
            qDebug() << "[StressAnalysis] HDF5: last_set incremented to" << last_set;
        }

        std::string prefix = ("/" + QString::number(last_set)).toStdString();
        qDebug() << "[StressAnalysis] HDF5 prefix for this dataset:" << QString::fromStdString(prefix);

        qDebug() << "[StressAnalysis] Writing voxels, cubeSize, numPoints, local_cs...";
        hdf5.write(prefix, "voxels",    Parameters::voxels, Parameters::instance()->getSize());
        hdf5.write(prefix, "cubeSize",  Parameters::instance()->getSize());
        hdf5.write(prefix, "numPoints", Parameters::instance()->getPoints());
        hdf5.write(prefix, "local_cs",  wr->local_cs);

        qDebug() << "[StressAnalysis] Writing per-step results ("
                 << (int)wr->eps_as_loading.size() << "steps)...";
        for (size_t ls_num = 1; ls_num <= wr->eps_as_loading.size(); ls_num++) {
            std::string ls_str = "/ls_" + std::to_string(ls_num);

            if (ls_num <= 3 || ls_num == wr->eps_as_loading.size())
                qDebug() << QString("  Step ls_%1: writing results, avg, max, min, eps_as_loading").arg(ls_num);
            else if (ls_num == 4)
                qDebug() << "  ... (intermediate steps) ...";

            wr->load_loadstep(ls_num);
            hdf5.write(prefix + ls_str, "results_avg",    wr->loadstep_results_avg);
            hdf5.write(prefix + ls_str, "results_max",    wr->loadstep_results_max);
            hdf5.write(prefix + ls_str, "results_min",    wr->loadstep_results_min);
            hdf5.write(prefix + ls_str, "results",        wr->loadstep_results);
            hdf5.write(prefix + ls_str, "eps_as_loading", wr->eps_as_loading[ls_num - 1]);
        }

        qDebug() << "[StressAnalysis] Computing effective elastic properties (homogenisation)...";
        auto props = wr->calculateElasticProperties();
        if (props.isValid) {
            qDebug() << "[StressAnalysis] v Elastic properties valid. Writing S, C, P matrices and moduli...";

            std::vector<std::vector<float>> mat_S(6, std::vector<float>(6));
            std::vector<std::vector<float>> mat_C(6, std::vector<float>(6));
            std::vector<std::vector<float>> mat_P(6, std::vector<float>(6));

            for (int r = 0; r < 6; ++r) {
                for (int c = 0; c < 6; ++c) {
                    mat_S[r][c] = static_cast<float>(props.S[r][c]);
                    mat_C[r][c] = static_cast<float>(props.C[r][c]);
                    mat_P[r][c] = static_cast<float>(props.P[r][c]);
                }
            }

            hdf5.write(prefix, "S_matrix", mat_S);
            hdf5.write(prefix, "C_matrix", mat_C);
            hdf5.write(prefix, "P_matrix", mat_P);

            auto safe_inv = [](double val) { return (std::abs(val) > 1e-20) ? static_cast<float>(1.0 / val) : 0.0f; };
            std::vector<float> moduli = {
                safe_inv(props.S[0][0]), safe_inv(props.S[1][1]), safe_inv(props.S[2][2]),
                safe_inv(props.S[3][3]), safe_inv(props.S[4][4]), safe_inv(props.S[5][5])
            };
            hdf5.write(prefix, "Effective_Moduli", moduli);

            qDebug() << "[StressAnalysis] Effective moduli (1/Sii) [Pa]:";
            qDebug() << QString("  Ex=%1  Ey=%2  Ez=%3  Gxy=%4  Gyz=%5  Gxz=%6")
                            .arg(moduli[0],0,'e',4).arg(moduli[1],0,'e',4).arg(moduli[2],0,'e',4)
                            .arg(moduli[3],0,'e',4).arg(moduli[4],0,'e',4).arg(moduli[5],0,'e',4);

            // Final fit of P_Hill from the main simulation results
            qDebug() << "[StressAnalysis] Computing final yield points and fitting P_Hill...";
            auto yield_points = m_hill.computeYieldPoints(wr, wr->local_cs, N, voxels);
            qDebug() << "[StressAnalysis]   Yield points obtained:" << (int)yield_points.size();

            if (!yield_points.empty() && m_hill.fit(yield_points)) {
                m_hill.saveToHDF5(hdf5, prefix);
                qDebug() << "[StressAnalysis] v P_Hill fitted and saved to HDF5.";
            } else {
                qWarning() << "[StressAnalysis] x Final P_Hill fit failed (too few points or fit error).";
            }
        } else {
            qWarning() << "[StressAnalysis] x calculateElasticProperties: invalid result, matrices not written.";
        }
    }

    qDebug() << "\n[StressAnalysis] Loading results into LoadStepManager from" << filename << "...";
    LoadStepManager::getInstance().LoadFromHDF5(filename);

    wr->clear_temp_data();
    delete wr;

    qDebug() << "[StressAnalysis] ████████████████████████████████████████████████";
    qDebug() << "[StressAnalysis] estimateStressWithANSYS FINISHED";
    qDebug() << "[StressAnalysis] ████████████████████████████████████████████████\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  PHASE 1.0: 6 canonical loads -> elastic compliance matrix S
//  Each load applies a unit strain along one component (ex, ey, ez, gxy, ...)
//  Result: S[6][6] via calculateElasticProperties
// ─────────────────────────────────────────────────────────────────────────────
bool StressAnalysis::computeSMatrix(short int numCubes, short int numPoints, int32_t ***voxels, double strain_val, double S_out[6][6])
{
    qDebug() << "\n[StressAnalysis::computeSMatrix] ────────────────────────────────";
    qDebug() << "[StressAnalysis::computeSMatrix] Computing elastic compliance matrix S";
    qDebug() << "[StressAnalysis::computeSMatrix]   6 canonical loads, strain_val =" << strain_val;

    ansysWrapper temp_wr(true);
    QString base_dir = Parameters::working_directory.isEmpty() ? QDir::currentPath() : Parameters::working_directory;
    QString work_dir = base_dir + "/phase1_s_matrix";
    temp_wr.setWorkingDirectory(work_dir);
    QDir().mkpath(work_dir);
    qDebug() << "[StressAnalysis::computeSMatrix]   ANSYS working directory:" << work_dir;

    temp_wr.setSeed(Parameters::seed);
    temp_wr.setNP(Parameters::num_threads);
    double c11 = 168.40e9, c12 = 121.40e9, c44 = 75.40e9;
    temp_wr.setAnisoMaterial(c11, c12, c12, c11, c12, c11, c44, c44, c44);
    temp_wr.setElemByNum(185);
    temp_wr.createFEfromArray8Node(voxels, numCubes, numPoints, true);

    // Canonical loads: one unit strain component at a time
    const char* comp_names[] = {"ex", "ey", "ez", "gxy", "gyz", "gxz"};
    std::vector<std::vector<double>> canonical = {
        {strain_val,0,0,0,0,0}, {0,strain_val,0,0,0,0}, {0,0,strain_val,0,0,0},
        {0,0,0,strain_val,0,0}, {0,0,0,0,strain_val,0}, {0,0,0,0,0,strain_val}
    };

    for (int k = 0; k < 6; ++k) {
        qDebug() << QString("[StressAnalysis::computeSMatrix]   Load #%1: %2 = %3")
                        .arg(k+1).arg(comp_names[k]).arg(strain_val, 0, 'e', 2);
        const auto& load = canonical[k];
        // applyComplexLoads() takes (eps_xy, eps_xz, eps_yz) in that param order,
        // but load[] is [ex,ey,ez,gxy,gyz,gxz] -- swap the last two to match.
        temp_wr.applyComplexLoads(0,0,0, numCubes,numCubes,numCubes,
                                  load[0],load[1],load[2],load[3],load[5],load[4]);
    }

    qDebug() << "[StressAnalysis::computeSMatrix]   Solving 6 load steps in ANSYS...";
    temp_wr.solveLS(1, (int)canonical.size());
    temp_wr.saveAll();

    qDebug() << "[StressAnalysis::computeSMatrix]   Launching ANSYS...";
    if (!temp_wr.run()) {
        qWarning() << "[StressAnalysis::computeSMatrix] x ANSYS run failed";
        return false;
    }
    qDebug() << "[StressAnalysis::computeSMatrix] v ANSYS finished. Computing elastic properties...";

    auto props = temp_wr.calculateElasticProperties();
    if (!props.isValid) {
        qWarning() << "[StressAnalysis::computeSMatrix] x calculateElasticProperties: invalid result";
        return false;
    }

    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            S_out[i][j] = props.S[i][j];

    qDebug() << "[StressAnalysis::computeSMatrix] v S matrix obtained. Diagonal S[i][i] [1/Pa]:";
    for (int i = 0; i < 6; ++i)
        qDebug() << QString("    S[%1][%1] = %2").arg(i).arg(S_out[i][i], 0, 'e', 4);

    temp_wr.clear_temp_data();
    qDebug() << "[StressAnalysis::computeSMatrix] ────────────────────────────────\n";
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  PHASE 1.5: Hill ellipsoid calibration
//  150 random deviatoric loads -> yield points -> LS fit of P_Hill
//  Loads are sampled uniformly in 6D deviatoric space and converted to
//  strain via S (from phase 1.0)
// ─────────────────────────────────────────────────────────────────────────────
bool StressAnalysis::calibrateHillMatrix(short int numCubes, short int numPoints, int32_t ***voxels, double strain_val, const double S_matrix[6][6])
{
    qDebug() << "\n[StressAnalysis::calibrateHillMatrix] ──────────────────────────────";
    qDebug() << "[StressAnalysis::calibrateHillMatrix] Calibrating Hill ellipsoid";

    ansysWrapper temp_wr(true);
    QString base_dir = Parameters::working_directory.isEmpty() ? QDir::currentPath() : Parameters::working_directory;
    QString work_dir = base_dir + "/phase1_hill_calibration";
    temp_wr.setWorkingDirectory(work_dir);
    QDir().mkpath(work_dir);
    qDebug() << "[StressAnalysis::calibrateHillMatrix]   Working directory:" << work_dir;

    temp_wr.setSeed(Parameters::seed);
    temp_wr.setNP(Parameters::num_threads);
    double c11 = 168.40e9, c12 = 121.40e9, c44 = 75.40e9;
    temp_wr.setAnisoMaterial(c11, c12, c12, c11, c12, c11, c44, c44, c44);
    temp_wr.setElemByNum(185);
    temp_wr.createFEfromArray8Node(voxels, numCubes, numPoints, true);

    // num_calib is a member field (editable from the UI).
    qDebug() << "[StressAnalysis::calibrateHillMatrix]   Calibration load cases :" << num_calib;
    qDebug() << "[StressAnalysis::calibrateHillMatrix]   RNG seed: Parameters::seed + 1 =" << (Parameters::seed + 1);

    std::vector<std::vector<double>> load_cases;
    std::mt19937 gen(Parameters::seed + 1);
    std::normal_distribution<double> dist(0.0, 1.0);

    int retries = 0;
    for (int n = 0; n < num_calib; ++n) {
        // Random unit vector in 6D (uniform on sphere via normal distribution)
        double d_sigma[6], norm_sq = 0.0;
        for (int i = 0; i < 6; ++i) {
            d_sigma[i] = dist(gen);
            norm_sq += d_sigma[i] * d_sigma[i];
        }
        double norm = std::sqrt(norm_sq);
        if (norm < 1e-12) { --n; ++retries; continue; }
        for (int i = 0; i < 6; ++i) d_sigma[i] /= norm;

        // Strain from stress via S: eps = S * sigma
        double d_eps[6] = {0}, eps_norm_sq = 0.0;
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j)
                d_eps[i] += S_matrix[i][j] * d_sigma[j];
            eps_norm_sq += d_eps[i] * d_eps[i];
        }
        double eps_norm = std::sqrt(eps_norm_sq);

        // Normalise strain to strain_val
        std::vector<double> eps(6);
        for (int i = 0; i < 6; ++i)
            eps[i] = (d_eps[i] / eps_norm) * strain_val;
        load_cases.push_back(eps);
    }

    qDebug() << "[StressAnalysis::calibrateHillMatrix]   Load cases generated :" << (int)load_cases.size();
    qDebug() << "[StressAnalysis::calibrateHillMatrix]   Retries (||d||~0)    :" << retries;
    qDebug() << "[StressAnalysis::calibrateHillMatrix]   First 3 strains:";
    for (int i = 0; i < std::min(3, (int)load_cases.size()); ++i) {
        const auto& e = load_cases[i];
        qDebug() << QString("    [%1] ex=%2 ey=%3 ez=%4 gxy=%5 gyz=%6 gxz=%7")
                        .arg(i).arg(e[0],0,'e',3).arg(e[1],0,'e',3).arg(e[2],0,'e',3)
                        .arg(e[3],0,'e',3).arg(e[4],0,'e',3).arg(e[5],0,'e',3);
    }

    qDebug() << "[StressAnalysis::calibrateHillMatrix]   Applying load cases to model...";
    for (const auto& load : load_cases) {
        // applyComplexLoads() takes (eps_xy, eps_xz, eps_yz) in that param order,
        // but load[] is [ex,ey,ez,gxy,gyz,gxz] -- swap the last two to match.
        temp_wr.applyComplexLoads(0, 0, 0, numCubes, numCubes, numCubes,
                                  load[0], load[1], load[2], load[3], load[5], load[4]);
    }

    qDebug() << "[StressAnalysis::calibrateHillMatrix]   Solving" << (int)load_cases.size() << "steps in ANSYS...";
    temp_wr.solveLS(1, (int)load_cases.size());
    temp_wr.saveAll();

    qDebug() << "[StressAnalysis::calibrateHillMatrix]   Launching ANSYS (calibration)...";
    if (!temp_wr.run()) {
        qWarning() << "[StressAnalysis::calibrateHillMatrix] x ANSYS run failed";
        return false;
    }
    qDebug() << "[StressAnalysis::calibrateHillMatrix] v ANSYS finished.";

    qDebug() << "[StressAnalysis::calibrateHillMatrix]   Extracting yield points (Schmid CRSS criterion)...";
    auto yield_points = m_hill.computeYieldPoints(&temp_wr, temp_wr.local_cs, numCubes, voxels);
    temp_wr.clear_temp_data();

    qDebug() << "[StressAnalysis::calibrateHillMatrix]   Yield points obtained :" << (int)yield_points.size();
    qDebug() << "[StressAnalysis::calibrateHillMatrix]   Minimum for LS fit    : 21";

    if ((int)yield_points.size() < 21) {
        qWarning() << "[StressAnalysis::calibrateHillMatrix] x Not enough yield points ("
                   << (int)yield_points.size() << "< 21). Hill fit impossible.";
        return false;
    }

    qDebug() << "[StressAnalysis::calibrateHillMatrix]   Running LS fit of P_Hill on" << (int)yield_points.size() << "points...";
    bool ok = m_hill.fit(yield_points);

    if (ok)
        qDebug() << "[StressAnalysis::calibrateHillMatrix] v Fit SUCCEEDED.";
    else
        qWarning() << "[StressAnalysis::calibrateHillMatrix] x HillCriterion::fit failed.";

    qDebug() << "[StressAnalysis::calibrateHillMatrix] ──────────────────────────────\n";
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Single known load case via ANSYS: one load step, no Hill calibration, no
//  HDF5 output.  Used by the single-shot / verification UI.  Modeled on
//  computeSMatrix() but with one applied load instead of six.
// ─────────────────────────────────────────────────────────────────────────────
SingleShotResult StressAnalysis::solveSingleLoadCase(short int numCubes, short int numPoints,
                                                      int32_t ***voxels, const double eps[6])
{
    SingleShotResult out;

    qDebug() << "\n[StressAnalysis::solveSingleLoadCase] ────────────────────────────────";
    qDebug() << "[StressAnalysis::solveSingleLoadCase]   eps =" << eps[0] << eps[1] << eps[2]
             << eps[3] << eps[4] << eps[5];

    static ansysWrapper temp_wr(true);
    QString base_dir = Parameters::working_directory.isEmpty() ? QDir::currentPath() : Parameters::working_directory;
    QString work_dir = base_dir + "/phase0_single_shot";
    temp_wr.setWorkingDirectory(work_dir);
    QDir().mkpath(work_dir);
    qDebug() << "[StressAnalysis::solveSingleLoadCase]   ANSYS working directory:" << work_dir;

    temp_wr.setSeed(Parameters::seed);
    temp_wr.setNP(Parameters::num_threads);
    double c11 = 168.40e9, c12 = 121.40e9, c44 = 75.40e9;
    qDebug() << "[StressAnalysis::solveSingleLoadCase]   [DEBUG] seed =" << Parameters::seed
             << " C11 =" << c11 << " C12 =" << c12 << " C44 =" << c44;
    temp_wr.setAnisoMaterial(c11, c12, c12, c11, c12, c11, c44, c44, c44);
    temp_wr.setElemByNum(185);

    // Same grain-count scan FFT's buildGrainField() does, so both solvers
    // index the exact same orientation array the exact same way.
    int nGrains = 0;
    for (int i = 0; i < numCubes; ++i)
        for (int j = 0; j < numCubes; ++j)
            for (int k = 0; k < numCubes; ++k)
                if (voxels[i][j][k] > nGrains) nGrains = voxels[i][j][k];
    std::array<double,3> forcedOrient;
    const bool useForced = getForcedOrientationDebugOverride(forcedOrient);
    auto sharedOrient = buildGrainOrientations(nGrains, Parameters::seed, useForced ? &forcedOrient : nullptr);
    if (useForced) {
        const double r2d = 180.0 / M_PI;
        qDebug() << "[StressAnalysis::solveSingleLoadCase]   [DEBUG] MATVIZ_FORCE_ORIENT_DEG active: every grain forced to"
                 << forcedOrient[0]*r2d << forcedOrient[1]*r2d << forcedOrient[2]*r2d << "(Bunge ZXZ, deg)";
    }
    qDebug() << "[StressAnalysis::solveSingleLoadCase]   [DEBUG] nGrains (scanned) =" << nGrains;
    if (nGrains >= 1) {
        const double r2d = 180.0 / M_PI;
        const auto& g1 = sharedOrient[1];
        qDebug() << "[StressAnalysis::solveSingleLoadCase]   [DEBUG] grain#1 orientation (shared Bunge ZXZ, deg) ="
                 << g1[0]*r2d << g1[1]*r2d << g1[2]*r2d;
    }

    temp_wr.createFEfromArray8Node(voxels, numCubes, numPoints, true, sharedOrient);
    qDebug() << "[StressAnalysis::solveSingleLoadCase]   [DEBUG] numCubes =" << numCubes
             << " numPoints(param) =" << numPoints
             << " solid_fraction =" << temp_wr.m_solid_fraction
             << " local_cs entries =" << (int)temp_wr.local_cs.size();
    // local_cs[0] is CS#11, the "no-grain/matrix" placeholder (voxel id 0);
    // local_cs[1] is grain id 1's orientation (CS#12), etc.
    if (temp_wr.local_cs.size() > 1) {
        const auto& g1 = temp_wr.local_cs[1]; // grain id 1, ANSYS ZXY (THXY,THYZ,THZX) degrees
        qDebug() << "[StressAnalysis::solveSingleLoadCase]   [DEBUG] grain#1 orientation (ANSYS ZXY, deg) ="
                 << g1[0] << g1[1] << g1[2];
    }

    // applyComplexLoads() takes (eps_xy, eps_xz, eps_yz) in that param order,
    // but eps[] is [ex,ey,ez,exy,eyz,exz] -- swap the last two to match.
    temp_wr.applyComplexLoads(0, 0, 0, numCubes, numCubes, numCubes,
                            eps[0], eps[1], eps[2], eps[3], eps[5], eps[4]);

    qDebug() << "[StressAnalysis::solveSingleLoadCase]   Solving 1 load step in ANSYS...";
    temp_wr.solveLS(1, 1);
    temp_wr.saveAll();
    temp_wr.saveElementAverages();

    qDebug() << "[StressAnalysis::solveSingleLoadCase]   Launching ANSYS...";
    if (!temp_wr.run()) {
        out.errorMessage = QObject::tr("ANSYS run failed");
        qWarning() << "[StressAnalysis::solveSingleLoadCase] x" << out.errorMessage;
        return out;
    }

    temp_wr.load_loadstep(1);
    temp_wr.loadElementAveragedResults(1);
    // Note: the ANSYS result-export script (ansyswrapper.cpp) only extracts
    // ID..EpsXZ (19 columns, up to index SXZ) -- SEQV is never written to the
    // CSV, so it must be derived from the averaged stress tensor here.
    if ((int)temp_wr.loadstep_results_avg.size() <= SXZ) {
        out.errorMessage = QObject::tr("ANSYS returned no per-step results");
        qWarning() << "[StressAnalysis::solveSingleLoadCase] x" << out.errorMessage;
        temp_wr.clear_temp_data();
        return out;
    }

    const auto& avg = temp_wr.loadstep_results_avg;
    out.macro_stress[0] = avg[SX];  out.macro_stress[1] = avg[SY];  out.macro_stress[2] = avg[SZ];
    out.macro_stress[3] = avg[SXY]; out.macro_stress[4] = avg[SYZ]; out.macro_stress[5] = avg[SXZ];
    out.von_mises = std::sqrt(0.5 * (
        std::pow(avg[SX] - avg[SY], 2) + std::pow(avg[SY] - avg[SZ], 2) + std::pow(avg[SZ] - avg[SX], 2) +
        6.0 * (std::pow(avg[SXY], 2) + std::pow(avg[SYZ], 2) + std::pow(avg[SXZ], 2))));
    out.ok        = true;

    // [DEBUG] Directly probe the SOLVED field at one CE-constrained face pair
    // and one D-prescribed corner pair, to see whether ANSYS actually honored
    // the periodic constraints (vs. the node-weighted AVG EPS metric above,
    // which is only a coarse proxy and may not be reliable on its own).
    {
        const float mid = numCubes / 2.0f;
        float ux_x1 = temp_wr.getValByCoord(0.0f, mid, mid, UX);
        float ux_x2 = temp_wr.getValByCoord((float)numCubes, mid, mid, UX);
        qDebug() << "[StressAnalysis::solveSingleLoadCase]   [DEBUG] CE face-pair (x=0 vs x=numCubes, mid y,z): UX ="
                 << ux_x1 << "/" << ux_x2 << " actual jump =" << (ux_x2 - ux_x1)
                 << " expected =" << eps[0] * numCubes;

        float ux_c0 = temp_wr.getValByCoord(0.0f, 0.0f, 0.0f, UX);
        float ux_c1 = temp_wr.getValByCoord((float)numCubes, 0.0f, 0.0f, UX);
        qDebug() << "[StressAnalysis::solveSingleLoadCase]   [DEBUG] D corner-pair (0,0,0) vs (numCubes,0,0): UX ="
                 << ux_c0 << "/" << ux_c1 << " actual jump =" << (ux_c1 - ux_c0)
                 << " expected =" << eps[0] * numCubes;
    }

    temp_wr.clear_temp_data();

    qDebug() << "[StressAnalysis::solveSingleLoadCase]   [DEBUG] macro_stress (Pa) sx,sy,sz,sxy,syz,sxz ="
             << out.macro_stress[0] << out.macro_stress[1] << out.macro_stress[2]
             << out.macro_stress[3] << out.macro_stress[4] << out.macro_stress[5];
    qDebug() << "[StressAnalysis::solveSingleLoadCase] v done. von_mises =" << out.von_mises;
    qDebug() << "[StressAnalysis::solveSingleLoadCase] ────────────────────────────────\n";
    return out;
}

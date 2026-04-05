#include "stressanalysis.h"
#include "ansyswrapper.h"
#include "hdf5wrapper.h"
#include "parameters.h"
#include "loadstepmanager.h"
#include <random>

void StressAnalysis::estimateStressWithANSYS(short int numCubes, short int numPoints, int32_t ***voxels)
{
    const auto N = numCubes;
    const double strain_val = 1e-04;
    const int    num_samples = 300;

    double S_matrix[6][6] = {0};
    qDebug() << "\n[PHASE 1.0] Running 6 canonical loads to find S matrix...";
    if (!computeSMatrix(numCubes, numPoints, voxels, strain_val, S_matrix)) {
        qCritical() << "Failed to compute S matrix. Exiting...";
        return;
    }

    qDebug() << "\n[PHASE 1.5] Running 50 smart loads to fit Hill ellipsoid...";
    if (!calibrateHillMatrix(numCubes, numPoints, voxels, strain_val, S_matrix)) {
        qCritical() << "Failed to calibrate Hill ellipsoid. Exiting...";
        return;
    }

    qDebug() << "\n[PHASE 2.0] Generating 300 final loads via Hill Matrix...";
    std::vector<std::vector<double>> load_cases = m_hill.generateLoads(num_samples, strain_val, S_matrix);

    if (load_cases.empty()) {
        qCritical() << "Final load cases generation failed. Exiting...";
        return;
    }

    wr = new ansysWrapper(true);
    if (!Parameters::working_directory.isEmpty())
        wr->setWorkingDirectory(Parameters::working_directory);
    wr->setSeed(Parameters::seed);
    wr->setNP(Parameters::num_threads);

    double c11 = 168.40e9, c12 = 121.40e9, c44 = 75.40e9;
    wr->setAnisoMaterial(c11, c12, c12, c11, c12, c11, c44, c44, c44);
    wr->setElemByNum(185);
    wr->createFEfromArray8Node(voxels, N, numPoints, true);

    for (const auto& load : load_cases) {
        wr->applyComplexLoads(0, 0, 0, N, N, N, load[0], load[1], load[2], load[3], load[4], load[5]);
    }

    wr->solveLS(1, (int)load_cases.size());
    wr->saveAll();

    qDebug() << "Running Main Simulation (300 cases)...";
    if (!wr->run()) {
        qCritical() << "Ansys run was unsuccessful. Exiting...";
        delete wr;
        return;
    }

    QString filename = Parameters::filename.length() ? Parameters::filename : "current_ls.hdf5";
    if (!Parameters::filename.length()) {
        qDebug() << "No output file with -o option. Saving to " << filename;
        if (QFile::exists(filename) && QFile::remove(filename))
            qDebug() << "\tFile exists. Removed.";
    }

    {
        HDF5Wrapper hdf5(filename.toStdString());
        int last_set = hdf5.readInt("/", "last_set");
        if (last_set == -1) {
            last_set = 1;
            hdf5.write("/", "last_set", last_set);
        } else {
            last_set += 1;
            hdf5.update("/", "last_set", last_set);
        }

        std::string prefix = ("/" + QString::number(last_set)).toStdString();
        hdf5.write(prefix, "voxels",    Parameters::voxels, Parameters::size);
        hdf5.write(prefix, "cubeSize",  Parameters::size);
        hdf5.write(prefix, "numPoints", Parameters::points);
        hdf5.write(prefix, "local_cs",  wr->local_cs);

        for (size_t ls_num = 1; ls_num <= wr->eps_as_loading.size(); ls_num++) {
            std::string ls_str = "/ls_" + std::to_string(ls_num);
            wr->load_loadstep(ls_num);
            hdf5.write(prefix + ls_str, "results_avg",    wr->loadstep_results_avg);
            hdf5.write(prefix + ls_str, "results_max",    wr->loadstep_results_max);
            hdf5.write(prefix + ls_str, "results_min",    wr->loadstep_results_min);
            hdf5.write(prefix + ls_str, "results",        wr->loadstep_results);
            hdf5.write(prefix + ls_str, "eps_as_loading", wr->eps_as_loading[ls_num - 1]);
        }

        auto props = wr->calculateElasticProperties();
        if (props.isValid) {
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

            auto yield_points = m_hill.computeYieldPoints(wr, wr->local_cs, N, voxels);
            if (!yield_points.empty() && m_hill.fit(yield_points)) {
                m_hill.saveToHDF5(hdf5, prefix);
                qDebug() << "P_Hill successfully updated and saved to HDF5.";
            }
        }
    }

    LoadStepManager::getInstance().LoadFromHDF5(filename);
    wr->clear_temp_data();
    delete wr;
}

bool StressAnalysis::computeSMatrix(short int numCubes, short int numPoints, int32_t ***voxels, double strain_val, double S_out[6][6])
{
    ansysWrapper temp_wr(true);
    QString base_dir = Parameters::working_directory.isEmpty() ? QDir::currentPath() : Parameters::working_directory;
    temp_wr.setWorkingDirectory(base_dir + "/phase1_s_matrix");
    QDir().mkpath(base_dir + "/phase1_s_matrix");

    temp_wr.setSeed(Parameters::seed);
    temp_wr.setNP(Parameters::num_threads);
    double c11 = 168.40e9, c12 = 121.40e9, c44 = 75.40e9;
    temp_wr.setAnisoMaterial(c11, c12, c12, c11, c12, c11, c44, c44, c44);
    temp_wr.setElemByNum(185);
    temp_wr.createFEfromArray8Node(voxels, numCubes, numPoints, true);

    std::vector<std::vector<double>> canonical = {
        {strain_val,0,0,0,0,0}, {0,strain_val,0,0,0,0}, {0,0,strain_val,0,0,0},
        {0,0,0,strain_val,0,0}, {0,0,0,0,strain_val,0}, {0,0,0,0,0,strain_val}
    };

    for (const auto& load : canonical)
        temp_wr.applyComplexLoads(0,0,0, numCubes,numCubes,numCubes, load[0],load[1],load[2],load[3],load[4],load[5]);

    temp_wr.solveLS(1, (int)canonical.size());
    temp_wr.saveAll();
    if (!temp_wr.run()) return false;

    auto props = temp_wr.calculateElasticProperties();
    if (!props.isValid) return false;

    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            S_out[i][j] = props.S[i][j];

    temp_wr.clear_temp_data();
    return true;
}

bool StressAnalysis::calibrateHillMatrix(short int numCubes, short int numPoints, int32_t ***voxels, double strain_val, const double S_matrix[6][6])
{
    ansysWrapper temp_wr(true);
    QString base_dir = Parameters::working_directory.isEmpty() ? QDir::currentPath() : Parameters::working_directory;
    temp_wr.setWorkingDirectory(base_dir + "/phase1_hill_calibration");
    QDir().mkpath(base_dir + "/phase1_hill_calibration");

    temp_wr.setSeed(Parameters::seed);
    temp_wr.setNP(Parameters::num_threads);
    double c11 = 168.40e9, c12 = 121.40e9, c44 = 75.40e9;
    temp_wr.setAnisoMaterial(c11, c12, c12, c11, c12, c11, c44, c44, c44);
    temp_wr.setElemByNum(185);
    temp_wr.createFEfromArray8Node(voxels, numCubes, numPoints, true);

    int num_calib = 50;
    std::vector<std::vector<double>> load_cases;
    std::mt19937 gen(Parameters::seed + 1);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int n = 0; n < num_calib; ++n) {
        double d_sigma[6], norm_sq = 0.0;
        for (int i = 0; i < 6; ++i) {
            d_sigma[i] = dist(gen);
            norm_sq += d_sigma[i] * d_sigma[i];
        }
        double norm = std::sqrt(norm_sq);
        if (norm < 1e-12) { --n; continue; }
        for (int i = 0; i < 6; ++i) d_sigma[i] /= norm;

        double d_eps[6] = {0}, eps_norm_sq = 0.0;
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                d_eps[i] += S_matrix[i][j] * d_sigma[j];
            }
            eps_norm_sq += d_eps[i] * d_eps[i];
        }
        double eps_norm = std::sqrt(eps_norm_sq);

        std::vector<double> eps(6);
        for (int i = 0; i < 6; ++i) {
            eps[i] = (d_eps[i] / eps_norm) * strain_val;
        }
        load_cases.push_back(eps);
    }

    for (const auto& load : load_cases) {
        temp_wr.applyComplexLoads(0, 0, 0, numCubes, numCubes, numCubes, load[0], load[1], load[2], load[3], load[4], load[5]);
    }

    temp_wr.solveLS(1, (int)load_cases.size());
    temp_wr.saveAll();
    if (!temp_wr.run()) return false;

    auto yield_points = m_hill.computeYieldPoints(&temp_wr, temp_wr.local_cs, numCubes, voxels);
    temp_wr.clear_temp_data();

    if (yield_points.size() < 21) {
        qWarning() << "Not enough yield points for Hill fit.";
        return false;
    }

    return m_hill.fit(yield_points);
}

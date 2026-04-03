#include "stressanalysis.h"
#include "ansyswrapper.h"
#include "hdf5wrapper.h"
#include "parameters.h"
#include "loadstepmanager.h"
#include <random>

void StressAnalysis::estimateStressWithANSYS(short int numCubes, short int numPoints, int32_t ***voxels)
{
    const auto N = numCubes;
    wr = new ansysWrapper(true);
    if (! Parameters::working_directory.isEmpty())
        wr->setWorkingDirectory(Parameters::working_directory);

    wr->setSeed(Parameters::seed);
    wr->setNP(Parameters::num_threads);
    //wr->setMaterial(2.1e10, 0.3, 0);

    double c11 = 168.40e9, c12=121.40e9, c44=75.40e9; // copper bcc single crystal  https://solidmechanics.org/Text/Chapter3_2/Chapter3_2.php#Sect3_2_17
    // wr->setAnisoMaterial(c11, c12, c12, c11, c12, c11, c44, c44, c44) ;

    // double E  = 2.1e7; // Young's Modulus (EX)      //
    // double nu = 0.3;   // Poisson's Ratio (PRXY)    // steel or cast iron

    // double G = E / (2.0 * (1.0 + nu));                      // Shear Modulus (C44)
    // double lambda = (E * nu) / ((1.0 + nu) * (1.0 - 2.0 * nu)); // Lame's first parameter

    // double c11 = lambda + 2.0 * G;
    // double c12 = lambda;
    // double c44 = G;

    wr->setAnisoMaterial(c11, c12, c12, c11, c12, c11, c44, c44, c44);
    /*
    double  c11 = 9e9, // test material
            c12 = 2e9,
            c13 = 3e9,
            c22 = 8e9,
            c23 = 5e9,
            c33 = 7e9,
            c44 = 6e9,
            c55 = 4e9,
            c66 = 1e9;
    wr->setAnisoMaterial(c11, c12, c13, c22, c23, c33, c44, c55, c66) ;
*/
    wr->setElemByNum(185);

    //wr->createFEfromArray(voxels, N, numPoints, false);
    wr->createFEfromArray8Node(voxels, N, numPoints, true);
    //wr->addStrainToBCMacroBlob();
    // ... (ініціалізація wr, матеріалів, вокселів залишається як було) ...

    const double strain_val = 1e-04;
    int num_samples = 300;

    // ====================================================================
    // ВИБІР СТРАТЕГІЇ ГЕНЕРАЦІЇ НАВАНТАЖЕНЬ (Закоментуйте непотрібне)
    // ====================================================================
    std::vector<std::vector<double>> load_cases;

    // ВАРИАНТ А: Прямая генерация деформаций
    // load_cases = generateDirectStrainLoads(num_samples, strain_val);

    // ВАРИАНТ Б: Умная генерация через матрицу S (переход в напряжения)
    load_cases = generateSmartStressLoads(num_samples, strain_val, numCubes, numPoints, voxels);

    if (load_cases.empty()) {
        qCritical() << "Load cases generation failed. Exiting...";
        return;
    }

    qDebug() << "Running Phase 2: Simulating" << load_cases.size() << "transformed load states...";

    // Застосовуємо нові навантаження для Фази 2
    for (const auto& load : load_cases) {
        wr->applyComplexLoads(0, 0, 0, N, N, N, load[0], load[1], load[2], load[3], load[4], load[5]);
    }

    wr->solveLS(1, load_cases.size());
    wr->saveAll();
    bool success = wr->run();

    if (!success)
    {
        qCritical() << "Ansys run was unsuccessfull. Exiting...";
        return;
    }
    QString filename;
    if (Parameters::filename.length())
    {
        filename = Parameters::filename;
        qDebug() << "Saving results to " << filename;
    }
    else
    {
        filename = "current_ls.hdf5";
        qDebug() << "No output file with -o option. Saving to " << filename;
        if (QFile::exists(filename) && QFile::remove(filename))
        {
            qDebug() << "\t File exists. Removed";
        }
    }
    {
        HDF5Wrapper hdf5(filename.toStdString());

        int last_set = hdf5.readInt("/", "last_set");
        if (last_set == -1) // Handle missing dataset case
        {
            last_set = 1;
            hdf5.write("/", "last_set", last_set);
        }
        else
        {
            last_set += 1;
            hdf5.update("/", "last_set", last_set);
        }

        std::string prefix = ("/" + QString::number(last_set)).toStdString();

        hdf5.write(prefix, "voxels", Parameters::voxels, Parameters::size);
        hdf5.write(prefix, "cubeSize", Parameters::size);
        hdf5.write(prefix, "numPoints", Parameters::points);
        hdf5.write(prefix, "local_cs", wr->local_cs);
        for (size_t ls_num = 1; ls_num <= wr->eps_as_loading.size(); ls_num++)
        {
            std::string ls_str = "/ls_" + std::to_string(ls_num);
            wr->load_loadstep(ls_num);

            hdf5.write(prefix + ls_str, "results_avg", wr->loadstep_results_avg);
            hdf5.write(prefix + ls_str, "results_max", wr->loadstep_results_max);
            hdf5.write(prefix + ls_str, "results_min", wr->loadstep_results_min);
            hdf5.write(prefix + ls_str, "results", wr->loadstep_results);
            hdf5.write(prefix + ls_str, "eps_as_loading", wr->eps_as_loading[ls_num-1]);
        }
        qDebug() << "Calculating effective elastic properties...";
        auto props = wr->calculateElasticProperties();

        if (props.isValid) {
            std::vector<std::vector<float>> mat_S(6, std::vector<float>(6));
            std::vector<std::vector<float>> mat_C(6, std::vector<float>(6));
            std::vector<std::vector<float>> mat_P(6, std::vector<float>(6));

            for(int r=0; r<6; ++r) {
                for(int c=0; c<6; ++c) {
                    mat_S[r][c] = static_cast<float>(props.S[r][c]);
                    mat_C[r][c] = static_cast<float>(props.C[r][c]);
                    mat_P[r][c] = static_cast<float>(props.P[r][c]);
                }
            }

            hdf5.write(prefix, "S_matrix", mat_S);
            hdf5.write(prefix, "C_matrix", mat_C);
            hdf5.write(prefix, "P_matrix", mat_P);

            std::vector<float> moduli;
            auto safe_inv = [](double val) {
                return (std::abs(val) > 1e-20) ? static_cast<float>(1.0 / val) : 0.0f;
            };

            moduli.push_back(safe_inv(props.S[0][0])); // Ex
            moduli.push_back(safe_inv(props.S[1][1])); // Ey
            moduli.push_back(safe_inv(props.S[2][2])); // Ez
            moduli.push_back(safe_inv(props.S[3][3])); // Gxy
            moduli.push_back(safe_inv(props.S[4][4])); // Gyz
            moduli.push_back(safe_inv(props.S[5][5])); // Gxz

            hdf5.write(prefix, "Effective_Moduli", moduli);

            qDebug() << "Elastic matrices saved as 6x6 float arrays.";
        } else {
            qWarning() << "Failed to calculate elastic matrices.";
        }
    }
    LoadStepManager::getInstance().LoadFromHDF5(filename);
    wr->clear_temp_data();
    delete wr;
}

// ====================================================================
// ГЕНЕРАТОР 1: Прямая генерация (Стратегия А)
// ====================================================================
std::vector<std::vector<double>> StressAnalysis::generateDirectStrainLoads(int num_samples, double strain_val) {
    std::vector<std::vector<double>> load_cases;
    std::mt19937 gen(Parameters::seed);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int n = 0; n < num_samples; ++n) {
        std::vector<double> d_eps(6, 0.0);
        double norm_sq = 0.0;
        for (int i = 0; i < 6; ++i) {
            d_eps[i] = dist(gen);
            norm_sq += d_eps[i] * d_eps[i];
        }
        double norm = std::sqrt(norm_sq);
        if (norm < 1e-12) { --n; continue; }

        for (int i = 0; i < 6; ++i) {
            d_eps[i] = (d_eps[i] / norm) * strain_val;
        }
        load_cases.push_back(d_eps);
    }
    return load_cases;
}

// ====================================================================
// ГЕНЕРАТОР 2: Умная генерация через матрицу S (Стратегия Б)
// ====================================================================
std::vector<std::vector<double>> StressAnalysis::generateSmartStressLoads(int num_samples, double strain_val, short int numCubes, short int numPoints, int32_t ***voxels) {
    ansysWrapper temp_wr(true);
    QString base_dir = Parameters::working_directory.isEmpty() ? QDir::currentPath() : Parameters::working_directory;
    QString phase1_dir = base_dir + "/phase1_calibration";
    QDir().mkpath(phase1_dir);
    temp_wr.setWorkingDirectory(phase1_dir);

    temp_wr.setSeed(Parameters::seed);
    temp_wr.setNP(Parameters::num_threads);
    double c11 = 168.40e9, c12=121.40e9, c44=75.40e9;
    temp_wr.setAnisoMaterial(c11, c12, c12, c11, c12, c11, c44, c44, c44);
    temp_wr.setElemByNum(185);
    temp_wr.createFEfromArray8Node(voxels, numCubes, numPoints, true);

    std::vector<std::vector<double>> canonical_cases = {
        {strain_val, 0, 0, 0, 0, 0}, {0, strain_val, 0, 0, 0, 0},
        {0, 0, strain_val, 0, 0, 0}, {0, 0, 0, strain_val, 0, 0},
        {0, 0, 0, 0, strain_val, 0}, {0, 0, 0, 0, 0, strain_val}
    };

    for (const auto& load : canonical_cases) {
        temp_wr.applyComplexLoads(0, 0, 0, numCubes, numCubes, numCubes, load[0], load[1], load[2], load[3], load[4], load[5]);
    }

    temp_wr.solveLS(1, canonical_cases.size());
    temp_wr.saveAll();

    qDebug() << "Running Calibration (Phase 1) for Smart Stress Strategy...";
    if (!temp_wr.run()) {
        qCritical() << "Calibration Ansys run failed. Returning empty loads.";
        return {};
    }

    auto props = temp_wr.calculateElasticProperties();
    if (!props.isValid) {
        qCritical() << "Failed to calculate S matrix. Returning empty loads.";
        return {};
    }

    double S_matrix[6][6];
    for(int i=0; i<6; ++i)
        for(int j=0; j<6; ++j)
            S_matrix[i][j] = props.S[i][j];

    temp_wr.clear_temp_data();

    std::vector<std::vector<double>> load_cases;
    std::mt19937 gen(Parameters::seed);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int n = 0; n < num_samples; ++n) {
        double d_sigma[6], norm_sq = 0.0;
        for (int i = 0; i < 6; ++i) {
            d_sigma[i] = dist(gen);
            norm_sq += d_sigma[i] * d_sigma[i];
        }
        double norm = std::sqrt(norm_sq);
        if (norm < 1e-12) { --n; continue; }
        for (int i = 0; i < 6; ++i) d_sigma[i] /= norm;

        std::vector<double> d_eps(6, 0.0);
        double eps_norm_sq = 0.0;
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                d_eps[i] += S_matrix[i][j] * d_sigma[j];
            }
            eps_norm_sq += d_eps[i] * d_eps[i];
        }
        double eps_norm = std::sqrt(eps_norm_sq);

        for (int i = 0; i < 6; ++i) {
            d_eps[i] = (d_eps[i] / eps_norm) * strain_val;
        }
        load_cases.push_back(d_eps);
    }
    return load_cases;
}

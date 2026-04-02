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
    const double strain_val = 1e-04;
    std::vector<std::vector<double>> load_cases;

    // Очищаємо попередні навантаження
    load_cases.clear();

    // Кількість точок для ймовірнісного покриття (Рівень 2)
    // 300 - хороший компроміс між щільністю покриття та часом симуляції
    int num_samples = 300;

    // Ініціалізація стандартного генератора
    std::mt19937 gen(Parameters::seed);

    // ВАЖЛИВО: Використовуємо саме нормальний розподіл, а не рівномірний (uniform).
    // Тільки нормальний розподіл N(0, 1) після нормалізації дає рівномірне покриття сфери.
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int n = 0; n < num_samples; ++n) {
        std::vector<double> eps(6, 0.0);
        double norm_sq = 0.0;

        // 1. Генеруємо 6 незалежних нормально розподілених координат
        for (int i = 0; i < 6; ++i) {
            eps[i] = dist(gen);
            norm_sq += eps[i] * eps[i];
        }

        // Обчислюємо довжину (норму) отриманого вектора
        double norm = std::sqrt(norm_sq);

        // Захист від ділення на нуль (вкрай рідкісний випадок, але потрібен для стабільності)
        if (norm < 1e-12) {
            --n; // Відкидаємо цю точку і генеруємо наново
            continue;
        }

        // 2. Проектуємо на одиничну сферу і масштабуємо до strain_val
        for (int i = 0; i < 6; ++i) {
            eps[i] = (eps[i] / norm) * strain_val;
        }

        load_cases.push_back(eps);
    }

    qDebug() << "Згенеровано" << load_cases.size() << "випадкових навантажень на сфері S^5.";

    for (const auto& load : load_cases)
    {
        double ex  = load[0];
        double ey  = load[1];
        double ez  = load[2];
        double exy = load[3];
        double exz = load[4];
        double eyz = load[5];

        wr->applyComplexLoads(0, 0, 0, N, N, N,
                              ex, ey, ez,
                              exy, exz, eyz);
    }
    const int n_total_steps = load_cases.size();

    wr->solveLS(1, n_total_steps);
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

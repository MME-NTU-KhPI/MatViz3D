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

    // 1. Ініціалізація матриці C (мідь BCC) — наближення P для першого запуску
    double C[6][6] = {0};
    for(int i=0; i<3; ++i) {
        C[i][i] = c11;
        for(int j=0; j<3; ++j) if(i != j) C[i][j] = c12;
        C[i+3][i+3] = c44;
    }

    // 2. Спроба завантажити P_matrix з попереднього запуску
    // Якщо файл існує та містить P — використовуємо її, інакше повертаємося до C
    double P_for_chol[6][6] = {0};
    bool use_P_from_file = false;

    QString hdf5_filename_prev = Parameters::filename.length() ? Parameters::filename : "current_ls.hdf5";
    if (QFile::exists(hdf5_filename_prev))
    {
        try {
            HDF5Wrapper hdf5_prev(hdf5_filename_prev.toStdString());
            int prev_last_set = hdf5_prev.readInt("/", "last_set");
            if (prev_last_set > 0)
            {
                std::string prev_prefix = ("/" + QString::number(prev_last_set)).toStdString();
                auto P_prev = hdf5_prev.readVectorVectorFloat(prev_prefix, "P_matrix");
                if (!P_prev.empty() && (int)P_prev.size() == 6 && (int)P_prev[0].size() == 6)
                {
                    for (int i = 0; i < 6; ++i)
                        for (int j = 0; j < 6; ++j)
                            P_for_chol[i][j] = static_cast<double>(P_prev[i][j]);
                    use_P_from_file = true;
                    qDebug() << "Using P_matrix from previous run (set" << prev_last_set << ") for load generation.";
                }
            }
        } catch (...) {
            qWarning() << "Failed to read P_matrix, using material C as approximation.";
        }
    }

    if (!use_P_from_file)
    {
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j)
                P_for_chol[i][j] = C[i][j];
        qDebug() << "No previous P_matrix found. Using material C as approximation for first run.";
    }

    // 3. Розклад Холецького P = L * L^T
    double L[6][6] = {0};
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j <= i; j++) {
            double sum = 0;
            for (int k = 0; k < j; k++) sum += L[i][k] * L[j][k];
            if (i == j) L[i][j] = std::sqrt(P_for_chol[i][i] - sum);
            else L[i][j] = (1.0 / L[j][j] * (P_for_chol[i][j] - sum));
        }
    }

    // 4. Інверсія L -> Linv (трансформатор: сфера -> еліпсоїд P)
    double Linv[6][6] = {0};
    for (int i = 0; i < 6; ++i) {
        Linv[i][i] = 1.0 / L[i][i];
        for (int j = 0; j < i; ++j) {
            double sum = 0;
            for (int k = j; k < i; ++k) sum += L[i][k] * Linv[k][j];
            Linv[i][j] = -sum / L[i][i];
        }
    }

    // 5. Генерація 300 навантажень — рівномірно по поверхні еліпсоїда P
    std::mt19937 gen(Parameters::seed);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int n = 0; n < 300; ++n) {
        // Випадковий напрямок на одиничній сфері в R^6
        double d[6], norm = 0;
        for (int i = 0; i < 6; ++i) { d[i] = dist(gen); norm += d[i]*d[i]; }
        norm = std::sqrt(norm);

        // Фіксована амплітуда — зондуємо напрямки по поверхні еліпсоїда
        const double r = strain_val;

        // eps = Linv^T * (d/norm) * r  — перехід зі сфери в еліпсоїд P
        double tmp[6] = {0};
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j <= i; ++j)
                tmp[i] += Linv[i][j] * (d[j] / norm) * r;

        // Шаг 2: eps = Linv^T * tmp
        std::vector<double> eps(6, 0.0);
        for (int i = 0; i < 6; ++i)
            for (int j = i; j < 6; ++j)
                eps[i] += Linv[j][i] * tmp[j];
        load_cases.push_back(eps);
    }

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

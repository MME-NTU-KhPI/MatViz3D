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
    wr->setAnisoMaterial(c11, c12, c12, c11, c12, c11, c44, c44, c44) ;
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

    for (int i = 0; i < 6; ++i) {
        std::vector<double> load(6, 0.0);
        load[i] = strain_val;
        load_cases.push_back(load);
    }

    for (int i = 0; i < 6; ++i) {
        for (int j = i + 1; j < 6; ++j) {
            std::vector<double> load(6, 0.0);
            load[i] = strain_val;
            load[j] = strain_val;
            load_cases.push_back(load);
        }
    }

    /*const double strain_eq = 1e-04;
    const int N_theta = 40;
    const int N_hydro = 10;
    const double hydro_min = -1e-04;
    const double hydro_max =  1e-04;

    std::vector<double> theta_list;
    for (int i = 0; i < N_theta; ++i)
        theta_list.push_back(i * 2.0 * M_PI / N_theta);

    std::vector<double> hydro_list;
    if (N_hydro > 1) {
        for (int i = 0; i < N_hydro; ++i)
            hydro_list.push_back(hydro_min + i * (hydro_max - hydro_min) / (N_hydro - 1));
    } else {
        hydro_list.push_back(0.0);
    }

    for (double e_hydro : hydro_list)
    {
        for (double theta : theta_list)
        {
            double e_dev1 = strain_eq * std::cos(theta);
            double e_dev2 = strain_eq * std::cos(theta - 2.0 * M_PI / 3.0);
            double e_dev3 = strain_eq * std::cos(theta + 2.0 * M_PI / 3.0);

            std::vector<double> load = {
                e_dev1 + e_hydro, // exx
                e_dev2 + e_hydro, // eyy
                e_dev3 + e_hydro, // ezz
                0.0,              // exy
                0.0,              // exz
                0.0               // eyz
            };
            load_cases.push_back(load);
        }
    }*/

    if (Parameters::num_rnd_loads > 0)
    {
        std::mt19937 gen(Parameters::seed);
        std::uniform_real_distribution<double> dis(0.0, strain_val);

        for (int i = 0; i < Parameters::num_rnd_loads; ++i)
        {
            std::vector<double> load(6);
            for (int k = 0; k < 6; ++k) load[k] = dis(gen);
            load_cases.push_back(load);
        }
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
    }
    LoadStepManager::getInstance().LoadFromHDF5(filename);
    wr->clear_temp_data();
    delete wr;
}

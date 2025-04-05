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
    const float min_val = -1e-04;
    const float max_val =  1e-04;
    const int grid_steps = 2; // -1 ; 1
    const int n_total_steps = Parameters::num_rnd_loads + pow(grid_steps, 6);
    const int n_comp_steps = grid_steps; // six components of tensor
    const float step = (max_val - min_val) / (n_comp_steps - 1);

    for (int ix = 0; ix < n_comp_steps; ix++)
      for (int iy = 0; iy < n_comp_steps; iy++)
        for (int iz = 0; iz < n_comp_steps; iz++)
          for (int ixy = 0; ixy < n_comp_steps; ixy++)
            for (int ixz = 0; ixz < n_comp_steps; ixz++)
              for (int iyz = 0; iyz < n_comp_steps; iyz++)
              {
                    auto f = [=](int i) { return min_val + i * step; };
                   //wr->addStrainToBCMacro(f(ix), f(iy), f(iz),
                   //                        f(ixy), f(ixz), f(iyz), numCubes);
                    wr->applyComplexLoads(0, 0, 0, N, N, N,
                                          f(ix), f(iy), f(iz),
                                          f(ixy), f(ixz), f(iyz));
              }

    std::mt19937 gen(Parameters::seed); // use global seed
    std::uniform_real_distribution<double> dis(min_val, max_val);
    for (int rnd_step = wr->eps_as_loading.size(); rnd_step < n_total_steps; rnd_step++)
    {
        wr->applyComplexLoads(0, 0, 0, N, N, N,
                              dis(gen), dis(gen), dis(gen),
                              dis(gen), dis(gen), dis(gen));
        //wr->addStrainToBCMacro(dis(gen), dis(gen), dis(gen),
        //                       dis(gen), dis(gen), dis(gen), numCubes);
    }
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

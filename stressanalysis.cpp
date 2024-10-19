#include "stressanalysis.h"
#include "ansysWrapper.h"
#include "hdf5wrapper.h"
#include "parameters.h"
#include <random>

void StressAnalysis::estimateStressWithANSYS(short int numCubes, short int numPoints, int32_t ***voxels)
{
    const auto N = numCubes;
    wr = new ansysWrapper(true);
    if (! Parameters::working_directory.isEmpty())
        wr->setWorkingDirectory(Parameters::working_directory);

    wr->setSeed(Parameters::seed);
    //wr->setNP(1);
    //wr.setMaterial(2.1e11, 0.3, 0);

    double c11 = 168.40e9, c12=121.40e9, c44=75.40e9; // copper bcc single crystal  https://solidmechanics.org/Text/Chapter3_2/Chapter3_2.php#Sect3_2_17
    wr->setAnisoMaterial(c11, c12, c12, c11, c12, c11, c44, c44, c44) ;

    wr->setElemByNum(186);

    wr->createFEfromArray(voxels, N, numPoints, true);

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
    }
    wr->solveLS(1, n_total_steps);
    wr->saveAll();
    wr->run();

    HDF5Wrapper hdf5(Parameters::filename.toStdString());

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
    for (int ls_num = 1; ls_num <= n_total_steps; ls_num++)
    {
        std::string ls_str = "/ls_" + std::to_string(ls_num);
        wr->load_loadstep(ls_num);

        hdf5.write(prefix + ls_str, "results_avg", wr->loadstep_results_avg);
        hdf5.write(prefix + ls_str, "results_max", wr->loadstep_results_max);
        hdf5.write(prefix + ls_str, "results_min", wr->loadstep_results_min);
        hdf5.write(prefix + ls_str, "results", wr->loadstep_results);
        hdf5.write(prefix + ls_str, "eps_as_loading", wr->eps_as_loading[ls_num]);
    }

    wr->clear_temp_data();
    //delete wr;
}

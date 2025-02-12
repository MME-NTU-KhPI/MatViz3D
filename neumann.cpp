#include <windows.h>
#include <ctime>
#include <cmath>
#include <myglwidget.h>
#include "neumann.h"

Neumann::Neumann()
{

}

Neumann::Neumann(short int numCubes, int numColors)
{
    this->numCubes = numCubes;
    this->numColors = numColors;
}

const std::array<std::array<int32_t, 3>, 6> NEUMANN_OFFSETS = {{
    {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
}};


void Neumann::Next_Iteration()
{
    unsigned int counter_max = pow(numCubes, 3);
    const size_t current_size = grains.size();
    std::vector<std::vector<Coordinate>> threadGrains(omp_get_max_threads());

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        std::vector<Coordinate>& privateGrains = threadGrains[thread_id];
        privateGrains.reserve(current_size * 6 / omp_get_max_threads());

        #pragma omp for schedule(guided)
        for (size_t i = 0; i < grains.size(); i++)
        {
            Coordinate temp = grains[i];
            int32_t x = temp.x, y = temp.y, z = temp.z;

            for (const auto& offset : NEUMANN_OFFSETS)
            {
                int32_t newX = x + offset[0];
                int32_t newY = y + offset[1];
                int32_t newZ = z + offset[2];

                if (flags.isPeriodicStructure == 1)
                {
                    newX = (newX + numCubes) % numCubes;
                    newY = (newY + numCubes) % numCubes;
                    newZ = (newZ + numCubes) % numCubes;
                }

                if (newX >= 0 && newX < numCubes &&
                    __sync_bool_compare_and_swap(&voxels[newX][y][z], 0, voxels[x][y][z]))
                {
                    privateGrains.push_back({newX, y, z});
                }
                if (newY >= 0 && newY < numCubes &&
                    __sync_bool_compare_and_swap(&voxels[x][newY][z], 0, voxels[x][y][z]))
                {
                    privateGrains.push_back({x, newY, z});
                }
                if (newZ >= 0 && newZ < numCubes &&
                    __sync_bool_compare_and_swap(&voxels[x][y][newZ], 0, voxels[x][y][z]))
                {
                    privateGrains.push_back({x, y, newZ});
                }
            }
        }
    }

    grains.clear();
    for (auto& tg : threadGrains)
    {
        grains.insert(grains.end(), tg.begin(), tg.end());
    }

    filled_voxels += grains.size();
    IterationNumber++;
    double o = (double)filled_voxels / counter_max;
    qDebug().nospace() << o << "\t" << IterationNumber << "\t" << grains.size();

    if (flags.isAnimation)
    {
        if (flags.isWaveGeneration && remainingPoints > 0)
        {
            pointsForThisStep = std::max(1, static_cast<int>(0.1 * remainingPoints));
            std::vector<Coordinate> newPoints = Add_New_Points(grains, pointsForThisStep);
            grains.insert(grains.end(), newPoints.begin(), newPoints.end());
            remainingPoints -= pointsForThisStep;
        }
        return;
    }
}

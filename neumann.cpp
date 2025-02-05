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


void Neumann::Generate_Filling()
{
    unsigned int counter_max = pow(numCubes, 3);
    const size_t current_size = grains.size();
    std::vector<Coordinate> newGrains;
    newGrains.reserve(current_size * 6);
    #pragma omp parallel
    {
        std::vector<Coordinate> privateGrains;
        privateGrains.reserve(current_size * 6 / omp_get_max_threads());
        #pragma omp for schedule(guided) nowait
        for (size_t i = 0; i < grains.size(); i++)
        {
            Coordinate temp = grains[i];
            int32_t x = temp.x, y = temp.y, z = temp.z;

            #pragma omp simd
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

                if (!(newX >= 0 && newX < numCubes)) continue;
                if (__sync_bool_compare_and_swap(&voxels[newX][y][z], 0, voxels[x][y][z]))
                {
                    privateGrains.push_back({newX, y, z});
                    #pragma omp atomic
                    filled_voxels++;
                }
                if (!(newY >= 0 && newY < numCubes)) continue;
                if (__sync_bool_compare_and_swap(&voxels[x][newY][z], 0, voxels[x][y][z]))
                {
                    privateGrains.push_back({x, newY, z});
                    #pragma omp atomic
                    filled_voxels++;
                }

                if (!(newZ >= 0 && newZ < numCubes)) continue;
                if (__sync_bool_compare_and_swap(&voxels[x][y][newZ], 0, voxels[x][y][z]))
                {
                    privateGrains.push_back({x, y, newZ});
                    #pragma omp atomic
                    filled_voxels++;
                }
            }

        }
        #pragma omp critical
        newGrains.insert(newGrains.end(), privateGrains.begin(), privateGrains.end());
    }
    counter += filled_voxels;
    grains = std::move(newGrains);
    IterationNumber++;
    double o = (double)counter / counter_max;
    qDebug().nospace() << o << "\t" << IterationNumber << "\t" << grains.size();
    if (flags.isAnimation)
    {
        if (flags.isWaveGeneration && remainingPoints > 0)
        {
            pointsForThisStep = std::max(1, static_cast<int>(0.1 * remainingPoints));
            newGrains = Add_New_Points(newGrains, pointsForThisStep);
            grains.insert(grains.end(), newGrains.begin(), newGrains.end());
            remainingPoints -= pointsForThisStep;
        }
        return;
    }
}

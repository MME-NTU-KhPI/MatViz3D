#include <ctime>
#include <cmath>
#include <QDebug>
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


void Neumann::Next_Iteration(std::function<void()> callback)
{
    unsigned int counter_max = pow(numCubes, 3);
    while (!grains.empty())
    {
        const size_t current_size = grains.size();
        std::vector<Coordinate> newGrains;
        newGrains.reserve(current_size * 6);
        unsigned int local_counter = 0;
        #pragma omp parallel reduction(+:local_counter)
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
                        local_counter++;
                    }
                    if (!(newY >= 0 && newY < numCubes)) continue;
                    if (__sync_bool_compare_and_swap(&voxels[x][newY][z], 0, voxels[x][y][z]))
                    {
                        privateGrains.push_back({x, newY, z});
                        local_counter++;
                    }

                    if (!(newZ >= 0 && newZ < numCubes)) continue;
                    if (__sync_bool_compare_and_swap(&voxels[x][y][newZ], 0, voxels[x][y][z]))
                    {
                        privateGrains.push_back({x, y, newZ});
                        local_counter++;
                    }
                }

            }
            #pragma omp critical
            newGrains.insert(newGrains.end(), privateGrains.begin(), privateGrains.end());
        }
        filled_voxels += local_counter;
        grains = std::move(newGrains);
        IterationNumber++;
        double o = (double)filled_voxels / counter_max;
        qDebug().nospace() << o << "\t" << IterationNumber << "\t" << grains.size();
        if (flags.isAnimation)
        {
            if (flags.isWaveGeneration && remainingPoints > 0)
            {
                pointsForThisStep = std::max(1, static_cast<int>(Parameters::wave_coefficient * remainingPoints));
                newGrains = Add_New_Points(newGrains, pointsForThisStep);
                grains.insert(grains.end(), newGrains.begin(), newGrains.end());
                remainingPoints -= pointsForThisStep;
            }
            callback();
        }
    }

}

#include "radial.h"
#include <windows.h>
#include <ctime>
#include <cmath>
#include "QDebug"
#include <omp.h>
#include <fstream>

Radial::Radial()
{

}

Radial::Radial(short int numCubes, int numColors)
{
    this->numCubes = numCubes;
    this->numColors = numColors;
}

const std::array<std::array<int32_t, 3>, 18> RADIAL_OFFSETS = {
    {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0},
     {0, 1, 0}, {0, 0, -1}, {0, 0, 1},
     {-1, -1, 0}, {-1, 1, 0}, {1, -1, 0},
     {1, 1, 0},{-1, 0, -1}, {-1, 0, 1},
     {1, 0, -1}, {1, 0, 1},{0, -1, -1},
     {0, -1, 1}, {0, 1, -1}, {0, 1, 1}
}};

void Radial::Generate_Filling(int isAnimation, int isWaveGeneration)
{
    omp_set_num_threads(omp_get_max_threads());
    const unsigned int counter_max = pow(numCubes, 3);
    auto start = std::chrono::high_resolution_clock::now();

    while (!grains.empty())
    {
        const size_t current_size = grains.size();
        std::vector<Coordinate> newGrains;
        newGrains.reserve(current_size * 18);

        unsigned int local_counter = 0;
        #pragma omp parallel reduction(+:local_counter)
        {
            std::vector<Coordinate> privateGrains;
            privateGrains.reserve(current_size * 18 / omp_get_max_threads());

            #pragma omp for schedule(guided) nowait
            for (size_t i = 0; i < current_size; i++)
            {
                const Coordinate& temp = grains[i];
                const int32_t x = temp.x, y = temp.y, z = temp.z;
                const int32_t current_value = voxels[x][y][z];

                #pragma omp simd
                for (const auto& offset : RADIAL_OFFSETS)
                {
                    const int32_t newX = x + offset[0];
                    const int32_t newY = y + offset[1];
                    const int32_t newZ = z + offset[2];

                    if (newX >= 0 && newX < numCubes &&
                        newY >= 0 && newY < numCubes &&
                        newZ >= 0 && newZ < numCubes)
                    {
                        if (__sync_bool_compare_and_swap(&voxels[newX][newY][newZ], 0, current_value))
                        {
                            privateGrains.push_back({newX, newY, newZ});
                            local_counter++;
                        }
                    }
                }
            }

            #pragma omp critical
            {
                newGrains.insert(newGrains.end(),
                               std::make_move_iterator(privateGrains.begin()),
                               std::make_move_iterator(privateGrains.end()));
            }
        }

        counter += local_counter;
        grains = std::move(newGrains);
        IterationNumber++;

        double o = static_cast<double>(counter) / counter_max;
        qDebug().nospace() << o << "\t" << IterationNumber << "\t" << grains.size();

        if (isAnimation == 1)
        {
            if (isWaveGeneration == 1 && remainingPoints > 0)
            {
                pointsForThisStep = std::max(1, static_cast<int>(0.1 * remainingPoints));
                newGrains = Add_New_Points(newGrains, pointsForThisStep);
                grains.insert(grains.end(), newGrains.begin(), newGrains.end());
                remainingPoints -= pointsForThisStep;
            }
            break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    qDebug() << "Algorithm execution time: " << duration.count() << " seconds";
}

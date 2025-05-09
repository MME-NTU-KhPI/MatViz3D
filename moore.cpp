#include <ctime>
#include <cmath>
#include "myglwidget.h"
#include "parent_algorithm.h"
#include "moore.h"


Moore::Moore()
{

}

Moore::Moore(short int numCubes, int numColors)
{
    this->numCubes = numCubes;
    this->numColors = numColors;
}

const std::array<std::array<int32_t, 3>, 26> MOORE_OFFSETS = {{
    {-1, -1, -1}, {-1, -1, 0}, {-1, -1, 1},
    {-1, 0, -1},  {-1, 0, 0},  {-1, 0, 1},
    {-1, 1, -1},  {-1, 1, 0},  {-1, 1, 1},
    {0, -1, -1},  {0, -1, 0},  {0, -1, 1},
    {0, 0, -1},                {0, 0, 1},
    {0, 1, -1},   {0, 1, 0},   {0, 1, 1},
    {1, -1, -1},  {1, -1, 0},  {1, -1, 1},
    {1, 0, -1},   {1, 0, 0},   {1, 0, 1},
    {1, 1, -1},   {1, 1, 0},   {1, 1, 1}
}};

void Moore::Next_Iteration(std::function<void()> callback)
{
    const unsigned int counter_max = pow(numCubes, 3);

    while (!grains.empty())
    {
        const size_t current_size = grains.size();
        std::vector<Coordinate> newGrains;
        newGrains.reserve(current_size * 26);
        unsigned int local_counter = 0;
        #pragma omp parallel reduction(+:local_counter)
        {
            std::vector<Coordinate> privateGrains;
            privateGrains.reserve(current_size * 26 / omp_get_max_threads());

            #pragma omp for schedule(guided) nowait
            for (size_t i = 0; i < current_size; i++)
            {
                const Coordinate& temp = grains[i];
                const int32_t x = temp.x, y = temp.y, z = temp.z;
                const int32_t current_value = voxels[x][y][z];

                #pragma omp simd
                for (const auto& offset : MOORE_OFFSETS)
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
        filled_voxels += local_counter;
        grains = std::move(newGrains);
        IterationNumber++;
        double o = static_cast<double>(filled_voxels) / counter_max;
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

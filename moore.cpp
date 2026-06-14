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

    const int N_gr = numColors;
    int total_nucleated_so_far = static_cast<int>(grains.size());

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

        QString nuclLogInfo = "";

        if (flags.isWaveGeneration && total_nucleated_so_far < N_gr && filled_voxels < counter_max)
        {
            double cumulative_fraction = 0.0;
            int wave_contribution = 0;

            if (IterationNumber > 1)
            {
                double arg = (IterationNumber - (Parameters::wave_coefficient / 2.0)) / (Parameters::wave_spread * std::sqrt(2.0));
                cumulative_fraction = 0.5 * (1.0 + std::erf(arg));

                int remaining_to_nucleate = N_gr - Parameters::initial_nuclei_count;

                wave_contribution = static_cast<int>(std::floor(cumulative_fraction * remaining_to_nucleate));
            }

            int total_should_be_now = Parameters::initial_nuclei_count + wave_contribution;

            int pointsToCreate = total_should_be_now - total_nucleated_so_far;

            if (pointsToCreate > 0)
            {
                int placedRandomly = 0;

                for (int p = 0; p < pointsToCreate; ++p) {
                    bool success = false;
                    for (int retry = 0; retry < 10; ++retry) {
                        int rx = std::rand() % numCubes;
                        int ry = std::rand() % numCubes;
                        int rz = std::rand() % numCubes;

                        if (voxels[rx][ry][rz] == 0) {
                            voxels[rx][ry][rz] = ++color;
                            grains.push_back({rx, ry, rz});
                            filled_voxels++;
                            success = true;
                            placedRandomly++;
                            break;
                        }
                    }
                    if (!success) break;
                }

                int leftToPlace = pointsToCreate - placedRandomly;

                if (leftToPlace > 0) {
                    grains = Add_New_Points(grains, leftToPlace);
                }

                total_nucleated_so_far += pointsToCreate;
            }
            nuclLogInfo = QString(" | [Nucl] N(n): %1 | Added: %2 | Tot: %3")
                              .arg(cumulative_fraction, -8, 'f', 4)
                              .arg(pointsToCreate, -6)
                              .arg(total_nucleated_so_far, -6);
        }

        double o = static_cast<double>(filled_voxels) / counter_max;
        QString logLine = QString("%1 %2 %3")
                              .arg(o, -12, 'g', 6)
                              .arg(IterationNumber, -6)
                              .arg((int)grains.size(), -10);

        if (!nuclLogInfo.isEmpty()) {
            logLine += nuclLogInfo;
        }

        qDebug().noquote() << logLine;

        if (flags.isAnimation)
        {
            callback();
        }
    }
}

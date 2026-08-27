#include "radial.h"
#include "algorithmplugin.h"
#include "parameters.h"
#include <ctime>
#include <cmath>
#include <QDebug>
#include <omp.h>
#include <array>

Radial::Radial()
{
}

Radial::Radial(short int numCubes, int numColors)
{
    this->numCubes = numCubes;
    this->numColors = numColors;
}

void Radial::Initialization(bool isWaveGeneration)
{
    Parent_Algorithm::Initialization(isWaveGeneration);
    qDebug().noquote()
        << QString("[Radial] %1^3 grid (%2 voxels), %3 seeds, 18-neighbor (Radial)%4")
               .arg(numCubes)
               .arg(static_cast<uint64_t>(numCubes) * numCubes * numCubes)
               .arg(seedPoints.size())
               .arg(flags.isPeriodicStructure ? ", periodic" : "");
}

const std::array<std::array<int32_t, 3>, 18> RADIAL_OFFSETS = {{
    {-1, 0, 0}, {1, 0, 0}, {0, -1, 0},
    {0, 1, 0}, {0, 0, -1}, {0, 0, 1},
    {-1, -1, 0}, {-1, 1, 0}, {1, -1, 0},
    {1, 1, 0}, {-1, 0, -1}, {-1, 0, 1},
    {1, 0, -1}, {1, 0, 1}, {0, -1, -1},
    {0, -1, 1}, {0, 1, -1}, {0, 1, 1}
}};

void Radial::Next_Iteration()
{
    if (getDone()) return;

    const unsigned int counter_max = pow(numCubes, 3);

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

    const double fraction = (counter_max > 0) ? (static_cast<double>(filled_voxels) / counter_max) : 1.0;
    const double pct = fraction * 100.0;

    if (getDone()) {
        qDebug().noquote()
            << QString("[Radial] Step %1: filled %2/%3 (%4%) | growth complete")
                   .arg(IterationNumber, 2)
                   .arg(filled_voxels)
                   .arg(counter_max)
                   .arg(pct, 5, 'f', 1);
    } else {
        qDebug().noquote()
            << QString("[Radial] Step %1: filled %2/%3 (%4%) | active front: %5 voxels")
                   .arg(IterationNumber, 2)
                   .arg(filled_voxels)
                   .arg(counter_max)
                   .arg(pct, 5, 'f', 1)
                   .arg(grains.size());
    }
}

bool Radial::getDone() const
{
    return grains.empty() || Parent_Algorithm::getDone();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Plugin registration
// ─────────────────────────────────────────────────────────────────────────────

MATVIZ_REGISTER_ALGORITHM(AlgorithmPlugin{
    "Radial",
    "Cellular automaton grain growth with 18-neighbor neighborhood, "
    "with material and texture selection.",
    /*order=*/ 4,
    standardGrainGrowthSchema(),
    [](const Parameters& p) {
        return std::make_shared<Radial>(static_cast<short int>(p.getSize()), p.getPoints());
    }
});

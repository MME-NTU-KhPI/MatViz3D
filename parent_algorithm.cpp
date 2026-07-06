#include <ctime>
#include <cmath>
#include <assert.h>
#include <fstream>
#include "parameters.h"
#include "parent_algorithm.h"
#include <cstdint>

Parent_Algorithm::Parent_Algorithm()
{

}

Parent_Algorithm::~Parent_Algorithm()
{

}
/*
 * The array is dynamic but continuous so it's a huge plus over the vector<> approach and loops of new[] calls.
 * https://stackoverflow.com/questions/8027958/c-3d-array-dynamic-memory-allocation-aligned-in-one-line
 */

template <class T> T ***Parent_Algorithm::Create3D(int N1, int N2, int N3)
{
    T *** array = new T ** [N1];
    array[0] = new T * [N1 * N2];
    array[0][0] = new T [N1 * N2 * N3];

    for(int i = 0; i < N1; i++)
    {
        if(i < N1 - 1)
        {
            array[0][(i + 1)*N2] = &(array[0][0][(i + 1) * N3 * N2]);
            array[i + 1] = &(array[0][(i + 1) * N2]);
        }

        for(int j = 0; j < N2; j++)
        {
            if(j > 0)
                array[i][j] = array[i][j - 1] + N3;
        }
    }
    return array;
};

int32_t*** Parent_Algorithm::Allocate_Memory()
{
    voxels = this->Create3D<int32_t>(numCubes, numCubes, numCubes); // create continТius allocated 3d dynamic array

    assert(voxels); // the new operator should throw an exeption, but we will check again

    #pragma omp parallel for collapse(3)
    for(int k = 0; k < numCubes; k++)
        for(int i = 0; i < numCubes; i++)
            for(int j = 0; j < numCubes; j++)
                voxels[k][i][j] = 0;

    return voxels;
};

template <class T> void Parent_Algorithm::Delete3D(T*** array)
{
    delete[] array[0][0];
    delete[] array[0];
    delete[] array;
};

// Explicit instantiation of Delete3D for int
template void Parent_Algorithm::Delete3D<int32_t>(int32_t***);

Parent_Algorithm::Coordinate Parent_Algorithm::randomCoord()
{
    std::uniform_int_distribution<int> dist(0, numCubes - 1);
    Coordinate a;
    a.x = dist(m_rng);
    a.y = dist(m_rng);
    a.z = dist(m_rng);
    return a;
}

int32_t Parent_Algorithm::birthGrain(int x, int y, int z)
{
    voxels[x][y][z] = ++color;
    grains.push_back({x, y, z});
    seedPoints.push_back({x, y, z});
    filled_voxels++;
    return color;
}

void Parent_Algorithm::CleanUp()
{
    if (voxels) {
        filled_voxels = 0;
        total_nucleated_so_far = -1;
        color = 0;
        grains.clear();
        seedPoints.clear();
        flags.isDone = false;
    }
}

void Parent_Algorithm::Random_Generate_Points(int currentPoints)
{
    for (int i = 0; i < currentPoints; i++)
    {
        Coordinate a = randomCoord();
        if (voxels[a.x][a.y][a.z] == 0)
            birthGrain(a.x, a.y, a.z);
    }
}

void Parent_Algorithm::Grid_Generate_Points(int totalPoints)
{
    const int N = std::max(1, static_cast<int>(std::cbrt(static_cast<double>(totalPoints))));
    const int num_points_to_generate = N * N * N;

    std::vector<int> available_colors(totalPoints);
    std::iota(available_colors.begin(), available_colors.end(), 1);

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::shuffle(available_colors.begin(), available_colors.end(), gen);

    const double M_minus_1 = static_cast<double>(numCubes - 1);
    const double N_minus_1 = (N > 1) ? static_cast<double>(N - 1) : 1.0;

    const double scale_factor = M_minus_1 / N_minus_1;
    const int center_cube = (numCubes - 1) / 2;
    const bool should_scale = (N > 1);
    const double offset = 0.5;

    int color_index = 0;

    for (int iz = 0; iz < N; ++iz)
    {
        for (int iy = 0; iy < N; ++iy)
        {
            for (int ix = 0; ix < N; ++ix)
            {
                if (color_index >= num_points_to_generate) {
                    return;
                }
                double mapped_ix = ix + offset;
                double mapped_iy = iy + offset;
                double mapped_iz = iz + offset;

                int x = should_scale
                            ? static_cast<int>(std::round(mapped_ix * scale_factor - offset * scale_factor))
                            : center_cube;

                int y = should_scale
                            ? static_cast<int>(std::round(mapped_iy * scale_factor - offset * scale_factor))
                            : center_cube;

                int z = should_scale
                            ? static_cast<int>(std::round(mapped_iz * scale_factor - offset * scale_factor))
                            : center_cube;

                double normalized_ix = (ix + 0.5) / N;
                double normalized_iy = (iy + 0.5) / N;
                double normalized_iz = (iz + 0.5) / N;

                x = should_scale
                        ? static_cast<int>(std::round(normalized_ix * M_minus_1))
                        : center_cube;

                y = should_scale
                        ? static_cast<int>(std::round(normalized_iy * M_minus_1))
                        : center_cube;

                z = should_scale
                        ? static_cast<int>(std::round(normalized_iz * M_minus_1))
                        : center_cube;

                int assigned_color = available_colors[color_index];

                if (voxels[x][y][z] == 0)
                {
                    birthGrain(x, y, z);
                }

                color_index++;
            }
        }
    }
}

void Parent_Algorithm::Initialization(bool isWaveGeneration)
{
    m_rng.seed(Parameters::seed);

    int currentPoints;
    if (isWaveGeneration)
    {
        currentPoints = Parameters::initial_nuclei_count;
    }
    else
    {
        currentPoints = numColors;
    }

    remainingPoints = numColors - currentPoints;
    std::ofstream file("crystallization_seeds.csv");

    if (!file.is_open())
    {
        qCritical() << "Unable to open file: crystallization_seeds.csv";
    }
    file << "x,y,z,color\n";
    Random_Generate_Points(currentPoints);
    //Grid_Generate_Points(currentPoints);
}

std::vector<Parent_Algorithm::Coordinate> Parent_Algorithm::Add_New_Points(std::vector<Coordinate> grains, int numPoints)
{
    std::mt19937 generator(Parameters::seed);
    std::vector<Coordinate> emptyCoords;

    // 1. Побудова списку всіх порожніх координат
    for (int x = 0; x < numCubes; ++x) {
        for (int y = 0; y < numCubes; ++y) {
            for (int z = 0; z < numCubes; ++z) {
                if (voxels[x][y][z] == 0) {
                    emptyCoords.push_back({x, y, z});
                }
            }
        }
    }

    // 2. Перевірка
    if (numPoints > emptyCoords.size()) {
        qCritical() << "Not enough free voxels to place new points.";
        return grains; // або abort(), або false
    }

    // 3. Перемішування
    std::shuffle(emptyCoords.begin(), emptyCoords.end(), generator);

    // 4. Додавання точок
    for (int i = 0; i < numPoints; ++i) {
        Coordinate a = emptyCoords[i];
        birthGrain(a.x, a.y, a.z);
    }

    return grains;
}

std::vector<Parent_Algorithm::Coordinate> Parent_Algorithm::Delete_Points(std::vector<Coordinate> grains, size_t i)
{
    grains.erase(grains.begin() + i);
    i--;
    return grains;
}

void Parent_Algorithm::saveSeeds()
{
    std::ofstream file("crystallization_seeds.csv");
    if (!file.is_open()) { qCritical() << "Unable to open crystallization_seeds.csv"; return; }
    file << "x,y,z,color\n";
    for (const Coordinate& a : seedPoints)
        file << a.x << "," << a.y << "," << a.z << ","
             << voxels[a.x][a.y][a.z] << "\n";
    file.close();
}

void Parent_Algorithm::Generate_To_End()
{
    while (!this->getDone())
    {
        this->Next_Iteration();
    }
}

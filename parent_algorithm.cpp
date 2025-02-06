#include <ctime>
#include <cmath>
#include <myglwidget.h>
#include <assert.h>
#include <random>
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

int32_t*** Parent_Algorithm::Generate_Initial_Cube()
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

template <class T> void Parent_Algorithm::Delete3D(T ***array)
{
    delete[] array[0][0];
    delete[] array[0];
    delete[] array;
};

void Parent_Algorithm::CleanUp()
{
    if (voxels) {
        Delete3D(voxels);
        voxels = nullptr;
        filled_voxels = 0;
    }
}


void Parent_Algorithm::Generate_Random_Starting_Points(bool isWaveGeneration)
{
    std::random_device rd;
    std::mt19937 generator(Parameters::seed);
    std::uniform_int_distribution<int> distribution(0, numCubes - 1);
    int currentPoints;
    if(isWaveGeneration)
    {
        currentPoints = static_cast<int>(0.1 * numColors);
    }
    else
    {
        currentPoints = numColors;
    }
    Coordinate a;
    for(int i = 0; i < currentPoints; i++)
    {
        a.x = distribution(generator);
        a.y = distribution(generator);
        a.z = distribution(generator);
        voxels[a.x][a.y][a.z] = ++color;
        grains.push_back(a);
        filled_voxels++;
    }
}

std::vector<Coordinate> Parent_Algorithm::Add_New_Points(std::vector<Coordinate> grains, int numPoints)
{
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, numCubes - 1);

    Coordinate a;
    for(int i = 0; i < numPoints; i++)
    {
        a.x = distribution(generator);
        a.y = distribution(generator);
        a.z = distribution(generator);
        voxels[a.x][a.y][a.z] = ++color;
        grains.push_back(a);
        counter++;
    }
    return grains;
}

std::vector<Coordinate> Parent_Algorithm::Delete_Points(std::vector<Coordinate> grains, size_t i)
{
    grains.erase(grains.begin() + i);
    i--;
    return grains;
}

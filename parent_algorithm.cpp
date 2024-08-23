#include <iostream>
#include <windows.h>
#include <ctime>
#include <list>
#include <cmath>
#include <myglwidget.h>
#include <random>
#include "parameters.h"
#include "parent_algorithm.h"
#include <cstdint>
#include <new>

Parent_Algorithm::Parent_Algorithm()
{

}

int16_t*** Parent_Algorithm::Generate_Initial_Cube() {

    //Создаём динамический массив. Вместо (30) подставить numCubes
    voxels = new int16_t** [numCubes];
    assert(voxels);
    for (int i = 0; i < numCubes; i++) {
        voxels[i] = new int16_t* [numCubes];
        assert(voxels[i]);
        for (int j = 0; j < numCubes; j++) {
            voxels[i][j] = new int16_t[numCubes];
            assert(voxels[i][j]);
        }
    }

    for (int k = 0; k < numCubes; k++)
    {
        for (int i = 0; i < numCubes; i++)
        {
            for (int j = 0; j < numCubes; j++)
            {
                voxels[k][i][j] = 0;
            }
        }
    }
    return voxels;
}

void Parent_Algorithm::Generate_Random_Starting_Points(int isWaveGeneration)
{
    std::random_device rd;
    std::mt19937 generator(Parameters::seed);
    std::uniform_int_distribution<int> distribution(0, numCubes - 1);
    int currentPoints;
    if (isWaveGeneration == 1)
    {
        currentPoints = static_cast<int>(0.1 * numColors);
    }
    else
    {
        currentPoints = numColors;
    }
    Coordinate a;
    for (int i = 0; i < currentPoints; i++)
    {
        a.x = distribution(generator);
        a.y = distribution(generator);
        a.z = distribution(generator);
        voxels[a.x][a.y][a.z] = ++color;
        grains.push_back(a);
        counter++;
    }
}

std::vector<Parent_Algorithm::Coordinate> Parent_Algorithm::Add_New_Points(std::vector<Coordinate> grains, int numPoints)
{
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, numCubes - 1);

    Coordinate a;
    for (int i = 0; i < numPoints; i++)
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

std::vector<Parent_Algorithm::Coordinate> Parent_Algorithm::Delete_Points(std::vector<Coordinate> grains,size_t i)
{
    grains.erase(grains.begin() + i);
    i--;
    return grains;
}

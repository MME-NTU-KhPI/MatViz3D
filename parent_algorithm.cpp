#include <iostream>
#include <windows.h>
#include <ctime>
#include <list>
#include <cmath>
#include <myglwidget.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "parent_algorithm.h"
#include <cstdint>
#include <new>

Parent_Algorithm::Parent_Algorithm()
{

}

int16_t*** Parent_Algorithm::Generate_Initial_Cube(short int numCubes) {

    //Создаём динамический массив. Вместо (30) подставить numCubes
    int16_t*** voxels = new int16_t** [numCubes];
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
//    Generate_Filling(voxels, numCubes, myglwidget);
    return voxels;
}

std::vector<Parent_Algorithm::Coordinate> Parent_Algorithm::Generate_Random_Starting_Points(int16_t*** voxels,short int numCubes, int numColors)
{
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, numCubes - 1);

    Coordinate a;
    counter = 0;
    short int color = 0;
    std::vector<Coordinate> grains;
    for (int i = 0; i < numColors; i++)
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

bool Parent_Algorithm::Check(int16_t*** voxels,short int numCubes, bool answer,int n)
{
    if (n == 1)
    {
        answer = false;
        return answer;
    }
    int k = 0;
    for(;k < numCubes;k++)
    {
    for(int i = 0; i < numCubes; i++)
    {
        for(int j = 0; j < numCubes; j++)
        {
            for(int k = 0; k < numCubes; k++)
            {
                if (voxels[i][j][k] == 0)
                {
                    answer = true;
                    return answer;
                }
            }
        }
    }
    }
    if (k == numCubes)
    {
        answer = false;
    }
    return answer;
}

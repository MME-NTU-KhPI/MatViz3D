#include <iostream>
#include <windows.h>
#include <ctime>
#include <list>
#include <cmath>
#include <myglwidget.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "parent_algorithm.h"
#include "probability_circle.h"
#include <cstdint>
#include <new>

Parent_Algorithm::Parent_Algorithm()
{

}

int16_t*** Parent_Algorithm::Generate_Initial_Cube(short int numCubes, int numColors) {

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

    srand(time(NULL));
    short int color = 0;
    for (int i = 0; i < numColors; i++) {
        voxels[rand() % numCubes][rand() % numCubes][rand() % numCubes] = ++color;
    }

//    Generate_Filling(voxels, numCubes, myglwidget);
    return voxels;
}



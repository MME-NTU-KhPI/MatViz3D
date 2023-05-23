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
Parent_Algorithm::Parent_Algorithm()
{

}

//void Parent_Algorithm::Generate_Filling(voxels, numCubes);

uint16_t*** Parent_Algorithm::Generate_Initial_Cube(short int numCubes, int numColors) {

    //Создаём динамический массив. Вместо (30) подставить numCubes
    uint16_t*** voxels = new uint16_t** [numCubes];
    for (int i = 0; i < numCubes; i++) {
        voxels[i] = new uint16_t* [numCubes];
        for (int j = 0; j < numCubes; j++) {
            voxels[i][j] = new uint16_t[numCubes];
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

    //Заполняем массив начальными точками. Вмето (7) надо подставить значение пользователя
    //    srand(time(NULL));
    //    short int color = 0;
    //    for (int i = 0; i < 7; i++)
    //    {
    //        voxels[rand() % numCubes][rand() % numCubes][rand() % numCubes] = ++color;
    //    }

    srand(time(NULL));
    short int color = 0;
    for (int i = 0; i < numColors; i++) {
        voxels[rand() % numCubes][rand() % numCubes][rand() % numCubes] = ++color;
    }

    Generate_Filling(voxels, numCubes);
    return voxels;
}



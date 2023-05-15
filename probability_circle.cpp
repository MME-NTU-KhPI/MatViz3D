#include <iostream>
#include <windows.h>
#include <ctime>
#include <list>
#include <cmath>
#include <probability_circle.h>
#include <myglwidget.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"


using namespace std;
Probability_Circle::Probability_Circle()
{
    // Constructor implementation
    // Add any initialization code or logic here
}

//Проверяем массив на заполненность, если находим пустую ячейку, то вызываем Generate_Filling(voxels, numCubes);
void Probability_Circle::Check(uint16_t*** voxels, short int numCubes) {

    for (short int k = 0; k < numCubes; k++)
    {
        for (short int i = 0; i < numCubes; i++)
        {
            for (short int j = 0; j < numCubes; j++)
            {

                if (voxels[k][i][j] < 0)
                {
                    Generate_Filling(voxels, numCubes);
                }
            }
        }
    }

    for (int i = 0; i < numCubes; i++) {
        for (int j = 0; j < numCubes; j++) {
            delete[] voxels[i][j];
        }
        delete[] voxels[i];
    }
    delete[] voxels;

    return;
}

//Функция заполнения массива
void Probability_Circle::Generate_Filling(uint16_t*** voxels, short int numCubes) {


    srand(time(NULL));
    for (short int k = 0; k < numCubes; k++)
    {
        for (short int i = 0; i < numCubes; i++)
        {
            for (short int j = 0; j < numCubes; j++)
            {

                if (voxels[k][i][j] > 0)
                {
                    for (short int z = -1; z < 2; z++)
                    {
                        if ((k + z) < numCubes && (k + z) >= 0)
                        {
                            for (short int x = -1; x < 2; x++)
                            {
                                if ((i + x) < numCubes && (i + x) >= 0)
                                {
                                    for (short int y = -1; y < 2; y++)
                                    {
                                        if ((j + y) < numCubes && (j + y) >= 0)
                                        {
                                            if (x == 0 || y == 0)
                                            {
                                                if ((1 + rand() % 100) > 97)
                                                {
                                                    voxels[k + z][i + x][j + y] = voxels[k][i][j];
                                                }
                                            }
                                            else if ((1 + rand() % 100) > 53)
                                            {
                                                voxels[k + z][i + x][j + y] = voxels[k][i][j];
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
   // MyGLWidget glWidget;  // Create an instance of MyGLWidget

    //glWidget.paintGL(voxels, numCubes);
    Check(voxels, numCubes);
}

uint16_t*** Probability_Circle::Generate_Initial_Cube(short int numCubes) {

    srand(time(NULL));
    //Создаём динамический массив. Вместо (30) подставить numCubes
    uint16_t*** voxels = new uint16_t** [numCubes];
    for (int i = 0; i < numCubes; i++) {
        voxels[i] = new uint16_t* [numCubes];
        for (int j = 0; j < numCubes; j++) {
            voxels[i][j] = new uint16_t[numCubes];
        }
    }

    //Заполняем массив начальными точками. Вместо (7) надо подставить значение пользователя
    short int color = 0;
    for (int i = 0; i < 7; i++)
    {
        voxels[rand() % numCubes][rand() % numCubes][rand() % numCubes] = ++color;
    }

    Generate_Filling(voxels, numCubes);
    return voxels;
}




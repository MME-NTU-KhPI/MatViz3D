#include <iostream>
#include <windows.h>
#include <ctime>
#include <list>
#include <cmath>
#include "probability_circle.h"
#include <myglwidget.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "probability_ellipse.h"
#include "parent_algorithm.h"


using namespace std;
Probability_Circle::Probability_Circle():Parent_Algorithm()
{
    // Constructor implementation
    // Add any initialization code or logic here
}


//Функция заполнения массива
void Probability_Circle::Generate_Filling(int16_t*** voxels, short int numCubes) {

    bool answer = true;
    while (answer) {

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
                                                if ((x == 0 || y == 0) && voxels[k + z][i + x][j + y] == 0)
                                                {
                                                    if ((rand() % 100) < 97)
                                                    {
                                                        voxels[k + z][i + x][j + y] = -voxels[k][i][j];
                                                    }
                                                }
                                                else if (voxels[k + z][i + x][j + y] == 0)
                                                {
                                                    if ((rand() % 100) < 53)
                                                    {
                                                        voxels[k + z][i + x][j + y] = -voxels[k][i][j];
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
        }

        for (int k = 0; k < numCubes; k++)
        {
            for (int i = 0; i < numCubes; i++)
            {
                for (int j = 0; j < numCubes; j++)
                {
                    if (voxels[k][i][j] < 0)
                    {
                        voxels[k][i][j] = abs(voxels[k][i][j]);
                    }
                }
            }
        }

        int k = 0;
        for (; k < numCubes; k++)
        {
            int i = 0;
            for (; i < numCubes; i++)
            {
                int j = 0;
                for (; j < numCubes; j++)
                {
                    if (voxels[k][i][j] == 0)
                    {
                        answer = true;
                        break;
                    }
                }

                if (j < numCubes)
                {
                    break;
                }
            }

            if (i < numCubes)
            {
                break;
            }
        }

        if (k == numCubes)
        {
            answer = false;
        }
    }

    //Check(voxels, numCubes);
    //return voxels;
}



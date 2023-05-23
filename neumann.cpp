#include <iostream>
#include <windows.h>
#include <ctime>
#include <list>
#include <cmath>
#include <myglwidget.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "parent_algorithm.h"
#include "neumann.h"

using namespace std;

Neumann::Neumann()
{

}

void Neumann::Generate_Filling(uint16_t*** voxels, short int numCubes)
{
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
                                                if (voxels[k + z][i + x][j + y] == 0)
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
}

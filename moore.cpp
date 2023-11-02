#include <iostream>
#include <windows.h>
#include <ctime>
#include <list>
#include <cmath>
#include <myglwidget.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "parent_algorithm.h"
#include "moore.h"

using namespace std;

Moore::Moore()
{

}

bool Moore::Generate_Filling(int16_t*** voxels, short int numCubes,int n,std::vector<Coordinate> grains)
{
    bool answer = true;
    int IterationNumber = 0;
    while (answer)
    {
        Coordinate temp;
        int16_t x,y,z;
        std::vector<Coordinate> newGrains;
        int counter_max = pow(numCubes,3);
        for(size_t i = 0; i < grains.size(); i++)
        {
            temp = grains[i];
            x = temp.x;
            y = temp.y;
            z = temp.z;
            for (int16_t k = -1; k < 2; k++)
            {
                for(int16_t p = -1; p < 2; p++)
                {
                    for(int16_t l = -1; l < 2; l++)
                    {
                        int16_t newX = k+x;
                        int16_t newY = p+y;
                        int16_t newZ = l+z;
                        bool isValidXYZ = (newX >= 0 && newX < numCubes) && (newY >= 0 && newY < numCubes) && (newZ >= 0 && newZ < numCubes) && voxels[newX][newY][newZ] == 0;
                        if (isValidXYZ)
                        {
                            voxels[newX][newY][newZ] = -voxels[x][y][z];
                            newGrains.push_back({newX,newY,newZ});
                        }
                    }
                }
            }
        }
        grains.clear();
        grains.insert(grains.end(), newGrains.begin(), newGrains.end());
        for (int k = 0; k < numCubes; k++)
        {
            for (int i = 0; i < numCubes; i++)
            {
                for (int j = 0; j < numCubes; j++)
                {
                    if (voxels[k][i][j] < 0)
                    {
                        voxels[k][i][j] = abs(voxels[k][i][j]);
                        counter++;
                    }
                }
            }
        }
        // Перевірка, чи є порожні місця в масиві voxels
        if (counter < counter_max)
            answer = true;
        else
            answer = false;
        // Перевірка, чи потрібна анімація
        if (n == 1)
        {
            break;
        }
        if (n > 1)
        {
            n--;
        }
        IterationNumber++;
        qDebug()<<IterationNumber<<grains.size()<<counter<<counter_max;
    }
    return answer;
}

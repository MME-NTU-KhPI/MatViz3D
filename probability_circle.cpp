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
std::vector<Parent_Algorithm::Coordinate> Probability_Circle::Generate_Filling(int16_t*** voxels, short int numCubes,int n, std::vector<Coordinate> grains)
{
    srand(time(NULL));
    unsigned int counter_max = pow(numCubes,3);
    while (!grains.empty())
    {
        Coordinate temp;
        int16_t x,y,z;
        std::vector<Coordinate> newGrains;
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
                        bool Chance97 = (rand() % 100) < 97;
                        bool Chance53 = (rand() % 100) < 53;
                        if (isValidXYZ)
                        {
                            if(p == 0 || l == 0)
                            {
                                if (Chance97)
                                {
                                    voxels[newX][newY][newZ] = voxels[x][y][z];
                                    newGrains.push_back({newX,newY,newZ});
                                    counter++;
                                }
                                else
                                {
                                    newGrains.push_back({x,y,z});
                                }
                            }
                            else
                            {
                                if(Chance53)
                                {
                                    voxels[newX][newY][newZ] = voxels[x][y][z];
                                    newGrains.push_back({newX,newY,newZ});
                                    counter++;
                                }
                                else
                                {
                                    newGrains.push_back({x,y,z});
                                }
                            }
                        }
                    }
                }
            }
        }
        grains.clear();
        grains.insert(grains.end(), newGrains.begin(), newGrains.end());
        IterationNumber++;
        double o = (double)counter/counter_max;
        qDebug().nospace() << o << "\t" << IterationNumber << "\t" << grains.size();
        if (n == 1)
        {
            break;
        }
    }
    return grains;
}
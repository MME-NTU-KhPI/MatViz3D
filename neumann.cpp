#include <iostream>
#include <windows.h>
#include <ctime>
#include <list>
#include <cmath>
#include <myglwidget.h>
#include "neumann.h"

using namespace std;

Neumann::Neumann()
{

}


std::vector<Parent_Algorithm::Coordinate> Neumann::Generate_Filling(int16_t*** voxels, short int numCubes, int n, std::vector<Coordinate> grains)
{
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
            for (int16_t k = -1; k < 2; k+=2)
            {
                int16_t newX = k+x;
                int16_t newY = k+y;
                int16_t newZ = k+z;
                bool isValidX = (newX >= 0 && newX < numCubes) && voxels[newX][y][z] == 0;
                bool isValidY = (newY >= 0 && newY < numCubes) && voxels[x][newY][z] == 0;
                bool isValidZ = (newZ >= 0 && newZ < numCubes) && voxels[x][y][newZ] == 0;
                if (isValidX)
                {
                    voxels[newX][y][z] = voxels[x][y][z];
                    newGrains.push_back({newX,y,z});
                    counter++;
                }
                if (isValidY)
                {
                    voxels[x][newY][z] = voxels[x][y][z];
                    newGrains.push_back({x,newY,z});
                    counter++;
                }
                if (isValidZ)
                {
                    voxels[x][y][newZ] = voxels[x][y][z];
                    newGrains.push_back({x,y,newZ});
                    counter++;
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

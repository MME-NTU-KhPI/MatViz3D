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


bool Neumann::Generate_Filling(int16_t*** voxels, short int numCubes,int n, std::vector<Coordinate> grains)
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
                    voxels[newX][y][z] = -voxels[x][y][z];
                    newGrains.push_back({newX,y,z});
                }
                if (isValidY)
                {
                    voxels[x][newY][z] = -voxels[x][y][z];
                    newGrains.push_back({x,newY,z});
                }
                if (isValidZ)
                {
                    voxels[x][y][newZ] = -voxels[x][y][z];
                    newGrains.push_back({x,y,newZ});
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
        if (counter < counter_max)
            answer = true;
        else
            answer = false;
        if (n == 1)
        {
            answer=false;
        }
        if (n > 1)
        {
            n--;
        }
        IterationNumber++;
        double o = (double)counter/counter_max;
        qDebug().nospace() << o << "\t" << IterationNumber << "\t" << grains.size();

    }
    return answer;
}

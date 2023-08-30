#include <iostream>
#include <windows.h>
#include <ctime>
#include <list>
#include <cmath>
#include <myglwidget.h>
#include "moore.h"

using namespace std;

Moore::Moore()
{

}

bool Moore::Generate_Filling(int16_t*** voxels, short int numCubes,int n, std::vector<int16_t> grains)
{
    bool answer = true;
    while (answer)
    {
        for (size_t gr = 0; gr < grains.size(); gr += 3)
        {
            for (int x = -1; x < 2; x++)
            {
                for (int y = -1; y < 2; y++)
                {
                    for (int z = -1; z < 2; z++)
                    {
                        int newX = grains[gr] + x;
                        int newY = grains[gr + 1] + y;
                        int newZ = grains[gr + 2] + z;

                        if (newX < 0) newX = numCubes - 1;
                        if (newX >= numCubes) newX = 0;
                        if (newY < 0) newY = numCubes - 1;
                        if (newY >= numCubes) newY = 0;
                        if (newZ < 0) newZ = numCubes - 1;
                        if (newZ >= numCubes) newZ = 0;

                        if (voxels[newX][newY][newZ] == 0)
                        {
                            voxels[newX][newY][newZ] = -voxels[grains[gr]][grains[gr + 1]][grains[gr + 2]];
                            grains.push_back(newX);
                            grains.push_back(newY);
                            grains.push_back(newZ);
                        }
                    }
                }
            }
        }
        for (int i = 0; i < numCubes; i++)
        {
            for (int j = 0; j < numCubes; j++)
            {
                for (int k = 0; k < numCubes; k++)
                {
                    if (voxels[i][j][k] < 0)
                    {
                        voxels[i][j][k] = abs(voxels[i][j][k]);
                        grains.push_back(i);grains.push_back(j);grains.push_back(k);
                    }
                }
            }
        }
        answer = Check(voxels,numCubes,answer,n);
        if (n == 1)
            break;
        Cleaning(voxels,numCubes,grains);
    }
    return answer;
}

void Moore::Cleaning(int16_t*** voxels,short int numCubes,std::vector<int16_t> grains)
{
    for (size_t gr = 0; gr < grains.size(); gr += 2)
    {
        int count = 0;
        for (int x = -1; x < 2; x++)
        {
            if (((grains[gr] + x) >= 0) && ((grains[gr] + x) < numCubes))
            {
                for (int y = -1; y < 2; y++)
                {
                    if (((grains[gr + 1] + y) >= 0) && ((grains[gr + 1] + y) < numCubes))
                    {
                        for (int z = -1; z < 2; z++)
                        {
                            if (((grains[gr+2] + z) >= 0) && ((grains[gr+2] + z) < numCubes))
                            {
                                if (voxels[grains[gr] + x][grains[gr + 1] + y][grains[gr + 2] + z] != 0)
                                {
                                    count++;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (count == 26)
        {
            int indexRemove = grains[gr];
            auto it = grains.begin();
            advance(it, indexRemove);
            grains.erase(it);

            indexRemove = grains[gr + 1];
            it = grains.begin();
            advance(it, indexRemove);
            grains.erase(it);

            indexRemove = grains[gr + 2];
            it = grains.begin();
            advance(it, indexRemove);
            grains.erase(it);
        }
    }
}

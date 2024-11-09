#include <iostream>
#include <fstream>
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

Neumann::Neumann(short int numCubes, int numColors)
{
    this->numCubes = numCubes;
    this->numColors = numColors;
}


void Neumann::Generate_Filling(int isAnimation, int isWaveGeneration)
{
    unsigned int counter_max = pow(numCubes,3);
    auto start = std::chrono::high_resolution_clock::now();
    while (!grains.empty())
    {
        Coordinate temp;
        int32_t x,y,z;
        std::vector<Coordinate> newGrains;
        #pragma omp parallel for num_threads(12)
        for(size_t i = 0; i < grains.size(); i++)
        {
            temp = grains[i];
            x = temp.x;
            y = temp.y;
            z = temp.z;
            for (int32_t k = -1; k < 2; k+=2)
            {
                int32_t newX = k+x;
                int32_t newY = k+y;
                int32_t newZ = k+z;
                bool isValidX = (newX >= 0 && newX < numCubes) && voxels[newX][y][z] == 0;
                bool isValidY = (newY >= 0 && newY < numCubes) && voxels[x][newY][z] == 0;
                bool isValidZ = (newZ >= 0 && newZ < numCubes) && voxels[x][y][newZ] == 0;
                #pragma omp critical
                {
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
        }
        grains = std::move(newGrains);
        IterationNumber++;
        double o = (double)counter/counter_max;
        qDebug().nospace() << o << "\t" << IterationNumber << "\t" << grains.size();
        if (isAnimation == 1)
        {
            if (isWaveGeneration == 1 && remainingPoints > 0)
            {
                pointsForThisStep = max(1, static_cast<int>(0.1 * remainingPoints));
                newGrains = Add_New_Points(newGrains,pointsForThisStep);
                grains.insert(grains.end(), newGrains.begin(), newGrains.end());
                remainingPoints -= pointsForThisStep;
            }
            break;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    qDebug() << "Algorithm execution time: " << duration.count() << " seconds";
}

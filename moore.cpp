#include <iostream>
#include <windows.h>
#include <ctime>
#include <list>
#include <cmath>
#include <myglwidget.h>
#include <fstream>
#include "parent_algorithm.h"
#include "moore.h"

using namespace std;

Moore::Moore()
{

}

Moore::Moore(short int numCubes, int numColors)
{
    this->numCubes = numCubes;
    this->numColors = numColors;
}

void Moore::Generate_Filling(int isAnimation, int isWaveGeneration)
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
            for (int32_t k = -1; k < 2; k++)
            {
                for(int32_t p = -1; p < 2; p++)
                {
                    for(int32_t l = -1; l < 2; l++)
                    {
                        int32_t newX = k+x;
                        int32_t newY = p+y;
                        int32_t newZ = l+z;
                        bool isValidXYZ = (newX >= 0 && newX < numCubes) && (newY >= 0 && newY < numCubes) && (newZ >= 0 && newZ < numCubes) && voxels[newX][newY][newZ] == 0;
                            if (isValidXYZ)
                            {
                                #pragma omp critical
                                {
                                    voxels[newX][newY][newZ] = voxels[x][y][z];
                                    newGrains.push_back({newX,newY,newZ});
                                    counter++;
                                }

                            }
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

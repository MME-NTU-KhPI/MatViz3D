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
    std::string filename = "Neumann_150_0.5.csv";
    std::ofstream file;
    file.open(filename, std::ios::app);
    //file << "Thread;Time\n";
    int num_threads = 6;
    omp_set_num_threads(num_threads);
    unsigned int counter_max = pow(numCubes, 3);
    auto start = std::chrono::high_resolution_clock::now();
    while (!grains.empty())
    {
        std::vector<Coordinate> newGrains;
        unsigned int local_counter = 0;
        #pragma omp parallel reduction(+:local_counter)
        {
            std::vector<Coordinate> privateGrains;
            #pragma omp for schedule(static)
            for (size_t i = 0; i < grains.size(); i++)
            {
                Coordinate temp = grains[i];
                int32_t x = temp.x, y = temp.y, z = temp.z;

                for (int32_t k = -1; k < 2; k += 2)
                {
                    int32_t newX = k + x;
                    int32_t newY = k + y;
                    int32_t newZ = k + z;

                    if (!(newX >= 0 && newX < numCubes)) continue;
                    if (__sync_bool_compare_and_swap(&voxels[newX][y][z], 0, voxels[x][y][z]))
                    {
                        privateGrains.push_back({newX, y, z});
                        local_counter++;
                    }


                    if (!(newY >= 0 && newY < numCubes)) continue;
                    if (__sync_bool_compare_and_swap(&voxels[x][newY][z], 0, voxels[x][y][z]))
                    {
                        privateGrains.push_back({x, newY, z});
                        local_counter++;
                    }

                    if (!(newZ >= 0 && newZ < numCubes)) continue;
                    if (__sync_bool_compare_and_swap(&voxels[x][y][newZ], 0, voxels[x][y][z]))
                    {
                        privateGrains.push_back({x, y, newZ});
                        local_counter++;
                    }
                }
            }
            #pragma omp critical
            newGrains.insert(newGrains.end(), privateGrains.begin(), privateGrains.end());
        }
        counter += local_counter;
        grains = std::move(newGrains);
        IterationNumber++;
        double o = (double)counter / counter_max;
        qDebug().nospace() << o << "\t" << IterationNumber << "\t" << grains.size();
        if (isAnimation == 1)
        {
            if (isWaveGeneration == 1 && remainingPoints > 0)
            {
                pointsForThisStep = max(1, static_cast<int>(0.1 * remainingPoints));
                newGrains = Add_New_Points(newGrains, pointsForThisStep);
                grains.insert(grains.end(), newGrains.begin(), newGrains.end());
                remainingPoints -= pointsForThisStep;
            }
            break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    qDebug() << "Algorithm execution time: " << duration.count() << " seconds";
    file << num_threads << ";" << duration.count() << "\n";
    file.close();
}


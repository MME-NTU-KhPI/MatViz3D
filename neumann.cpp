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
    std::string filename = "output.csv";
    std::ofstream file;
    file.open(filename, std::ios::app);
    int num_threads = 12;
    omp_set_num_threads(num_threads);
    unsigned int counter_max = pow(numCubes, 3);
    auto start = std::chrono::high_resolution_clock::now();

    while (!grains.empty())
    {
        std::vector<Coordinate> newGrains;
        #pragma omp parallel
        {
            std::vector<Coordinate> privateGrains;

            #pragma omp for schedule(static)
            for (size_t i = 0; i < grains.size(); i++) {
                Coordinate temp = grains[i];
                int32_t x = temp.x, y = temp.y, z = temp.z;

                for (int32_t k = -1; k < 2; k += 2) {
                    int32_t newX = k + x;
                    int32_t newY = k + y;
                    int32_t newZ = k + z;

                    if ((newX >= 0 && newX < numCubes) && voxels[newX][y][z] == 0) {
                        if (__sync_bool_compare_and_swap(&voxels[newX][y][z], 0, voxels[x][y][z])) {
                            privateGrains.push_back({newX, y, z});
                            #pragma omp atomic
                            counter++;
                        }
                    }

                    if ((newY >= 0 && newY < numCubes) && voxels[x][newY][z] == 0) {
                        if (__sync_bool_compare_and_swap(&voxels[x][newY][z], 0, voxels[x][y][z])) {
                            privateGrains.push_back({x, newY, z});
                            #pragma omp atomic
                            counter++;
                        }
                    }

                    if ((newZ >= 0 && newZ < numCubes) && voxels[x][y][newZ] == 0) {
                        if (__sync_bool_compare_and_swap(&voxels[x][y][newZ], 0, voxels[x][y][z])) {
                            privateGrains.push_back({x, y, newZ});
                            #pragma omp atomic
                            counter++;
                        }
                    }
                }
            }

            // Слияние результатов из локального буфера каждого потока
            #pragma omp critical
            {
                newGrains.insert(newGrains.end(), privateGrains.begin(), privateGrains.end());
            }
        }

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
    file << num_threads << ";" << duration.count() << "\n";
    qDebug() << "Algorithm execution time: " << duration.count() << " seconds";
    file.close();
}


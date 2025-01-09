#include "composite.h"
#include <cmath>
#include <fstream>
#include <chrono>
#include <QString>
#include <QTextStream>
#include <QFile>
#include <QDir>

Composite::Composite() {}

Composite::Composite(short int numCubes, int numColors)
{
    this->numCubes = numCubes;
    this->numColors = numColors;
}

void Composite::setRadius(short int radius)
{
    this->radius = radius;
}

void Composite::FillWithCylinder(int isAnimation, int isWaveGeneration)
{
    std::ofstream file;
    file.open("Composite_250_50.csv", std::ios::app);
    //file << "Thread;Time\n";
    int num_threads = 6;
    omp_set_num_threads(num_threads);
    auto start = std::chrono::high_resolution_clock::now();

    // Initialize all voxels to 1 in parallel
    #pragma omp parallel for collapse(3)
    for (int k = 0; k < numCubes; k++)
        for (int i = 0; i < numCubes; i++)
            for (int j = 0; j < numCubes; j++)
                voxels[k][i][j] = 1;

    short int cylinderDiameter = static_cast<short int>(2 * radius);
    short int gap = 5;
    short int centerOffset = cylinderDiameter + gap;
    short int maxCylindersPerRow = numCubes / centerOffset + 1;
    short int xStartOffset = (numCubes - (maxCylindersPerRow * centerOffset)) / 2;
    const float radius_squared = static_cast<float>(radius * radius);

    #pragma omp parallel
    {
        #pragma omp for collapse(4) schedule(dynamic, 8)
        for (int i = 0; i < maxCylindersPerRow; i++) {
            for (int j = 0; j < maxCylindersPerRow; j++) {
                for (int x = 0; x < numCubes; x++) {
                    for (int y = 0; y < numCubes; y++) {
                        short int shiftX = (j % 2 == 0) ? 0 : centerOffset / 2;
                        short int centerX = xStartOffset + i * centerOffset + radius + shiftX;
                        short int centerY = xStartOffset + j * centerOffset + radius;

                        float dx = static_cast<float>(x - centerX);
                        float dy = static_cast<float>(y - centerY);
                        float distance_squared = dx * dx + dy * dy;
                        
                        if (distance_squared <= radius_squared) {
                            #pragma omp simd
                            for (int z = 0; z < numCubes; z++) {
                                voxels[x][y][z] = 5;
                            }
                        }
                    }
                }
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    qDebug() << "Algorithm execution time: " << duration.count() << " seconds";
    file << num_threads << ";" << duration.count() << "\n";
    file.close();
}

void Composite::FillWithTetra(int isAnimation, int isWaveGeneration, short int offsetX, short int offsetY, short int offsetZ)
{
    #pragma omp parallel for collapse(3)
    for(int k = 0; k < numCubes; k++)
        for(int i = 0; i < numCubes; i++)
            for(int j = 0; j < numCubes; j++)
                voxels[k][i][j] = 1;

    short int cylinderDiameter = static_cast<short int>(2 * radius);
    short int gap = 5;
    short int centerOffset = cylinderDiameter + gap;

    short int maxCylindersPerRow = numCubes / centerOffset + 1;

    short int xStartOffset = (numCubes - (maxCylindersPerRow * centerOffset)) / 2;

    #pragma omp parallel for collapse(2)
    for (int i = 0; i < maxCylindersPerRow; i++) {
        for (int j = 0; j < maxCylindersPerRow; j++) {
            short int shiftX = (j % 2 == 0) ? 0 : centerOffset / 2;
            short int centerX = xStartOffset + i * centerOffset + radius + shiftX;
            short int centerY = xStartOffset + j * centerOffset + radius;

            #pragma omp parallel for collapse(3)
            for (int x = 0; x < numCubes; x++) {
                for (int y = 0; y < numCubes; y++) {
                    for (int z = 0; z < numCubes; z++) {
                        int distance = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                        if (distance <= radius) {
                            voxels[x][y][z] = 5;
                        }
                    }
                }
            }
        }
    }
}

void Composite::FillWithHexa(int isAnimation, int isWaveGeneration,short int offsetX, short int offsetY, short int offsetZ)
{
    #pragma omp parallel for collapse(3)
        for(int k = 0; k < numCubes; k++)
            for(int i = 0; i < numCubes; i++)
                for(int j = 0; j < numCubes; j++)
                    voxels[k][i][j] = 1;

    short int centerX = offsetX + radius;
    short int centerY = offsetY + radius;
    const float sqrt3 = sqrt(3.0);

    for (int z = offsetZ; z < offsetZ + 2 * radius && z < numCubes; z++) {
        for (int y = offsetY; y < offsetY + 2 * radius && y < numCubes; y++) {
            for (int x = offsetX; x < offsetX + 2 * radius && x < numCubes; x++) {
                short int relX = x - centerX;
                short int relY = y - centerY;

                if (abs(relX) <= radius &&
                    abs(relY) <= sqrt3 * radius / 2 &&
                    relY <= sqrt3 * radius - sqrt3 * abs(relX) &&
                    relY >= -sqrt3 * radius + sqrt3 * abs(relX))
                {
                    voxels[x][y][z] = 4;
                }
            }
        }
    }
}


void Composite::Generate_Filling(int isAnimation, int isWaveGeneration, int isPeriodicStructure)
{

};

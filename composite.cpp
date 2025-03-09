#include "composite.h"
#include <cmath>
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

void Composite::FillWithCylinder()
{
    radius = numColors;
    #pragma omp parallel for collapse(3)
    for (int k = 0; k < numCubes; k++)
        for (int i = 0; i < numCubes; i++)
            for (int j = 0; j < numCubes; j++)
                voxels[k][i][j] = 1;

    for (int x = 0; x < numCubes; x++) {
        for (int y = 0; y < numCubes; y++) {
            double dist = sqrt((x - numCubes / 2) * (x - numCubes / 2) + (y - numCubes / 2) * (y - numCubes / 2));
            if (round(dist) <= radius)
            {
                for (int z = 0; z < numCubes; z++)
                {
                    voxels[x][y][z] = 5;
                }
            }
        }
    }

    int cornerRadius = radius;
    int maxIdx = numCubes - 1;

    for (int x = 0; x < numCubes; x++) {
        for (int y = 0; y < numCubes; y++) {
            double dist1 = sqrt(x * x + y * y);
            double dist2 = sqrt((maxIdx - x) * (maxIdx - x) + y * y);
            double dist3 = sqrt(x * x + (maxIdx - y) * (maxIdx - y));
            double dist4 = sqrt((maxIdx - x) * (maxIdx - x) + (maxIdx - y) * (maxIdx - y));

            if (dist1 <= cornerRadius || dist2 <= cornerRadius ||
                dist3 <= cornerRadius || dist4 <= cornerRadius) {
                for (int z = 0; z < numCubes; z++) {
                    voxels[x][y][z] = 5;
                }
            }
        }
    }
}

void Composite::FillWithTetra()
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

void Composite::FillWithHexa()
{
    #pragma omp parallel for collapse(3)
        for(int k = 0; k < numCubes; k++)
            for(int i = 0; i < numCubes; i++)
                for(int j = 0; j < numCubes; j++)
                    voxels[k][i][j] = 1;
}


void Composite::Next_Iteration(std::function<void()> callback)
{
    setRadius(numColors);

    int local_filled_voxels = 0;

    #pragma omp parallel for collapse(3) reduction(+:local_filled_voxels)
    for (int k = 0; k < numCubes; k++)
        for (int i = 0; i < numCubes; i++)
            for (int j = 0; j < numCubes; j++)
            {
                voxels[k][i][j] = 1;
                local_filled_voxels++;
            }

    #pragma omp atomic
    filled_voxels += local_filled_voxels;

    #pragma omp parallel for collapse(2)
    for (int x = 0; x < numCubes; x++) {
        for (int y = 0; y < numCubes; y++) {
            double dist = std::sqrt((x - numCubes / 2) * (x - numCubes / 2) + (y - numCubes / 2) * (y - numCubes / 2));
            if (std::round(dist) <= radius)
            {
                for (int z = 0; z < numCubes; z++)
                {
                    voxels[x][y][z] = 5;
                }
            }
        }
    }
}


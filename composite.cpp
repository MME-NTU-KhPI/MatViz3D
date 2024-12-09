#include "composite.h"
#include <cmath>

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
    #pragma omp parallel for collapse(3)
    for (int k = 0; k < numCubes; k++)
        for (int i = 0; i < numCubes; i++)
            for (int j = 0; j < numCubes; j++)
                voxels[k][i][j] = 1;

    float cubeVolume = std::pow(numCubes, 3);

    float targetCylinderVolume = cubeVolume * 0.1 * radius;

    float radius = std::cbrt(targetCylinderVolume / (M_PI * numCubes));

    short int cylinderDiameter = static_cast<short int>(2 * radius);
    short int spacing = cylinderDiameter;
    short int centerOffset = cylinderDiameter + spacing;

    short int maxCylindersPerRow = numCubes / centerOffset;

    short int xStartOffset = (numCubes - (maxCylindersPerRow * centerOffset - spacing)) / 2;
    short int yStartOffset = xStartOffset;

    for (int i = 0; i < maxCylindersPerRow; i++) {
        for (int j = 0; j < maxCylindersPerRow; j++) {
            short int centerX = xStartOffset + i * centerOffset + radius;
            short int centerY = yStartOffset + j * centerOffset + radius;

            for (int x = 0; x < numCubes; x++) {
                for (int y = 0; y < numCubes; y++) {
                    int distance = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                    if (distance <= radius) {
                        for (int z = 0; z < numCubes; z++) {
                            voxels[x][y][z] = 5;
                        }
                    }
                }
            }
        }
    }
}

void Composite::FillWithTetra(int isAnimation, int isWaveGeneration, short int offsetX, short int offsetY, short int offsetZ)
{
    #pragma omp parallel for collapse(3)
        for(int k = 0; k < numCubes; k++)
            for(int i = 0; i < numCubes; i++)
                for(int j = 0; j < numCubes; j++)
                    voxels[k][i][j] = 1;

    float cubeVolume = std::pow(numCubes, 3);

    float targetCylinderVolume = cubeVolume * 0.1 * radius;

    float radius = std::cbrt(targetCylinderVolume / (M_PI * numCubes));

    short int cylinderDiameter = static_cast<short int>(2 * radius);
    short int spacing = cylinderDiameter;
    short int centerOffset = cylinderDiameter + spacing;

    short int maxCylindersBase = numCubes / centerOffset;

    short int baseXOffset = (numCubes - (maxCylindersBase * centerOffset - spacing)) / 2;
    short int baseZOffset = baseXOffset;

    for (int yLayer = 0; yLayer < maxCylindersBase; yLayer++) {
        short int cylindersInLayer = maxCylindersBase - yLayer;

        if (cylindersInLayer <= 0) {
            break;
        }

        short int xOffset = baseXOffset + yLayer * centerOffset / 2;
        short int zOffset = baseZOffset + yLayer * centerOffset / 2;

        for (int i = 0; i < cylindersInLayer; i++) {
            for (int j = 0; j < cylindersInLayer; j++) {
                short int centerX = xOffset + i * centerOffset + radius;
                short int centerY = zOffset + j * centerOffset + radius;

                for (int x = 0; x <= numCubes; x++) {
                    for (int y = 0; y <= numCubes; y++) {
                        int distance = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                        if (distance <= radius) {
                            for (int zFill = 0; zFill <= numCubes; zFill++) {
                                voxels[x][y][zFill] = 5;
                            }
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


void Composite::Generate_Filling(int isAnimation, int isWaveGeneration)
{

};

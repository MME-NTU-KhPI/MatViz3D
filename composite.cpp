#include "composite.h"
#include <math.h>

Composite::Composite() {}

Composite::Composite(short int numCubes, int numColors)
{
    this->numCubes = numCubes;
    this->numColors = numColors;
}

void Composite::FillWithCylinder(int isAnimation, int isWaveGeneration, short int radius)
{
    short int centerX = numCubes / 2;
    short int centerY = numCubes / 2;

    for (int i = 0; i < numCubes; i++) // Z
    {
        for (int j = 0; j < numCubes; j++) // Y
        {
            for (int k = 0; k < numCubes; k++) // X
            {

                short int distance = sqrt(pow(j - centerY, 2) + pow(k - centerX, 2));

                if (distance <= radius) {
                    voxels[k][j][i] = 2;
                }
            }
        }
    }
}

void Composite::Generate_Filling(int isAnimation, int isWaveGeneration)
{

};

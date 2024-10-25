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
        for(int k = 0; k < numCubes; k++)
            for(int i = 0; i < numCubes; i++)
                for(int j = 0; j < numCubes; j++)
                    voxels[k][i][j] = 1;

    short int centerX = numCubes / 2;
    short int centerY = numCubes / 2;

    #pragma omp parallel for collapse(3)
    for (int i = 0; i < numCubes; i++) // Z
    {
        for (int j = 0; j < numCubes; j++) // Y
        {
            for (int k = 0; k < numCubes; k++) // X
            {

                short int distance = sqrt(pow(j - centerY, 2) + pow(k - centerX, 2));

                if (distance <= radius) {
                    voxels[k][j][i] = 4;
                }
            }
        }
    }
}

void Composite::FillWithTetra(int isAnimation, int isWaveGeneration)
{
    #pragma omp parallel for collapse(3)
        for(int k = 0; k < numCubes; k++)
            for(int i = 0; i < numCubes; i++)
                for(int j = 0; j < numCubes; j++)
                    voxels[k][i][j] = 1;

    short int centerX = numCubes / 2;
    short int centerY = numCubes / 2;
    short int a = radius * 2;

    for (int i = 0; i < numCubes; i++) // Z
    {
        for (int j = 0; j < numCubes; j++) // Y
        {
            for (int k = 0; k < numCubes; k++) // X
            {
                double distance = sqrt(pow(j - centerY, 2) + pow(k - centerX, 2));

                if ((distance <= (a / 2)) &&
                    (k == (numCubes / 2) || j == (numCubes / 2)))
                {
                    voxels[k][j][i] = 3;
                }
                else if ((distance <= (sqrt(2) * (a / 2))) &&
                         (k != (numCubes / 2)) &&
                         (j != (numCubes / 2)))
                {
                    voxels[k][j][i] = 5;
                }
            }
        }
    }
}

void Composite::FillWithHexa(int isAnimation, int isWaveGeneration)
{
    #pragma omp parallel for collapse(3)
        for(int k = 0; k < numCubes; k++)
            for(int i = 0; i < numCubes; i++)
                for(int j = 0; j < numCubes; j++)
                    voxels[k][i][j] = 1;

    short int centerX = numCubes / 2;
    short int centerY = numCubes / 2;

    // Константа для расчёта размера гексагона (корень из 3)
    const float sqrt3 = sqrt(3.0);

    for (int i = 0; i < numCubes; i++) // Z
    {
        for (int j = 0; j < numCubes; j++) // Y
        {
            for (int k = 0; k < numCubes; k++) // X
            {
                // Приведение координат к центру куба
                short int relX = k - centerX;
                short int relY = j - centerY;

                // Проверка принадлежности точки (relX, relY) гексагонической области
                if (abs(relX) <= radius &&
                    abs(relY) <= sqrt3 * radius / 2 &&
                    abs(relY) <= -sqrt3 * abs(relX) + sqrt3 * radius)
                {
                    voxels[k][j][i] = 2; // Заполняем область гексагональной призмы
                }
            }
        }
    }
}

void Composite::Generate_Filling(int isAnimation, int isWaveGeneration)
{

};

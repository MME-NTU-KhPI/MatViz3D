#include "composite.h"
#include <math.h>

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
                if ((sqrt(pow(j - centerY, 2) + pow(k - centerX, 2))) <= (short int)(a / 2) && (k == (short int)(numCubes / 2) || j == (short int)(numCubes / 2)))
                {
                    voxels[k][j][i] = 2;
                }
                else if ((sqrt(pow(j - centerY, 2) + pow(k - centerX, 2))) <= (short int)(sqrt(2) * (a / 2)) && k != (short int)(numCubes / 2) && j != (short int)(numCubes / 2))
                {
                    voxels[k][j][i] = 3;
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

    // Коэффициент для управления размером гексагональной области (приблизительное значение).
    const float sqrt3 = sqrt(3.0); // sqrt(3) для расчёта длины стороны гексагональной области

    for (int i = 0; i < numCubes; i++) // Z
    {
        for (int j = 0; j < numCubes; j++) // Y
        {
            for (int k = 0; k < numCubes; k++) // X
            {
                // Приведение координат к центру
                short int relX = k - centerX;
                short int relY = j - centerY;

                // Проверка, принадлежит ли точка (x, y) гексагону
                // Неравенства для описания шестиугольника
                if (abs(relX) <= radius && abs(relY) <= sqrt3 * radius / 2 &&
                    abs(relY) <= -sqrt3 * abs(relX) + sqrt3 * radius)
                {
                    //voxels[x][y][z] = 1; // Заполняем область гексагональной призмы
                }
            }
        }
    }
}

void Composite::Generate_Filling(int isAnimation, int isWaveGeneration)
{

};

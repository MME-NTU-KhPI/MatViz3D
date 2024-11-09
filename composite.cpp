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

        short int cylinderDiameter = radius * 2;
        short int spacing = radius * 1.5;  // Увеличенный зазор между цилиндрами
        short int centerOffset = cylinderDiameter + spacing;

        short int maxCylinders = numCubes / centerOffset;  // Максимальное количество цилиндров по одной оси

        color = 1 + rand() % 10;

                        // Итерация по слоям (Z-координата)
        for (int z = 0; z < numCubes; z++) {
            // Итерация по сетке с учётом centerOffset
            for (int i = 0; i < maxCylinders; i++) {
                for (int j = 0; j < maxCylinders; j++) {
                    // Определение центра цилиндра
                    short int centerX = i * centerOffset + radius;
                    short int centerY = j * centerOffset + radius;

                    // Чередование шахматного порядка
                    if ((i + j) % 2 == 1) {
                        centerX += centerOffset / 2;
                    }

                    // Создание цилиндра на текущей позиции
                    for (int y = 0; y < numCubes; y++) {  // Y
                        for (int x = 0; x < numCubes; x++) {  // X
                            int distance = sqrt(pow(y - centerY, 2) + pow(x - centerX, 2));
                            if (distance <= radius) {
                                voxels[x][y][z] = color;
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

    short int centerX = offsetX + radius;
    short int centerY = offsetY + radius;
    short int a = radius * 2;

    for (int z = offsetZ; z < offsetZ + 2 * radius && z < numCubes; z++) {
        for (int y = offsetY; y < offsetY + 2 * radius && y < numCubes; y++) {
            for (int x = offsetX; x < offsetX + 2 * radius && x < numCubes; x++) {
                double distance = sqrt(pow(y - centerY, 2) + pow(x - centerX, 2));

                if ((distance <= (a / 2)) &&
                    (x == (numCubes / 2) || y == (numCubes / 2)))
                {
                    voxels[x][y][z] = 2;
                }
                else if ((distance <= (sqrt(2) * (a / 2))) &&
                         (x != (numCubes / 2)) &&
                         (y != (numCubes / 2)))
                {
                    voxels[x][y][z] = 4;
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

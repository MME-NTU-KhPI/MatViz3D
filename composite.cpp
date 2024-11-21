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

    // Объём куба
    float cubeVolume = std::pow(numCubes, 3);

    // Общий объём цилиндров
    float targetCylinderVolume = cubeVolume * 0.1 * radius;

    // Радиус цилиндров (вычисляется из заданного объёма цилиндров)
    float radius = std::cbrt(targetCylinderVolume / (M_PI * numCubes));

    // Диаметр цилиндра и промежутки между цилиндрами
    short int cylinderDiameter = static_cast<short int>(2 * radius);
    short int spacing = cylinderDiameter * 0.05; // Зазор между цилиндрами
    short int centerOffset = cylinderDiameter + spacing;

    // Определяем максимальное количество цилиндров вдоль одной стороны
    short int maxCylindersPerRow = numCubes / centerOffset;

    // Итерация по сетке для размещения цилиндров
    for (int i = 0; i < maxCylindersPerRow; i++) {
        for (int j = 0; j < maxCylindersPerRow; j++) {
            // Определяем центр текущего цилиндра
            short int centerX = spacing + i * centerOffset + radius;
            short int centerY = spacing + j * centerOffset + radius;

            // Создаём цилиндр, проходящий через весь куб вдоль оси Z
            for (int x = 0; x <= numCubes; x++) {
                for (int y = 0; y <= numCubes; y++) {
                    int distance = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                    if (distance <= radius) {
                        // Заполняем весь столбец по оси Z
                        for (int z = 0; z <= numCubes; z++) {
                            voxels[x][y][z] = 5; // Помечаем цилиндры
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

#include <QDebug>
#include <ctime>
#include <cmath>
#include <cstdint>
#include "probability_circle.h"
#include "parent_algorithm.h"


using namespace std;
Probability_Circle::Probability_Circle():Parent_Algorithm()
{
    // Constructor implementation
    // Add any initialization code or logic here
}

Probability_Circle::Probability_Circle(short int numCubes, int numColors)
{
    this->numCubes = numCubes;
    this->numColors = numColors;
}

void Probability_Circle::Next_Iteration(std::function<void()> callback)
{
    srand(time(NULL));
    unsigned int counter_max = pow(numCubes,3);
    Coordinate temp;
    int32_t x,y,z;
    std::vector<Coordinate> newGrains;

    const int N_gr = numColors;
    int total_nucleated_so_far = 1;

    for(size_t i = 0; i < grains.size(); i++)
    {
        temp = grains[i];
        x = temp.x;
        y = temp.y;
        z = temp.z;
        for (int32_t k = -1; k < 2; k++)
        {
            for(int32_t p = -1; p < 2; p++)
            {
                for(int32_t l = -1; l < 2; l++)
                {
                    int32_t newX = k+x;
                    int32_t newY = p+y;
                    int32_t newZ = l+z;
                    bool isValidXYZ = (newX >= 0 && newX < numCubes) && (newY >= 0 && newY < numCubes) && (newZ >= 0 && newZ < numCubes) && voxels[newX][newY][newZ] == 0;
                    bool Chance94 = (rand() % 100) < 94;
                    bool Chance50 = (rand() % 100) < 50;
                    bool Chance17 = (rand() % 100) < 17;
                    if (isValidXYZ)
                    {
                        if (p != 0 && l != 0 && k != 0)
                        {
                            if(Chance17)
                            {
                                voxels[newX][newY][newZ] = voxels[x][y][z];
                                newGrains.push_back({newX,newY,newZ});
                                filled_voxels++;
                            }
                            else
                            {
                                newGrains.push_back({x,y,z});
                            }
                        }
                        if(p == 0 || l == 0 || k == 0)
                        {
                            if (Chance50)
                            {
                                voxels[newX][newY][newZ] = voxels[x][y][z];
                                newGrains.push_back({newX,newY,newZ});
                                filled_voxels++;
                            }
                            else
                            {
                                newGrains.push_back({x,y,z});
                            }
                        }
                        else
                        {
                            if(Chance94)
                            {
                                voxels[newX][newY][newZ] = voxels[x][y][z];
                                newGrains.push_back({newX,newY,newZ});
                                filled_voxels++;
                            }
                            else
                            {
                                newGrains.push_back({x,y,z});
                            }
                        }
                    }
                }
            }
        }
    }
    grains.clear();
    grains.insert(grains.end(), newGrains.begin(), newGrains.end());
    newGrains.clear();
    qDebug() << filled_voxels << "\t" << pow(numCubes,3);
    IterationNumber++;

    QString nuclLogInfo = "";

    if (flags.isWaveGeneration && total_nucleated_so_far < N_gr && filled_voxels < counter_max)
    {
        double arg = (IterationNumber - (Parameters::wave_coefficient / 2.0)) / std::sqrt(static_cast<double>(N_gr));
        double cumulative_fraction = 0.5 * (1.0 + std::erf(arg));

        int total_should_be_now = static_cast<int>(cumulative_fraction * N_gr);
        if (total_should_be_now < 1) total_should_be_now = 1;

        int pointsToCreate = total_should_be_now - total_nucleated_so_far;

        if (pointsToCreate > 0)
        {
            int placedRandomly = 0;

            for (int p = 0; p < pointsToCreate; ++p) {
                bool success = false;
                for (int retry = 0; retry < 10; ++retry) {
                    int rx = std::rand() % numCubes;
                    int ry = std::rand() % numCubes;
                    int rz = std::rand() % numCubes;

                    if (voxels[rx][ry][rz] == 0) {
                        voxels[rx][ry][rz] = ++color;
                        grains.push_back({rx, ry, rz});
                        filled_voxels++;
                        success = true;
                        placedRandomly++;
                        break;
                    }
                }
                if (!success) break;
            }

            int leftToPlace = pointsToCreate - placedRandomly;

            if (leftToPlace > 0) {
                grains = Add_New_Points(grains, leftToPlace);
            }

            total_nucleated_so_far += pointsToCreate;

            nuclLogInfo = QString(" | [Nucl] N(n): %1 | Added: %2 | Tot: %3")
                              .arg(cumulative_fraction, -8, 'f', 4)
                              .arg(pointsToCreate, -6)
                              .arg(total_nucleated_so_far, -6);
        }
    }

    double o = static_cast<double>(filled_voxels) / counter_max;
    QString logLine = QString("%1 %2 %3")
                          .arg(o, -12, 'g', 6)
                          .arg(IterationNumber, -6)
                          .arg((int)grains.size(), -10);

    if (!nuclLogInfo.isEmpty()) {
        logLine += nuclLogInfo;
    }

    qDebug().noquote() << logLine;

    if (flags.isAnimation)
    {
        callback();
    }
}

#include <ctime>
#include <cmath>
#include <cstdint>
#include <QDebug>
#include "probability_ellipse.h"
#include "parent_algorithm.h"


using namespace std;

Probability_Ellipse::Probability_Ellipse()
{

}

Probability_Ellipse::Probability_Ellipse(short int numCubes, int numColors)
{
    this->numCubes = numCubes;
    this->numColors = numColors;
}


void Probability_Ellipse::Next_Iteration(std::function<void()> callback)
{
    srand(time(NULL));
    unsigned int counter_max = pow(numCubes,3);
    Coordinate temp;
    int32_t x,y,z;
    std::vector<Coordinate> newGrains;
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
                    bool Chance50 = (rand() % 100) < 50;
                    bool Chance16 = (rand() % 100) < 16;
                    bool Chance44 = (rand() % 100) < 44;
                    if (isValidXYZ)
                    {
                        if(p == 0 || l == 0)
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
                        else if ((p + l == abs(2)))
                        {
                            if(Chance16)
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
                            if(Chance44)
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
    double o = (double)filled_voxels/counter_max;
    qDebug().nospace() << o << "\t" << IterationNumber << "\t" << grains.size();
    if (flags.isAnimation)
    {
        if (flags.isWaveGeneration)
        {
            if (remainingPoints > 0)
            {
                pointsForThisStep = max(1, static_cast<int>(Parameters::wave_coefficient * remainingPoints));
                newGrains = Add_New_Points(newGrains,pointsForThisStep);
                grains.insert(grains.end(), newGrains.begin(), newGrains.end());
                remainingPoints -= pointsForThisStep;
            }
        }
        callback();
    }
}

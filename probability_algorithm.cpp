#include "probability_algorithm.h"
#include "ui_probability_algorithm.h"

Probability_Algorithm::Probability_Algorithm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Probability_Algorithm)
{
    ui->setupUi(this);
}

Probability_Algorithm::~Probability_Algorithm()
{
    delete ui;
}

void Probability_Algorithm::Generate_Filling(int isAnimation, int isWaveGeneration)
{
    srand(time(NULL));
    unsigned int counter_max = pow(numCubes,3);
    while (!grains.empty())
    {
        Coordinate temp;
        int16_t x,y,z;
        std::vector<Coordinate> newGrains;
        for(size_t i = 0; i < grains.size(); i++)
        {
            temp = grains[i];
            x = temp.x;
            y = temp.y;
            z = temp.z;
            for (int16_t k = -1; k < 2; k++)
            {
                for(int16_t p = -1; p < 2; p++)
                {
                    for(int16_t l = -1; l < 2; l++)
                    {
                        int16_t newX = k+x;
                        int16_t newY = p+y;
                        int16_t newZ = l+z;
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
                                    counter++;
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
                                    counter++;
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
                                    counter++;
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
        IterationNumber++;
        double o = (double)counter/counter_max;
        qDebug().nospace() << o << "\t" << IterationNumber << "\t" << grains.size();
        if (isAnimation == 1)
        {
            if (isWaveGeneration == 1)
            {
                if (remainingPoints > 0)
                {
                    pointsForThisStep = std::max(1, static_cast<int>(0.1 * remainingPoints));
                    newGrains = Add_New_Points(newGrains,pointsForThisStep);
                    grains.insert(grains.end(), newGrains.begin(), newGrains.end());
                    remainingPoints -= pointsForThisStep;
                }
            }
            break;
        }
    }
}

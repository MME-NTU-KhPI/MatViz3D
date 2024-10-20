#include "radial.h"
#include "QDebug"

Radial::Radial()
{

}

std::vector<Parent_Algorithm::Coordinate> Radial::Generate_Filling(int16_t*** voxels, short int numCubes,int n, std::vector<Parent_Algorithm::Coordinate> grains)
{
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
                        bool XYZ = (k == 0) || (p == 0) || (l == 0);
                        if (isValidXYZ)
                        {
                            if(XYZ)
                            {
                                voxels[newX][newY][newZ] = voxels[x][y][z];
                                newGrains.push_back({newX,newY,newZ});
                                counter++;
                            }
                        }
                    }
                }
            }
        }
        grains.clear();
        grains.insert(grains.end(), newGrains.begin(), newGrains.end());
        IterationNumber++;
        double o = (double)counter/counter_max;
        qDebug().nospace() << o << "\t" << IterationNumber << "\t" << grains.size();
        // Перевірка, чи потрібна анімація
        if (n == 1)
        {
            break;
        }
    }
    return grains;
}

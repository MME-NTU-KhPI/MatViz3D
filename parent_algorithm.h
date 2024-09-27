
#ifndef PARENT_ALGORITHM_H
#define PARENT_ALGORITHM_H

#include <omp.h>
#include <cstdint>
#include <vector>

class Parent_Algorithm
{
private:
    template <class T> T*** Create3D(int N1, int N2, int N3);
    template <class T> void Delete3D(T ***array);
public:
    #pragma pack(push, 4)
    struct Coordinate
    {
        int32_t x;
        int32_t y;
        int32_t z;
    };
    #pragma pack(pop)
    unsigned int counter;
    unsigned int IterationNumber = 0;
    int32_t*** voxels;
    short int numCubes;
    int numColors;
    int32_t color = 0;
    std::vector<Coordinate> grains;
    Parent_Algorithm();
    virtual void Generate_Filling(int isAnimation, int isWaveGeneration) = 0;
    int32_t*** Generate_Initial_Cube();
    void Generate_Random_Starting_Points(int isWaveGeneration);
    std::vector<Coordinate> Add_New_Points(std::vector<Coordinate> grains, int numPoints);
    std::vector<Coordinate> Delete_Points(std::vector<Coordinate> grains,size_t i);
    int initialWave;
    int remainingPoints;
    int pointsForThisStep;
};

#endif // PARENT_ALGORITHM_H

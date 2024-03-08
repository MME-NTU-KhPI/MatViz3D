
#ifndef PARENT_ALGORITHM_H
#define PARENT_ALGORITHM_H

#include <omp.h>
#include <cstdint>
#include <vector>


class Parent_Algorithm
{
public:
    #pragma pack(push, 4)
    struct Coordinate
    {
        int16_t x;
        int16_t y;
        int16_t z;
    };
    #pragma pack(pop)

    unsigned int counter;
    unsigned int IterationNumber = 0;
    Parent_Algorithm();
    virtual std::vector<Coordinate> Generate_Filling(int16_t*** voxels, short int numCubes,int n, std::vector<Coordinate> grains) = 0;
    int16_t*** Generate_Initial_Cube(short int numCubes);
    std::vector<Coordinate> Generate_Random_Starting_Points(int16_t*** voxels, short int numCubes, int numColors);
    std::vector<Coordinate> Delete_Points(std::vector<Coordinate> grains,size_t i);
    bool Check(int16_t*** voxels,short int numCubes, bool answer,int n);
};

#endif // PARENT_ALGORITHM_H

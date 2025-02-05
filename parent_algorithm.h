
#ifndef PARENT_ALGORITHM_H
#define PARENT_ALGORITHM_H

#include <omp.h>
#include <memory>
#include <cmath>
#include <cstdint>
#include <vector>

struct AlgorithmFlags {
    bool isAnimation = false;
    bool isWaveGeneration = false;
    bool isPeriodicStructure = false;
    bool isDone = false;
};

class Parent_Algorithm
{
private:
    template <class T> T*** Create3D(int N1, int N2, int N3);
    template <class T> void Delete3D(T ***array);

protected:
    AlgorithmFlags flags;
    unsigned int counter = 0;
    unsigned int IterationNumber = 0;
    int initialWave;
    int remainingPoints;
    int pointsForThisStep;
    int32_t*** voxels;
    short int numCubes;
    int numColors;
    int32_t color = 0;
    unsigned int filled_voxels = 0;

public:
    #pragma pack(push, 4)
    struct Coordinate
    {
        int32_t x;
        int32_t y;
        int32_t z;
    };
    #pragma pack(pop)
    std::vector<Coordinate> grains;
    void setNumCubes(short int numCubes) { this->numCubes = numCubes; };
    void setNumColors(int numColors) {this->numColors = numColors; };
    void setRemainingPoints(int remainingPoints) { this->remainingPoints = remainingPoints; };
    void setGrains (std::vector<Coordinate> grains) { this->grains = grains; };
    void setFlags(const AlgorithmFlags& newFlags) { flags = newFlags; }
    void setAnimation(bool value) { flags.isAnimation = value; }
    void setWaveGeneration(bool value) { flags.isWaveGeneration = value; }
    void setPeriodicStructure(bool value) { flags.isPeriodicStructure = value; }
    void setDone() { if (filled_voxels >= pow(numCubes,3)) { flags.isDone = true; } };
    short int getNumCubes() { return numCubes; };
    int getNumColors() { return numColors; };
    int getRemainingPoints() { return remainingPoints; };
    unsigned int getFilled_Voxels() { return filled_voxels; };
    std::vector<Coordinate> getGrains() { return grains; };
    int32_t*** getVoxels() { return voxels; };
    AlgorithmFlags getFlags() const { return flags; };
    bool getAnimation() const { return flags.isAnimation; };
    bool getDone() { return flags.isDone; };
    Parent_Algorithm();
    virtual ~Parent_Algorithm();
    virtual void Generate_Filling() = 0;
    virtual int32_t*** Generate_Initial_Cube();
    virtual void Generate_Random_Starting_Points(bool isWaveGeneration);
    std::vector<Coordinate> Add_New_Points(std::vector<Coordinate> grains, int numPoints);
    std::vector<Coordinate> Delete_Points(std::vector<Coordinate> grains,size_t i);
};

#endif // PARENT_ALGORITHM_H

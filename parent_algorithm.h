#ifndef PARENT_ALGORITHM_H
#define PARENT_ALGORITHM_H

#include <omp.h>
#include <cmath>
#include <cstdint>
#include <functional>
#include "parameters.h"
#include "loadstepmanager.h"
/**
 * @brief Structure containing algorithm flags.
 */
struct AlgorithmFlags {
    bool isAnimation = false;  ///< Animation flag
    bool isWaveGeneration = false; ///< Wave generation flag
    bool isPeriodicStructure = false; ///< Periodic structure flag
    bool isDone = false; ///< Algorithm completion flag
};

/**
 * @brief Base algorithm class containing main parameters and methods.
 */
class Parent_Algorithm
{
    friend class LoadStepManager;
private:
    /**
     * @brief Creates a 3D array of size N1 x N2 x N3.
     * @tparam T Type of array elements.
     * @param N1 Size along the first dimension.
     * @param N2 Size along the second dimension.
     * @param N3 Size along the third dimension.
     * @return Pointer to the created 3D array.
     */
     template <class T> static T*** Create3D(int N1, int N2, int N3);

    /**
     * @brief Deletes a 3D array.
     * @tparam T Type of array elements.
     * @param array Pointer to the 3D array.
     */
    template <class T> static void Delete3D(T*** array);

protected:
    AlgorithmFlags flags; ///< Algorithm flags
    unsigned int IterationNumber = 0; ///< Current iteration number
    int initialWave; ///< Initial wave
    int remainingPoints; ///< Remaining points
    int pointsForThisStep; ///< Number of points for the current step
    int32_t*** voxels; ///< Voxel representation of the structure
    short int numCubes; ///< Number of cubes in the structure
    int numColors; ///< Number of colors
    int32_t color = 0; ///< Current color
    unsigned int filled_voxels = 0; ///< Number of filled voxels

public:
#pragma pack(push, 4)
    /**
     * @brief Coordinate structure for a point in 3D space.
     */
    struct Coordinate
    {
        int32_t x; ///< X coordinate
        int32_t y; ///< Y coordinate
        int32_t z; ///< Z coordinate
    };
#pragma pack(pop)

    std::vector<Coordinate> grains; ///< Vector of structure grains

    /** @brief Sets the number of cubes. */
    void setNumCubes(short int numCubes) { this->numCubes = numCubes; };

    /** @brief Sets the number of colors. */
    void setNumColors(int numColors) {this->numColors = numColors; };

    /** @brief Sets the number of remaining points. */
    void setRemainingPoints(int remainingPoints) { this->remainingPoints = remainingPoints; };

    /** @brief Sets the grains of the structure. */
    void setGrains (std::vector<Coordinate> grains) { this->grains = grains; };

    /** @brief Sets the algorithm flags. */
    void setFlags(const AlgorithmFlags& newFlags) { flags = newFlags; }

    /** @brief Enables or disables animation. */
    void setAnimation(bool value) { flags.isAnimation = value; }

    /** @brief Enables or disables wave generation. */
    void setWaveGeneration(bool value) { flags.isWaveGeneration = value; }

    /** @brief Enables or disables periodic structure. */
    void setPeriodicStructure(bool value) { flags.isPeriodicStructure = value; }

    /** @brief Set if the algorithm is complete or not. */
    void setDone(bool Done) { flags.isDone = Done; };

    /** @brief Returns the number of cubes. */
    short int getNumCubes() { return numCubes; };

    /** @brief Returns the number of colors. */
    int getNumColors() { return numColors; };

    /** @brief Returns the number of remaining points. */
    int getRemainingPoints() { return remainingPoints; };

    /** @brief Returns the number of filled voxels. */
    unsigned int getFilled_Voxels() { return filled_voxels; };

    /** @brief Returns the grains of the structure. */
    std::vector<Coordinate> getGrains() { return grains; };

    /** @brief Returns the pointer to voxels. */
    int32_t*** getVoxels() { return voxels; };

    /** @brief Returns the algorithm flags. */
    AlgorithmFlags getFlags() const { return flags; };

    /** @brief Checks if animation is enabled. */
    bool getAnimation() const { return flags.isAnimation; };

    /** @brief Checks if the algorithm is complete. */
    virtual bool getDone() { if (filled_voxels >= pow(numCubes,3)) { setDone(true); } else { setDone(false); } return flags.isDone; };

    /** @brief Class constructor. */
    Parent_Algorithm();

    /** @brief Class destructor. */
    ~Parent_Algorithm();

    /** @brief Generates the filling of the structure. */
    virtual void Next_Iteration(std::function<void()> callback) = 0;

    /** @brief Generates random starting points in cube. */
    virtual void Initialization(bool isWaveGeneration);

    /** @brief Allocate memory for the initial cube. */
    virtual int32_t*** Allocate_Memory();

    /** @brief Cleans up the data. */
    virtual void CleanUp();

    /**
     * @brief Adds new points to the structure.
     * @param grains Original vector of points.
     * @param numPoints Number of new points.
     * @return Vector of points after addition.
     */
    std::vector<Coordinate> Add_New_Points(std::vector<Coordinate> grains, int numPoints);

    /**
     * @brief Deletes points from the structure.
     * @param grains Original vector of points.
     * @param i Index of the point to be deleted.
     * @return Vector of points after deletion.
     */
    std::vector<Coordinate> Delete_Points(std::vector<Coordinate> grains, size_t i);
};

#endif // PARENT_ALGORITHM_H

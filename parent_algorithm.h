#ifndef PARENT_ALGORITHM_H
#define PARENT_ALGORITHM_H

#include <omp.h>
#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include "paramfield.h"
#include "parameters.h"
#include "grain_analyzer.h"
#include "loadstepmanager.h"
/**
 * @brief Structure containing algorithm flags.
 */
struct AlgorithmFlags {
    bool isAnimation = false;  ///< Animation flag
    bool isWaveGeneration = false; ///< Wave generation flag
    bool isPeriodicStructure = false; ///< Periodic structure flag
    bool isDone = false; ///< Algorithm completion flag
    bool isThinLayer = false; ///< Thin layer flag
};

/**
 * @brief Base algorithm class containing main parameters and methods.
 */
class Parent_Algorithm
{
    friend class LoadStepManager;
    public:
    struct Coordinate;
    template <class T> static T*** Create3D(int N1, int N2, int N3);
    template <class T> static void Delete3D(T*** array);
private:
    void Random_Generate_Points(int currentPoints);
    void Grid_Generate_Points(int currentPoints);
    void Thin_Layer_Generate_Points(int currentPoints);

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
    int total_nucleated_so_far = -1;
    std::mt19937 m_rng; ///< Single generator
    QString m_layerDirection = "+Z"; ///< Thin layer growth direction

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

    /// @brief Vector of structure grains (contains coordinates of all elements belonging to the grains).
    std::vector<Coordinate> grains;

    /// @brief Vector of seed points from which grain growth begins.
    std::vector<Coordinate> seedPoints;

    /**
     * @brief Nucleates a new grain at the specified 3D coordinates.
     * @param x X-coordinate of the nucleation point.
     * @param y Y-coordinate of the nucleation point.
     * @param z Z-coordinate of the nucleation point.
     * @return int32_t The unique identifier (ID) or index of the created grain.
     */
    int32_t birthGrain(int x, int y, int z);

    virtual std::vector<ParamField> paramSchema() const { return {}; }

    /**
     * @brief Generates a random coordinate within the valid modeling domain.
     * @return Coordinate The generated random coordinate.
     */
    Coordinate randomCoord();

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

    /** @brief Enables or disables thin layer structure. */
    void setThinLayer(bool value) { flags.isThinLayer = value; }

    /** @brief Checks if thin layer is enabled. */
    bool getThinLayer() const { return flags.isThinLayer; }

    /** @brief Sets thin layer starting growth direction. */
    void setLayerDirection(const QString& dir) { m_layerDirection = dir; }

    /** @brief Returns thin layer starting growth direction. */
    QString getLayerDirection() const { return m_layerDirection; }

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
    virtual bool getDone() const {
        return filled_voxels >= std::pow(numCubes, 3);
    }

    /** @brief Class constructor. */
    Parent_Algorithm();

    /** @brief Class destructor. */
    virtual ~Parent_Algorithm();

    /** @brief Generates the filling of the structure. */
    virtual void Next_Iteration() = 0;

    virtual void Generate_To_End();

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

    virtual void saveSeeds();
};

#endif // PARENT_ALGORITHM_H

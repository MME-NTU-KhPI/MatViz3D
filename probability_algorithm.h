#ifndef PROBABILITY_ALGORITHM_H
#define PROBABILITY_ALGORITHM_H

#include "parent_algorithm.h"
#include <chrono>
#include <vector>
#include <array>
#include <QString>

struct CrystallizationRecord
{
    unsigned int iteration;            // iteration number
    double       fill_fraction;        // filled_voxels / N_total  [0..1]
    unsigned int captured;             // voxels crystallised this iteration (dN)
    unsigned int cap;                  // thermodynamic cap (N_total / St)
    float        cap_utilization;      // captured / cap  [0..1]
    size_t       frontier_size;        // active boundary cells after growth
    size_t       active_size;          // frontier subset actually processed
    int          nucleated_this_iter;  // new nuclei added by wave nucleation
    int          total_nucleated;      // cumulative nuclei count
};

/**
 * @brief Stochastic cellular automaton with superellipsoid probability kernels,
 *        thermodynamic Stefan growth cap, periodic boundary wrapping, and presets.
 */
class Probability_Algorithm : public Parent_Algorithm
{
public:
    Probability_Algorithm();
    Probability_Algorithm(short int numCubes, int numColors);
    ~Probability_Algorithm() override;

    void Initialization(bool isWaveGeneration) override;
    void Next_Iteration() override;
    bool getDone() const override;
    void CleanUp() override;

    enum class ProbabilityMode {
        VolumeSampling,
        SurfaceFlux
    };

    void processProbabilities(ProbabilityMode mode);
    void calculateVolumeProbabilities();
    void calculateSurfaceFluxProbabilities();

    void setNumCubes(short int numCubes);
    void setNumColors(int numColors);

    const std::vector<CrystallizationRecord>& getHistory() const { return m_history; }
    void clearHistory() { m_history.clear(); }
    void writeHistoryToCSV(const QString& dirPath) const;

private:
    std::chrono::time_point<std::chrono::steady_clock> run_start;
    bool isPointIn(double x, double y, double z);
    void rotatePoint(double& x, double& y, double& z);
    static double toRadians(double degrees);
    double probability[3][3][3]{{{0.0}}};
    int32_t*** m_claimGrid = nullptr;
    std::vector<CrystallizationRecord> m_history;

    void partialShuffle(size_t active_size);
    void recordIteration(unsigned int counter_max,
                         unsigned int cap,
                         unsigned int captured,
                         size_t       active_size,
                         int          nucleated_this_iter,
                         int          total_nucleated);

    unsigned int computeThermodynamicCap(unsigned int counter_max) const;
    unsigned int growFrontier(unsigned int maxCaptures, size_t active_size);
    void fillIsolatedVoxels();
};

#endif // PROBABILITY_ALGORITHM_H

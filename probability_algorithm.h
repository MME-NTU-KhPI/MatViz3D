#ifndef PROBABILITY_ALGORITHM_H
#define PROBABILITY_ALGORITHM_H

#include <QWidget>
#include "parent_algorithm.h"

namespace Ui {
class Probability_Algorithm;
}

struct CrystallizationRecord
{
    unsigned int iteration;            // iteration number
    double       fill_fraction;        // filled_voxels / N_total  [0..1]
    unsigned int captured;             // voxels crystallised this iteration (dN)
    unsigned int cap;                  // thermodynamic cap (N_total / St)
    float        cap_utilization;      // captured / cap  [0..1]
    //   < 1.0 => frontier is the bottleneck
    //   = 1.0 => thermodynamics is the bottleneck
    size_t       frontier_size;        // active boundary cells after growth
    size_t       active_size;          // frontier subset actually processed
    int          nucleated_this_iter;  // new nuclei added by wave nucleation
    int          total_nucleated;      // cumulative nuclei count
};


class Probability_Algorithm : public QWidget, public Parent_Algorithm
{
    Q_OBJECT

public:
    explicit Probability_Algorithm(QWidget *parent = nullptr);
    Probability_Algorithm(short int numCubes, int numColors, QWidget *parent = nullptr);
    ~Probability_Algorithm();

    void setHalfAxis();
    void Next_Iteration(std::function<void()> callback) override;

    enum class ProbabilityMode {
        VolumeSampling,
        SurfaceFlux
    };

    void processProbabilities(ProbabilityMode mode);
    void calculateVolumeProbabilities();
    void calculateSurfaceFluxProbabilities();

    void writeProbabilitiesToCSV(const QString& filePath, uint64_t N);
    void setNumCubes(short int numCubes);
    void setNumColors(int numColors);
    const std::vector<CrystallizationRecord>& getHistory() const
    {
        return m_history;
    }

    void clearHistory() { m_history.clear(); }

    void writeHistoryToCSV(const QString& dirPath) const;


private:
    Ui::Probability_Algorithm *ui;
    void CleanUp() override;
    bool isPointIn(double x,double y,double z);
    void rotatePoint(double& x, double& y, double& z);
    double toRadians(double degress);
    int pointsinvoxel;
    double probability[3][3][3];
    void prettyPrint3DArray(double arr[3][3][3]);
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
    int          nucleateWave(int totalNucleatedSoFar, QString& logInfo);
    void         fillIsolatedVoxels();
    void logIteration(unsigned int counter_max,
                      unsigned int cap,
                      unsigned int captured,
                      size_t       active_size,
                      size_t       frontier_before,
                      double       iter_dt_s,
                      double       elapsed_s,
                      const QString& extra) const;
};

#endif // PROBABILITY_ALGORITHM_H

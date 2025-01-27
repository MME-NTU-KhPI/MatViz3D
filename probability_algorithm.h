#ifndef PROBABILITY_ALGORITHM_H
#define PROBABILITY_ALGORITHM_H

#include <QWidget>
#include "parent_algorithm.h"

namespace Ui {
class Probability_Algorithm;
}

class Probability_Algorithm : public QWidget, public Parent_Algorithm
{
    Q_OBJECT

public:
    explicit Probability_Algorithm(QWidget *parent = nullptr);
    Probability_Algorithm(short int numCubes, int numColors, QWidget *parent = nullptr);
    ~Probability_Algorithm();

    void setHalfAxis();
    void Generate_Filling(bool isAnimation, bool isWaveGeneration, bool isPeriodicStructure);
    void processValues();
    void processValuesGrid();
    void writeProbabilitiesToCSV(const QString& filePath, uint64_t N);
    void setNumCubes(short int numCubes);
    void setNumColors(int numColors);

private:
    Ui::Probability_Algorithm *ui;
    bool isPointIn(double x,double y,double z);
    void rotatePoint(double& x, double& y, double& z);
    double toRadians(double degress);
    int pointsinvoxel;
    float halfaxis_a = 1.5;
    float halfaxis_b = 1.5;
    float halfaxis_c = 1.5;
    float orientation_angle_a = 0;
    float orientation_angle_b = 0;
    float orientation_angle_c = 0;
    double probability[3][3][3];
};

#endif // PROBABILITY_ALGORITHM_H

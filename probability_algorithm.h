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
    float getHalfaxisA() const { return halfaxis_a; }
    float getHalfaxisB() const { return halfaxis_b; }
    float getHalfaxisC() const { return halfaxis_c; }
    float getOrientationAngleA() const { return orientation_angle_a; }
    float getOrientationAngleB() const { return orientation_angle_b; }
    float getOrientationAngleC() const { return orientation_angle_c; }

private:
    Ui::Probability_Algorithm *ui;
    bool isPointIn(double x,double y,double z);
    void rotatePoint(double& x, double& y, double& z);
    double toRadians(double degress);
    int pointsinvoxel;
    float halfaxis_a = 0.0f;
    float halfaxis_b = 0.0f;
    float halfaxis_c = 0.0f;
    float orientation_angle_a = 0.0f;
    float orientation_angle_b = 0.0f;
    float orientation_angle_c = 0.0f;
    double probability[3][3][3];
};

#endif // PROBABILITY_ALGORITHM_H

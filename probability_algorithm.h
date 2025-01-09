#ifndef PROBABILITY_ALGORITHM_H
#define PROBABILITY_ALGORITHM_H

#include <QWidget>
#include "parent_algorithm.h"
#include "ui_probability_algorithm.h"

namespace Ui {
class Probability_Algorithm;
}

class Probability_Algorithm : public QWidget, public Parent_Algorithm
{
    Q_OBJECT

public:
    explicit Probability_Algorithm(QWidget *parent = nullptr);
    ~Probability_Algorithm();
    void setHalfAxis();
    void Generate_Filling(int isAnimation, int isWaveGeneration);
    void processValues();
    void processValuesGrid();
    void writeProbabilitiesToCSV(const QString& filePath, uint64_t N); // Method
    void setNumCubes(short int numCubes);
    void setNumColors(int numColors);
private:
    std::vector<Coordinate> Add_New_Points(const std::vector<Coordinate>& newGrains, int pointsForThisStep);
    Ui::Probability_Algorithm *ui;
    bool isPointIn(double x,double y,double z);
    void rotatePoint(double& x, double& y, double& z);
    double toRadians(double degress);
    int pointsinvoxel;
    float halfaxis_a;
    float halfaxis_b;
    float halfaxis_c;
    float orientation_angle_a;
    float orientation_angle_b;
    float orientation_angle_c;
    double probability[3][3][3];
};

#endif // PROBABILITY_ALGORITHM_H

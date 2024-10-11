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
    ~Probability_Algorithm();
    void setHalfAxis();
    void Generate_Filling(int isAnimation, int isWaveGeneration);
    bool isPointIn(double x,double y,double z);
    void processValues(double probability[3][3][3]);
    void rotatePoint(double& x, double& y, double& z);
    double toRadians(double degress);
    void setNumCubes(short int numCubes);
    void setNumColors(int numColors);
private:
    std::vector<Coordinate> Add_New_Points(const std::vector<Coordinate>& newGrains, int pointsForThisStep);
    Ui::Probability_Algorithm *ui;
    int pointsinvoxel;
    float halfaxis_a = 1;
    float halfaxis_b = 1;
    float halfaxis_c = 1;
    float orientation_angle_a;
    float orientation_angle_b;
    float orientation_angle_c;
};

#endif // PROBABILITY_ALGORITHM_H

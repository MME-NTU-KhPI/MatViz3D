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
    //void Generate_Filling(int isAnimation, int isWaveGeneration);
    bool isPointIn(double x,double y,double z, double radius);
    std::vector<Parent_Algorithm::Coordinate> get_sphere_points(Parent_Algorithm::Coordinate center, int radius);
    void processValues(int CS);
private:
    std::vector<Coordinate> Add_New_Points(const std::vector<Coordinate>& newGrains, int pointsForThisStep);
    Ui::Probability_Algorithm *ui;
    int pointsinvoxel;
    float halfaxis_a;
    float halfaxis_b;
    float halfaxis_c;
    float orintation_angle;
};

#endif // PROBABILITY_ALGORITHM_H

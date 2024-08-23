#include "probability_algorithm.h"
#include "ui_probability_algorithm.h"
#include <random>
#include <omp.h>
#include <QString>
#include <QTextStream>

Probability_Algorithm::Probability_Algorithm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Probability_Algorithm)
{
    ui->setupUi(this);
    connect(ui->applyPushButton,&QPushButton::clicked,this,&Probability_Algorithm::setHalfAxis);
    connect(ui->applyPushButton,&QPushButton::clicked,this,&QWidget::close);
    connect(ui->cancelPushButton,&QPushButton::clicked,this,&QWidget::close);
}

Probability_Algorithm::~Probability_Algorithm()
{
    delete ui;
}

bool Probability_Algorithm::isPointIn(double x,double y,double z, double radius)
{
    return pow(x - radius, 2) / halfaxis_a + pow(y - radius, 2) / halfaxis_b + pow(z - radius, 2) / halfaxis_c <= radius * radius;
}

void Probability_Algorithm::setHalfAxis()
{
    halfaxis_a = ui->axisALineEdit->text().toFloat();
    halfaxis_b = ui->axisBLineEdit->text().toFloat();
    halfaxis_c = ui->axisCLineEdit->text().toFloat();
    orintation_angle = ui->orintationAngleLineEdit->text().toFloat();
}

void Probability_Algorithm::processValues(int CS)
{
    const uint64_t N = 100000000;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(0, CS);

    uint64_t fileld_in[3][3][3] = {{{0}}};
    uint64_t fileld_total[3][3][3] = {{{0}}};

#pragma omp parallel
    {
        int nthreads = omp_get_num_threads();
        uint64_t fileld_in_local[3][3][3] = {{{0}}};
        uint64_t fileld_total_local[3][3][3] = {{{0}}};

        for (uint64_t i = 0; i < N / nthreads; i++)
        {
            double x = dis(gen), y = dis(gen), z = dis(gen);
            int k = (int)floor(x), l = (int)floor(y), m = (int)floor(z);

            fileld_total_local[k][l][m]++;

            if (isPointIn(x, y, z, CS / 2.))
            {
                fileld_in_local[k][l][m]++;
            }
        }

#pragma omp critical
        {
            for (int i = 0; i < CS; i++)
                for (int j = 0; j < CS; j++)
                    for (int k = 0; k < CS; k++)
                    {
                        fileld_in[i][j][k] += fileld_in_local[i][j][k];
                        fileld_total[i][j][k] += fileld_total_local[i][j][k];
                    }
        }
    }
    for (int i = 0; i < CS; i++)
        for (int j = 0; j < CS; j++)
            for (int k = 0; k < CS; k++)
            {
                qDebug() << i << "\t" << j << "\t" << k << "\t"
                       << fileld_in[i][j][k] << "\t" << fileld_total[i][j][k] << "\t"
                       << (double)fileld_in[i][j][k] / fileld_total[i][j][k] << "\n";
            }
}

std::vector<Parent_Algorithm::Coordinate> Probability_Algorithm::Add_New_Points(const std::vector<Coordinate>& newGrains, int pointsForThisStep) {
    std::vector<Coordinate> addedPoints;
    for (int i = 0; i < pointsForThisStep; ++i) {
        Coordinate point;
        do {
            point = {(int16_t)(rand() % numCubes), (int16_t)(rand() % numCubes), (int16_t)(rand() % numCubes)};
        } while (voxels[point.x][point.y][point.z] != 0);
        voxels[point.x][point.y][point.z] = 1;
        addedPoints.push_back(point);
    }
    return addedPoints;
}

/*void Probability_Algorithm::Generate_Filling(int isAnimation, int isWaveGeneration)
{
    srand(time(NULL));
    unsigned int counter_max = std::pow(numCubes, 3);
    while (!grains.empty()) {
        Coordinate temp;
        int16_t x, y, z;
        std::vector<Coordinate> newGrains;
        for (size_t i = 0; i < grains.size(); i++) {
            temp = grains[i];
            x = temp.x;
            y = temp.y;
            z = temp.z;
            float radius = 1.5f;
            std::vector<Coordinate> sphere_points = get_sphere_points(temp, radius);
            for (const auto& sp : sphere_points) {
                int16_t newX = sp.x;
                int16_t newY = sp.y;
                int16_t newZ = sp.z;
                if (voxels[newX][newY][newZ] == 0) {
                    std::vector<Coordinate> neighbors = get_sphere_points({newX, newY, newZ}, 1);
                    int filled_neighbors = std::count_if(neighbors.begin(), neighbors.end(), [&](Coordinate coord) {
                        return voxels[coord.x][coord.y][coord.z] != 0;
                    });
                    double probability = static_cast<double>(filled_neighbors) / neighbors.size();
                    if ((rand() % 100) / 100.0 < probability) {
                        voxels[newX][newY][newZ] = voxels[x][y][z];
                        newGrains.push_back({newX, newY, newZ});
                        counter++;
                    }
                    else
                    {
                        newGrains.push_back({x,y,z});
                    }
                }
            }
        }
        grains = std::move(newGrains);
        IterationNumber++;
        double o = static_cast<double>(counter) / counter_max;
        qDebug().nospace() << o << "\t" << IterationNumber << "\t" << grains.size();
        if (isAnimation == 1) {
            if (isWaveGeneration == 1 && remainingPoints > 0) {
                pointsForThisStep = std::max(1, static_cast<int>(0.1 * remainingPoints));
                newGrains = Add_New_Points(grains, pointsForThisStep);
                grains.insert(grains.end(), newGrains.begin(), newGrains.end());
                remainingPoints -= pointsForThisStep;
            }
            break;
        }
    }
}*/

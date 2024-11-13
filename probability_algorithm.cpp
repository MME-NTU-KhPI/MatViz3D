#include "probability_algorithm.h"
#include "ui_probability_algorithm.h"
#include <random>
#include <cmath>
#include <omp.h>
#include <QString>
#include <QTextStream>
#include <QFile>
#include <QDir>

Probability_Algorithm::Probability_Algorithm(QWidget *parent) :
    QWidget(parent), Parent_Algorithm(),
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

void Probability_Algorithm::setHalfAxis()
{
    halfaxis_a = ui->axisALineEdit->text().toFloat();
    halfaxis_b = ui->axisBLineEdit->text().toFloat();
    halfaxis_c = ui->axisCLineEdit->text().toFloat();
    orintation_angle = ui->orintationAngleLineEdit->text().toFloat();
}

bool Probability_Algorithm::isPointIn(double x, double y, double z)
{
    return (pow((x - 1.5) / halfaxis_a, 2) + pow((y-1.5)  / halfaxis_b, 2) + pow((z-1.5) / halfaxis_c, 2)) <= 1.0;
}

void Probability_Algorithm::setNumCubes(short int size)
{
    numCubes = size;
}

void Probability_Algorithm::setNumColors(int points)
{
    numColors = points;
}


void Probability_Algorithm::processValues(double probability[3][3][3])
{
    const uint64_t N = 1000;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(0, 3);

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
            int k = std::min(std::max((int)floor(x), 0), 2);
            int l = std::min(std::max((int)floor(y), 0), 2);
            int m = std::min(std::max((int)floor(z), 0), 2);

            fileld_total_local[k][l][m]++;

            if (isPointIn(x, y, z))
            {
                fileld_in_local[k][l][m]++;
            }
        }

#pragma omp critical
        {
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    for (int k = 0; k < 3; k++)
                    {
                        fileld_in[i][j][k] += fileld_in_local[i][j][k];
                        fileld_total[i][j][k] += fileld_total_local[i][j][k];
                    }
        }
    }
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++)
            {
                probability[i][j][k] = (double)fileld_in[i][j][k] / fileld_total[i][j][k];
                qDebug() << i << "\t" << j << "\t" << k << "\t"
                         << fileld_in[i][j][k] << "\t" << fileld_total[i][j][k] << "\t"
                         << (double)fileld_in[i][j][k] / fileld_total[i][j][k] << "\n";
            }

    // Call the function to save the probabilities to a CSV file
    writeProbabilitiesToCSV(probability, "D:/Project(MatViz3D)/fall2024/", N);
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

void Probability_Algorithm::Generate_Filling(int isAnimation, int isWaveGeneration)
{
    double probability[3][3][3];
    processValues(probability);
    srand(time(NULL));
    unsigned int counter_max = pow(numCubes,3);
    while (!grains.empty())
    {
        Coordinate temp;
        int16_t x,y,z;
        std::vector<Coordinate> newGrains;
        for(size_t i = 0; i < grains.size(); i++)
        {
            temp = grains[i];
            x = temp.x;
            y = temp.y;
            z = temp.z;
            for (int16_t k = -1; k < 2; k++)
            {
                for(int16_t p = -1; p < 2; p++)
                {
                    for(int16_t l = -1; l < 2; l++)
                    {
                        int16_t newX = k+x;
                        int16_t newY = p+y;
                        int16_t newZ = l+z;
                        bool isValidXYZ = (newX >= 0 && newX < numCubes) && (newY >= 0 && newY < numCubes) && (newZ >= 0 && newZ < numCubes) && voxels[newX][newY][newZ] == 0;
                        std::random_device rd;
                        std::mt19937 gen(rd());
                        std::uniform_real_distribution<> dis(0.0, 1.0);
                        if (isValidXYZ)
                        {
                            if(dis(gen) >= probability[1+k][1+p][1+l])
                            {
                                voxels[newX][newY][newZ] = voxels[x][y][z];
                                newGrains.push_back({newX,newY,newZ});
                                counter++;
                            }
                            else
                            {
                                newGrains.push_back({x,y,z});
                            }
                        }
                    }
                }
            }
        }
        grains = std::move(newGrains);
        IterationNumber++;
        double o = (double)counter/counter_max;
        qDebug().nospace() << o << "\t" << IterationNumber << "\t" << grains.size();
        if (isAnimation == 1)
        {
            if (isWaveGeneration == 1)
            {
                if (remainingPoints > 0)
                {
                    pointsForThisStep = std::max(1, static_cast<int>(Parameters::wave_coefficient * remainingPoints));
                    newGrains = Add_New_Points(newGrains,pointsForThisStep);
                    grains.insert(grains.end(), newGrains.begin(), newGrains.end());
                    remainingPoints -= pointsForThisStep;
                }
            }
            break;
        }
    }
}


// Method to write the probabilities to a CSV file
void Probability_Algorithm::writeProbabilitiesToCSV(double probability[3][3][3], const QString& filePath, uint64_t N)
{
    // Створення імені файлу з додаванням значення N
    QString fileName = filePath + QDir::separator() + "N_" + QString::number(N) + ".csv";
    QFile file(fileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "Помилка відкриття файлу для запису ймовірностей.";
        return;
    }

    QTextStream out(&file);

    // Запис заголовка
    out << "X,Y,Z,Probability\n";

    // Запис значень ймовірностей
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                out << i << "," << j << "," << k << "," << probability[i][j][k] << "\n";
            }
        }
    }

    file.close();
}

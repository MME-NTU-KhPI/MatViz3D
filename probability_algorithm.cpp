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
    orientation_angle_a = ui->orintationAngleLineEdit->text().toFloat();
    orientation_angle_b = ui->lineEdit->text().toFloat();
    orientation_angle_c = ui->lineEdit_2->text().toFloat();
}

bool Probability_Algorithm::isPointIn(double x, double y, double z)
{
    rotatePoint(x,y,z);
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

double Probability_Algorithm::toRadians(double degrees)
{
    return degrees * (M_PI/180) ;
}

const std::array<std::array<int32_t, 3>, 26> PROBABILITY_OFFSETS = {{
    {-1, -1, -1}, {-1, -1, 0}, {-1, -1, 1},
    {-1, 0, -1},  {-1, 0, 0},  {-1, 0, 1},
    {-1, 1, -1},  {-1, 1, 0},  {-1, 1, 1},
    {0, -1, -1},  {0, -1, 0},  {0, -1, 1},
    {0, 0, -1},                {0, 0, 1},
    {0, 1, -1},   {0, 1, 0},   {0, 1, 1},
    {1, -1, -1},  {1, -1, 0},  {1, -1, 1},
    {1, 0, -1},   {1, 0, 0},   {1, 0, 1},
    {1, 1, -1},   {1, 1, 0},   {1, 1, 1}
}};

void Probability_Algorithm::rotatePoint(double& x, double& y, double& z)
{
    double Rx[3][3] = {
        {1, 0, 0},
        {0, cos(toRadians(orientation_angle_a)), -sin(toRadians(orientation_angle_a))},
        {0, sin(toRadians(orientation_angle_a)), cos(toRadians(orientation_angle_a))}
    };

    double Ry[3][3] = {
        {cos(toRadians(orientation_angle_b)), 0, sin(toRadians(orientation_angle_b))},
        {0, 1, 0},
        {-sin(toRadians(orientation_angle_b)), 0, cos(toRadians(orientation_angle_b))}
    };

    double Rz[3][3] = {
        {cos(toRadians(orientation_angle_c)), -sin(toRadians(orientation_angle_c)), 0},
        {sin(toRadians(orientation_angle_c)), cos(toRadians(orientation_angle_c)), 0},
        {0, 0, 1}
    };
    double x1 = Rx[0][0] * x + Rx[0][1] * y + Rx[0][2] * z;
    double y1 = Rx[1][0] * x + Rx[1][1] * y + Rx[1][2] * z;
    double z1 = Rx[2][0] * x + Rx[2][1] * y + Rx[2][2] * z;

    double x2 = Ry[0][0] * x1 + Ry[0][1] * y1 + Ry[0][2] * z1;
    double y2 = Ry[1][0] * x1 + Ry[1][1] * y1 + Ry[1][2] * z1;
    double z2 = Ry[2][0] * x1 + Ry[2][1] * y1 + Ry[2][2] * z1;

    double x3 = Rz[0][0] * x2 + Rz[0][1] * y2 + Rz[0][2] * z2;
    double y3 = Rz[1][0] * x2 + Rz[1][1] * y2 + Rz[1][2] * z2;
    double z3 = Rz[2][0] * x2 + Rz[2][1] * y2 + Rz[2][2] * z2;

    x = x3;
    y = y3;
    z = z3;
}

void Probability_Algorithm::processValues()
{
    const uint64_t N = 1000000;
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
    writeProbabilitiesToCSV("D:/Project(MatViz3D)/fall2024/", N);
}

void Probability_Algorithm::processValuesGrid()
{
    const uint64_t N = 1000000;
    uint64_t n = std::round(std::cbrt(N));

    double step = 3.0 / (n - 1);

    uint64_t fileld_in[3][3][3] = {{{0}}};
    uint64_t fileld_total[3][3][3] = {{{0}}};
    uint64_t fileld_in_local[3][3][3] = {{{0}}};
    uint64_t fileld_total_local[3][3][3] = {{{0}}};

    for (uint64_t i = 0; i < n; ++i)
    {
        for (uint64_t j = 0; j < n; ++j)
        {
            for (uint64_t k = 0; k < n; ++k)
            {
                double x = i * step;
                double y = j * step;
                double z = k * step;

                int k_voxel = (int)floor(x);
                int l_voxel = (int)floor(y);
                int m_voxel = (int)floor(z);

                if( k_voxel >= 0 && k_voxel <= 2)
                {
                    if( l_voxel >= 0 && l_voxel <= 2)
                    {
                        if( m_voxel >= 0 && m_voxel <= 2)
                        {
                            fileld_total_local[k_voxel][l_voxel][m_voxel]++;
                        }
                    }
                }
                if (isPointIn(x, y, z))
                {
                    fileld_in_local[k_voxel][l_voxel][m_voxel]++;
                }
            }
        }
    }
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++)
            {
                fileld_in[i][j][k] += fileld_in_local[i][j][k];
                fileld_total[i][j][k] += fileld_total_local[i][j][k];
            }

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                if (fileld_total[i][j][k] > 0)
                {
                    probability[i][j][k] = (double)fileld_in[i][j][k] / fileld_total[i][j][k];
                }
                else
                {
                    probability[i][j][k] = 0.0;
                }

                qDebug() << i << "\t" << j << "\t" << k << "\t"
                         << fileld_in[i][j][k] << "\t" << fileld_total[i][j][k] << "\t"
                         << probability[i][j][k] << "\n";
            }
        }
    }
    writeProbabilitiesToCSV("D:/Project(MatViz3D)/fall2024/", N);
}

void Probability_Algorithm::Generate_Filling(int isAnimation, int isWaveGeneration, int isPeriodicStructure)
{
    omp_set_num_threads(omp_get_max_threads());
    const unsigned int counter_max = static_cast<unsigned int>(pow(numCubes, 3));
    auto start = std::chrono::high_resolution_clock::now();

    std::random_device rd;
    std::mt19937 global_gen(rd());
    std::uniform_real_distribution<> global_dis(0.0, 1.0);

    while (!grains.empty())
    {
        const size_t current_size = grains.size();
        std::vector<Coordinate> newGrains;
        newGrains.reserve(current_size * 26);

        std::atomic<unsigned int> local_counter(0);

        #pragma omp parallel
        {
            std::vector<Coordinate> privateGrains;
            privateGrains.reserve(current_size * 26 / omp_get_num_threads());

            std::mt19937 local_gen(global_gen());
            std::uniform_real_distribution<> local_dis(0.0, 1.0);

            #pragma omp for schedule(guided) nowait
            for (size_t i = 0; i < current_size; i++)
            {
                const Coordinate& temp = grains[i];
                const int16_t x = temp.x, y = temp.y, z = temp.z;
                const int32_t current_value = voxels[x][y][z];

                for (const auto& offset : PROBABILITY_OFFSETS)
                {
                    int16_t newX = x + offset[0];
                    int16_t newY = y + offset[1];
                    int16_t newZ = z + offset[2];

                    if (newX < 0 || newX >= numCubes ||
                        newY < 0 || newY >= numCubes ||
                        newZ < 0 || newZ >= numCubes)
                        continue;

                    if (isPeriodicStructure == 1)
                    {
                        newX = (newX + numCubes) % numCubes;
                        newY = (newY + numCubes) % numCubes;
                        newZ = (newZ + numCubes) % numCubes;
                    }

                    if (voxels[newX][newY][newZ] == 0 && local_dis(local_gen) >= probability[1 + offset[0]][1 + offset[1]][1 + offset[2]])
                    {
                        if (voxels[newX][newY][newZ] == 0 &&
                            __sync_bool_compare_and_swap(&voxels[newX][newY][newZ], 0, current_value))
                        {
                            privateGrains.emplace_back(Coordinate{newX, newY, newZ});
                            local_counter.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                }
            }

            #pragma omp critical
            {
                newGrains.insert(newGrains.end(),
                                 std::make_move_iterator(privateGrains.begin()),
                                 std::make_move_iterator(privateGrains.end()));
            }
        }

        counter += local_counter.load(std::memory_order_relaxed);
        grains = std::move(newGrains);
        IterationNumber++;

        double progress = static_cast<double>(counter) / counter_max;
        qDebug().nospace() << progress << "\t" << IterationNumber << "\t" << grains.size();

        if (isAnimation == 1)
        {
            if (isWaveGeneration == 1 && remainingPoints > 0)
            {
                pointsForThisStep = std::max(1, static_cast<int>(0.1 * remainingPoints));
                auto newPoints = Add_New_Points(grains, pointsForThisStep);
                grains.insert(grains.end(), newPoints.begin(), newPoints.end());
                remainingPoints -= pointsForThisStep;
            }
            break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    qDebug() << "Algorithm execution time: " << duration.count() << " seconds";
}

void Probability_Algorithm::writeProbabilitiesToCSV(const QString& filePath, uint64_t N)
{
    QString fileName = filePath + QDir::separator() + "N_" + QString::number(N) + ".csv";
    QFile file(fileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "Помилка відкриття файлу для запису ймовірностей.";
        return;
    }

    QTextStream out(&file);

    out << "X,Y,Z,Probability\n";

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

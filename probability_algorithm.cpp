#include "probability_algorithm.h"
#include "ui_probability_algorithm.h"
#include "parameters.h"
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

Probability_Algorithm::Probability_Algorithm(short int numCubes, int numColors, QWidget *parent)
    : QWidget(parent), ui(new Ui::Probability_Algorithm)
{
    ui->setupUi(this);
    connect(ui->applyPushButton,&QPushButton::clicked,this,&Probability_Algorithm::setHalfAxis);
    connect(ui->applyPushButton,&QPushButton::clicked,this,&QWidget::close);
    connect(ui->cancelPushButton,&QPushButton::clicked,this,&QWidget::close);

    setNumCubes(numCubes);
    setNumColors(numColors);
    processValuesGrid();
}

Probability_Algorithm::~Probability_Algorithm()
{
    delete ui;
}

void Probability_Algorithm::setHalfAxis()
{
    Parameters::halfaxis_a = ui->axisALineEdit->text().toFloat();
    Parameters::halfaxis_b = ui->axisBLineEdit->text().toFloat();
    Parameters::halfaxis_c = ui->axisCLineEdit->text().toFloat();
    Parameters::orientation_angle_a = ui->orintationAngleLineEdit->text().toFloat();
    Parameters::orientation_angle_b = ui->lineEdit->text().toFloat();
    Parameters::orientation_angle_c = ui->lineEdit_2->text().toFloat();
}

bool Probability_Algorithm::isPointIn(double x, double y, double z)
{
    rotatePoint(x,y,z);
    return (pow(std::abs(x - 1.5) / Parameters::halfaxis_a, Parameters::ellipse_order)
            + pow(std::abs(y-1.5)  / Parameters::halfaxis_b, Parameters::ellipse_order)
            + pow(std::abs(z-1.5) / Parameters::halfaxis_c, Parameters::ellipse_order)) <= 1.0;
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
        {0, cos(toRadians(Parameters::orientation_angle_a)), -sin(toRadians(Parameters::orientation_angle_a))},
        {0, sin(toRadians(Parameters::orientation_angle_a)), cos(toRadians(Parameters::orientation_angle_a))}
    };

    double Ry[3][3] = {
        {cos(toRadians(Parameters::orientation_angle_b)), 0, sin(toRadians(Parameters::orientation_angle_b))},
        {0, 1, 0},
        {-sin(toRadians(Parameters::orientation_angle_b)), 0, cos(toRadians(Parameters::orientation_angle_b))}
    };

    double Rz[3][3] = {
        {cos(toRadians(Parameters::orientation_angle_c)), -sin(toRadians(Parameters::orientation_angle_c)), 0},
        {sin(toRadians(Parameters::orientation_angle_c)), cos(toRadians(Parameters::orientation_angle_c)), 0},
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
    std::mt19937 gen(Parameters::seed);
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
    writeProbabilitiesToCSV(QCoreApplication::applicationDirPath(), N);
}

void Probability_Algorithm::processValuesGrid()
{
    const uint64_t N = pow(102, 3);
    const uint64_t n = std::round(std::cbrt(N));

    const double step = 3.0 / n;

    const double num_points_per_voxel = pow(1.0 / step, 3);

    qDebug() << "Runing probabilty estimation on grid algorithm" << Qt::endl
             << "  Total points N = " << N << Qt::endl
             << "  Points on endge n = " << n << Qt::endl
             << "  Step = " << step << Qt::endl
             << "  Number of points per one voxel = " << num_points_per_voxel << Qt::endl
             << "  h_a = " << Parameters::halfaxis_a << Qt::endl
             << "  h_b = " << Parameters::halfaxis_b << Qt::endl
             << "  h_c = " << Parameters::halfaxis_c << Qt::endl
             << "  ellipse_order = " << Parameters::ellipse_order << Qt::endl
             << "  angle_a = " << Parameters::orientation_angle_a << Qt::endl
             << "  angle_b = " << Parameters::orientation_angle_b << Qt::endl
             << "  angle_c = " << Parameters::orientation_angle_c << Qt::endl
        ;

    uint64_t fileld_in_local[3][3][3] = {{{0}}};


    for (uint64_t i = 0; i < n; ++i)
    {
        for (uint64_t j = 0; j < n; ++j)
        {
            for (uint64_t k = 0; k < n; ++k)
            {
                double x = (i + 0.5) * step;
                double y = (j + 0.5) * step;
                double z = (k + 0.5) * step;

                int k_voxel = (int)floor(x);
                int l_voxel = (int)floor(y);
                int m_voxel = (int)floor(z);

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
                this->probability[i][j][k] = (double)fileld_in_local[i][j][k] / num_points_per_voxel;

    prettyPrint3DArray(this->probability);
    writeProbabilitiesToCSV(QCoreApplication::applicationDirPath(), N);
}

void Probability_Algorithm::prettyPrint3DArray(double arr[3][3][3])
{
    QString output;

    // Header
    output += "Probability Array [3][3][3]:\n";
    output += "==================\n";

    // Print the 3D array
    for (int i = 0; i < 3; i++) {
        output += QString("Layer %1:\n").arg(i);
        for (int j = 0; j < 3; j++) {
            output += "  [";
            for (int k = 0; k < 3; k++) {
                output += QString("%1").arg(arr[i][j][k], 5, 'f', 3);
                if (k < 2) output += ", ";
            }
            output += "]\n";
        }
        if (i < 2) output += "\n";
    }

    // Calculate directional probabilities
    output += "\n==================\n";
    output += "Directional Probabilities:\n";
    output += "==================\n";

    output += QString("+X direction: %1\n").arg(arr[2][1][1], 6, 'f', 3);
    output += QString("-X direction: %1\n").arg(arr[0][1][1], 6, 'f', 3);
    output += QString("+Y direction: %1\n").arg(arr[1][2][1], 6, 'f', 3);
    output += QString("-Y direction: %1\n").arg(arr[1][0][1], 6, 'f', 3);
    output += QString("+Z direction: %1\n").arg(arr[1][1][2], 6, 'f', 3);
    output += QString("-Z direction: %1\n").arg(arr[1][1][0], 6, 'f', 3);

    // Single qDebug call at the end
    qDebug().noquote() << output;
}

void Probability_Algorithm::Next_Iteration(std::function<void()> callback)
{
    const unsigned int counter_max = pow(numCubes, 3);

    while (!grains.empty())
    {
        const size_t current_size = grains.size();
        std::vector<Coordinate> newGrains;
        newGrains.reserve(current_size * 2);

        unsigned int local_counter = 0;

        #pragma omp parallel reduction(+:local_counter)
        {
            std::vector<Coordinate> privateGrains;
            privateGrains.reserve(current_size * 2 / omp_get_max_threads());

            std::mt19937 local_gen(Parameters::seed + omp_get_thread_num());
            std::uniform_real_distribution<> local_dis(0.0, 1.0);

            #pragma omp for schedule(guided) nowait
            for (size_t i = 0; i < current_size; i++)
            {
                const Coordinate& temp = grains[i];
                const int32_t x = temp.x, y = temp.y, z = temp.z;
                const int32_t current_value = voxels[x][y][z];

                bool keep_this_grain_active = false;

                for (const auto& offset : PROBABILITY_OFFSETS)
                {
                    int32_t newX = x + offset[0];
                    int32_t newY = y + offset[1];
                    int32_t newZ = z + offset[2];

                    if (flags.isPeriodicStructure == 1)
                    {
                        newX = (newX + numCubes) % numCubes;
                        newY = (newY + numCubes) % numCubes;
                        newZ = (newZ + numCubes) % numCubes;
                    }

                    bool out_of_box_cond = newX < 0 || newY < 0 || newZ < 0 ||
                                           newX >= numCubes || newY >= numCubes || newZ >= numCubes;

                    if (out_of_box_cond) continue;

                    if (voxels[newX][newY][newZ] != 0) continue;

                    double prob = probability[1 + offset[0]][1 + offset[1]][1 + offset[2]];

                    if (local_dis(local_gen) < prob)
                    {
                        int num_tryies = 5;
                        bool captured = false;
                        while(num_tryies--) {
                            if (__sync_bool_compare_and_swap(&voxels[newX][newY][newZ], 0, current_value)) {
                                captured = true;
                                break;
                            }
                            if (voxels[newX][newY][newZ] != 0) break;
                        }

                        if (captured)
                        {
                            privateGrains.push_back({newX, newY, newZ});
                            local_counter++;
                        }
                    }
                    else
                    {
                        keep_this_grain_active = true;
                    }
                }

                if (keep_this_grain_active)
                {
                    privateGrains.push_back({x, y, z});
                }
            }

            #pragma omp critical
            {
                newGrains.insert(newGrains.end(),
                                 std::make_move_iterator(privateGrains.begin()),
                                 std::make_move_iterator(privateGrains.end()));
            }
        }
        filled_voxels += local_counter;
        grains = std::move(newGrains);
        IterationNumber++;

        double progress = static_cast<double>(filled_voxels) / counter_max;
        //qDebug().nospace() << progress << "\t" << IterationNumber << "\t" << grains.size();
        qDebug().noquote() << " Iteration number:" << IterationNumber
                           << " Progress:" << QString("%1").arg(progress * 100, 5, 'f', 2)
                           << " Size of Grain vector:" << grains.size();

        if (flags.isWaveGeneration && remainingPoints > 0)
        {
            pointsForThisStep = std::max(1, static_cast<int>(std::round(Parameters::wave_coefficient * remainingPoints)));
            newGrains = Add_New_Points(newGrains, pointsForThisStep);
            grains.insert(grains.end(), newGrains.begin(), newGrains.end());
            remainingPoints -= pointsForThisStep;
        }

        if (flags.isAnimation)
        {
            callback();
        }
    }

    if (grains.empty() && filled_voxels != counter_max)
    {
        for (int x = 0; x < numCubes; ++x)
            for (int y = 0; y < numCubes; ++y)
                for (int z = 0; z < numCubes; ++z)
                {
                    if (voxels[x][y][z] != 0) continue;

                    for (const auto& offset : PROBABILITY_OFFSETS)
                    {
                        int nx = x + offset[0];
                        int ny = y + offset[1];
                        int nz = z + offset[2];

                        if (nx < 0 || nx >= numCubes || ny < 0 || ny >= numCubes || nz < 0 || nz >= numCubes)
                            continue;

                        if (voxels[nx][ny][nz] == 0)
                            continue;

                        voxels[x][y][z] = voxels[nx][ny][nz];
                        filled_voxels++;
                        qDebug() << "Voxel filled at:" << x << y << z << "with color:" << voxels[x][y][z];
                        break;
                    }
                }
    }
}

void Probability_Algorithm::writeProbabilitiesToCSV(const QString& filePath, uint64_t N)
{
    QString tempFileName = filePath + QDir::separator() + "temp_N_" + QString::number(N) + ".csv";
    QFile tempFile(tempFileName);

    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "Error opening a temporary file for writing.";
        return;
    }

    QTextStream out(&tempFile);
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

    tempFile.close();

    QString finalFileName = filePath + QDir::separator() + "N_" + QString::number(N) + ".csv";

    QFile::remove(finalFileName);

    if (QFile::rename(tempFileName, finalFileName))
    {
        qDebug() << "The file has been successfully written to: " << finalFileName;
    }
    else
    {
        qDebug() << "Temporary file renaming error.";
    }
}

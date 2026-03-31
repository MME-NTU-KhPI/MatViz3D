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
    // Translate to origin first
    double cx = x - 1.5;
    double cy = y - 1.5;
    double cz = z - 1.5;
    // Rotate the centered offset, not the raw point
    rotatePoint(cx, cy, cz);
    return (pow(std::abs(cx) / Parameters::halfaxis_a, Parameters::ellipse_order)
            + pow(std::abs(cy) / Parameters::halfaxis_b, Parameters::ellipse_order)
            + pow(std::abs(cz) / Parameters::halfaxis_c, Parameters::ellipse_order)) <= 1.0;
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
    std::uniform_real_distribution<double> dis(0, 3);

    uint64_t fileld_in[3][3][3]    = {{{0}}};
    uint64_t fileld_total[3][3][3] = {{{0}}};

#pragma omp parallel
    {
        // Each thread owns its own generator — no data race
        std::mt19937 local_gen(Parameters::seed + static_cast<uint64_t>(omp_get_thread_num()));
        int nthreads = omp_get_num_threads();
        uint64_t fileld_in_local[3][3][3]    = {{{0}}};
        uint64_t fileld_total_local[3][3][3] = {{{0}}};

        for (uint64_t i = 0; i < N / nthreads; i++)
        {
            double x = dis(local_gen), y = dis(local_gen), z = dis(local_gen);
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
                        fileld_in[i][j][k]    += fileld_in_local[i][j][k];
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
    const uint64_t N_surface = 500000;

    const double a = Parameters::halfaxis_a;
    const double b = Parameters::halfaxis_b;
    const double c = Parameters::halfaxis_c;
    const double p = Parameters::ellipse_order;   // superellipsoid exponent

    // Accumulated Lambertian flux for each of the 26 neighbours
    double flux[3][3][3] = {{{0.0}}};

    // Seeded RNG for reproducible results
    std::mt19937_64 rng(Parameters::seed);
    std::uniform_real_distribution<double> uni(-1.0, 1.0);

    for (uint64_t s = 0; s < N_surface; ++s)
    {
        // --- Step 1: draw a uniform random direction u on the unit sphere ---
        double ux, uy, uz, len2;
        do {
            ux = uni(rng); uy = uni(rng); uz = uni(rng);
            len2 = ux*ux + uy*uy + uz*uz;
        } while (len2 < 1e-12 || len2 > 1.0);   // rejection sampling inside unit ball
        double inv_len = 1.0 / std::sqrt(len2);
        ux *= inv_len; uy *= inv_len; uz *= inv_len;

        // --- Step 2: find the surface point along u ---
        // |t*ux/a|^p + |t*uy/b|^p + |t*uz/c|^p = 1
        // t^p * (|ux/a|^p + ...) = 1  =>  t = (...)^(-1/p)
        double sum_p = pow(std::abs(ux / a), p)
                       + pow(std::abs(uy / b), p)
                       + pow(std::abs(uz / c), p);
        if (sum_p < 1e-30) continue;
        double t = pow(sum_p, -1.0 / p);
        double sx = t * ux;
        double sy = t * uy;
        double sz = t * uz;

        // --- Step 3: compute the outward normal at (sx, sy, sz) ---
        // Gradient of F = |x/a|^p + |y/b|^p + |z/c|^p  is:
        //   dF/dx = (p/a) * |x/a|^(p-1) * sign(x)
        // The 1/a factor is critical for non-unit axes.
        double nx = (p / a) * pow(std::abs(sx / a), p - 1.0) * (sx >= 0 ? 1.0 : -1.0);
        double ny = (p / b) * pow(std::abs(sy / b), p - 1.0) * (sy >= 0 ? 1.0 : -1.0);
        double nz = (p / c) * pow(std::abs(sz / c), p - 1.0) * (sz >= 0 ? 1.0 : -1.0);

        // Normalise the gradient to get the unit outward normal
        double nlen = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (nlen < 1e-30) continue;
        nx /= nlen; ny /= nlen; nz /= nlen;

        // --- Step 4: apply grain orientation rotation to the normal ---
        // Rotating the normal is equivalent to rotating the whole ellipsoid.
        // (Bug 1 is already fixed in isPointIn; here we replicate the same
        //  convention: rotate the *vector in grain frame* to lab frame.)
        rotatePoint(nx, ny, nz);

        // --- Step 5: accumulate flux onto each of the 26 neighbour directions ---
        for (const auto& offset : PROBABILITY_OFFSETS)
        {
            // Normalised direction toward this neighbour
            double dx = offset[0], dy = offset[1], dz = offset[2];
            double dlen = std::sqrt(dx*dx + dy*dy + dz*dz);
            dx /= dlen; dy /= dlen; dz /= dlen;

            double dot = nx*dx + ny*dy + nz*dz;
            if (dot > 0.0)
            {
                // +1 offset maps to index 0,1,2 — same convention as before
                flux[1 + offset[0]][1 + offset[1]][1 + offset[2]] += dot;
            }
        }
    }

    // --- Step 6: normalise — strongest direction gets probability 1.0 ---
    double maxFlux = 0.0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++)
                if (flux[i][j][k] > maxFlux) maxFlux = flux[i][j][k];

    if (maxFlux < 1e-30) maxFlux = 1.0;   // safety guard for degenerate cases

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++)
                this->probability[i][j][k] = flux[i][j][k] / maxFlux;

    // Centre voxel (self) is never a capture target
    this->probability[1][1][1] = 0.0;

    prettyPrint3DArray(this->probability);
    writeProbabilitiesToCSV(QCoreApplication::applicationDirPath(), N_surface);
}

void Probability_Algorithm::prettyPrint3DArray(double arr[3][3][3])
{
    QString output;

    output += "Probability Array [3][3][3]:\n";
    output += "==================\n";

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

    output += "\n==================\n";
    output += "Directional Probabilities:\n";
    output += "==================\n";

    output += QString("+X direction: %1\n").arg(arr[2][1][1], 6, 'f', 3);
    output += QString("-X direction: %1\n").arg(arr[0][1][1], 6, 'f', 3);
    output += QString("+Y direction: %1\n").arg(arr[1][2][1], 6, 'f', 3);
    output += QString("-Y direction: %1\n").arg(arr[1][0][1], 6, 'f', 3);
    output += QString("+Z direction: %1\n").arg(arr[1][1][2], 6, 'f', 3);
    output += QString("-Z direction: %1\n").arg(arr[1][1][0], 6, 'f', 3);

    qDebug().noquote() << output;
}

void Probability_Algorithm::Next_Iteration(std::function<void()> callback)
{
    const unsigned int counter_max = pow(numCubes, 3);

    const int N_gr = numColors;
    int total_nucleated_so_far = static_cast<int>(grains.size());

    std::mt19937 wave_rng(Parameters::seed ^ 0xDEADBEEFULL);
    std::uniform_int_distribution<int> cube_dis(0, numCubes - 1);

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

                    if (prob < 1e-12) continue;   // skip zero-prob directions entirely

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
                        // Only keep active when a non-trivial roll actually failed
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

        QString nuclLogInfo = "";

        if (flags.isWaveGeneration && total_nucleated_so_far < N_gr && filled_voxels < counter_max)
        {
            double cumulative_fraction = 0.0;
            int wave_contribution = 0;

            if (IterationNumber > 1)
            {
                double arg = (IterationNumber - (Parameters::wave_coefficient / 2.0)) / (Parameters::wave_spread * std::sqrt(2.0));
                cumulative_fraction = 0.5 * (1.0 + std::erf(arg));

                int remaining_to_nucleate = N_gr - Parameters::initial_nuclei_count;

                wave_contribution = static_cast<int>(std::floor(cumulative_fraction * remaining_to_nucleate));
            }

            int total_should_be_now = Parameters::initial_nuclei_count + wave_contribution;

            int pointsToCreate = total_should_be_now - total_nucleated_so_far;

            if (pointsToCreate > 0)
            {
                int placedRandomly = 0;

                for (int p = 0; p < pointsToCreate; ++p) {
                    bool success = false;
                    for (int retry = 0; retry < 10; ++retry) {
                        int rx = cube_dis(wave_rng);
                        int ry = cube_dis(wave_rng);
                        int rz = cube_dis(wave_rng);

                        if (voxels[rx][ry][rz] == 0) {
                            voxels[rx][ry][rz] = ++color;
                            grains.push_back({rx, ry, rz});
                            filled_voxels++;
                            success = true;
                            placedRandomly++;
                            break;
                        }
                    }
                    if (!success) break;
                }

                int leftToPlace = pointsToCreate - placedRandomly;

                if (leftToPlace > 0) {
                    grains = Add_New_Points(grains, leftToPlace);
                }

                total_nucleated_so_far += pointsToCreate;
            }
            nuclLogInfo = QString(" | [Nucl] N(n): %1 | Added: %2 | Tot: %3")
                              .arg(cumulative_fraction, -8, 'f', 4)
                              .arg(pointsToCreate, -6)
                              .arg(total_nucleated_so_far, -6);
        }

        double o = static_cast<double>(filled_voxels) / counter_max;
        QString logLine = QString("%1 %2 %3")
                              .arg(o, -12, 'g', 6)
                              .arg(IterationNumber, -6)
                              .arg((int)grains.size(), -10);

        if (!nuclLogInfo.isEmpty()) {
            logLine += nuclLogInfo;
        }

        qDebug().noquote() << logLine;

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

#include "statistics.h"
#include "ui_statistics.h"
#include <QtCharts>
#include <QVector>
#include <algorithm>
#include <vector>
#include <QSet>
#include <cmath>


Statistics::Statistics(QWidget *parent)
    : QWidget(parent), ui(new Ui::Statistics)
{
    ui->setupUi(this);
}

//Statistics::Statistics(int16_t*** voxels, int numCubes, QWidget *parent)
//    : QWidget(parent), ui(new Ui::Statistics)
//{
//    ui->setupUi(this);

//    setVoxelCounts(voxels, numCubes);
//}


Statistics::~Statistics()
{
    delete ui;
}

//Functions for 3D properties

void Statistics::surfaceArea3D(int16_t ***voxels, int numCubes) {
    #pragma omp parallel
    {
        std::map<int, int> local_surface_area;

    #pragma omp for nowait // Распределение работы по z без ожидания
        for (int z = 0; z < numCubes; ++z) {
            for (int y = 0; y < numCubes; ++y) {
                for (int x = 0; x < numCubes; ++x) {
                    int current = voxels[z][y][x];
                    if (current != 0) {
                        int face_count = 0;
                        face_count += (x == 0 || voxels[z][y][x-1] != current) ? 1 : 0;
                        face_count += (x == numCubes-1 || voxels[z][y][x+1] != current) ? 1 : 0;
                        face_count += (y == 0 || voxels[z][y-1][x] != current) ? 1 : 0;
                        face_count += (y == numCubes-1 || voxels[z][y+1][x] != current) ? 1 : 0;
                        face_count += (z == 0 || voxels[z-1][y][x] != current) ? 1 : 0;
                        face_count += (z == numCubes-1 || voxels[z+1][y][x] != current) ? 1 : 0;

                        local_surface_area[current] += face_count;
                    }
                }
            }
        }

        // Объединение локальных данных в глобальную карту
    #pragma omp critical
        for (auto &entry : local_surface_area) {
            surface_area_3D[entry.first] += entry.second;
        }
    }

    for (const auto& pair : surface_area_3D) {
        qDebug() << "Grain ID " << pair.first << " has a surface area of " << pair.second << " square units.\n";
    }
}

void Statistics::calcVolume3D(int16_t ***voxels, int numCubes) {
    #pragma omp parallel
    {
        std::map<int, int> local_volume_counts; // Локальная карта для каждого потока
        #pragma omp for nowait // Распределяем циклы по потокам
            for (int z = 0; z < numCubes; ++z) {
                for (int y = 0; y < numCubes; ++y) {
                    for (int x = 0; x < numCubes; ++x) {
                        int grain_id = voxels[z][y][x];
                        if (grain_id > 0) {
                            local_volume_counts[grain_id]++;
                        }
                    }
                }
            }
        // Синхронизируем локальные карты с глобальной картой
        #pragma omp critical
            for (const auto& pair : local_volume_counts) {
                volume_3D[pair.first] += pair.second;
            }
    }
    for (const auto& pair : volume_3D) {
        qDebug() << "Grain ID " << pair.first << " has a volume of " << pair.second << " voxels.\n";
    }
}

void Statistics::calcNormVolume3D()
{
    double volume_scale;
    for (const auto& pair : volume_3D)
    {
        volume_scale = pair.second;
        if (pair.second > volume_scale)
        {
                volume_scale = pair.second;
        }
    }
    for (const auto& pair : volume_3D)
    {
        norm_volume_3D[pair.first] = pair.second / volume_scale;
        qDebug() << "Grain ID " << pair.first << " has a norm volume of " << pair.second << " voxels.\n";
    }
}

void Statistics::calcESR()
{
    for (const auto& pair : volume_3D)
    {
        double esr = std::pow((3.0 / (4.0 * M_PI)) * pair.second, 1.0 / 3.0);
        ESR_3D[pair.first] = esr;
        qDebug() << "Grain ID " << pair.first << " has a ESR of " << pair.second << " voxels.\n";
    }
}

void Statistics::calcMomentInertia()
{
    for (const auto& pair : ESR_3D)
    {
        double momentinertia = (2/5)*1*std::pow(pair.second,2);
        moment_inertia_3D[pair.first] = momentinertia;
        qDebug() << "Grain ID " << pair.first << " has a moment of inertia of " << pair.second << " voxels.\n";
    }
}

//Functions for 2D properties


void Statistics::setVoxelCounts(int16_t*** voxels, int numCubes)
{
    // Отримання розмірів масиву
    int sizeX = numCubes;
    int sizeY = numCubes;
    int sizeZ = numCubes;

    // Розділення масиву на шари по координаті Z
    for (int z = 0; z < sizeZ; ++z) {
        // Вивід номеру шару
        qDebug() << "Layer" << z;

        // Вивід значень шару
        for (int y = 0; y < sizeY; ++y) {
            QString row;
            for (int x = 0; x < sizeX; ++x) {
                // Додавання числа до рядка
                row.append(QString::number(voxels[x][y][z]) + " ");
            }
            // Вивід рядка в QDebug
            qDebug() << row;
        }

        // Розділювач між шарами
        qDebug() << "-------------------------";
    }
}














//void Statistics::setVoxelCounts(const QVector<int>& counts)
//{
//    // Створення нового Area Chart
//    QAreaSeries *series = new QAreaSeries();

//    QList<int> valuesList(counts.begin(), counts.end());

//    int minValue = *std::min_element(valuesList.constBegin(), valuesList.constEnd());
//    int maxValue = *std::max_element(valuesList.constBegin(), valuesList.constEnd());

//    int numberOfBins = static_cast<int>(std::round(std::sqrt(valuesList.size())));

//    // Визначення розміру біна (ширини стовпця)
//    double binSize = static_cast<double>(maxValue - minValue) / numberOfBins;

//    QVector<int> binCounts(numberOfBins, 0);
//    for (int value : valuesList) {
//        int binIndex = qFloor((value - minValue) / binSize);
//        if (binIndex >= 0 && binIndex < numberOfBins) {
//            binCounts[binIndex]++;
//        }
//    }

//    int maxBinCount = *std::max_element(binCounts.constBegin(), binCounts.constEnd());
//    int maxAxisValue = (maxBinCount + 9) / 10 * 10; // Заокруглення вгору до ближчого кратного 10

//    // Додавання точок до Area Chart
//    QVector<QPointF> lowerPoints;
//    QVector<QPointF> upperPoints;
//    for (int i = 0; i < numberOfBins; ++i) {
//        int binStart = qRound(minValue + i * binSize);
//        int binEnd = qRound(minValue + (i + 1) * binSize);
//        lowerPoints.append(QPointF(binStart, 0));
//        lowerPoints.append(QPointF(binEnd, 0));
//        upperPoints.append(QPointF(binStart, binCounts[i]));
//        upperPoints.append(QPointF(binEnd, binCounts[i]));
//    }

//    // Встановлення точок для нижньої та верхньої межі області
//    QLineSeries *lowerSeries = new QLineSeries();
//    QLineSeries *upperSeries = new QLineSeries();
//    lowerSeries->append(lowerPoints);
//    upperSeries->append(upperPoints);
//    series->setLowerSeries(lowerSeries);
//    series->setUpperSeries(upperSeries);

//    // Встановлення кольорів графіки та фону графіку
//    QBrush areaBrush(QColor(0, 86, 77, 128)); // Колір графіки з напівпрозорістю
//    QPen areaPen(QColor(0, 86, 77, 255)); // Колір обводки графіки з напівпрозорістю
//    areaPen.setWidth(2); // Ширина обводки

//    series->setBrush(areaBrush);
//    series->setPen(areaPen);

//    // Створення графіка та налаштування властивостей
//    QChart *chart = new QChart();
//    chart->addSeries(series);
//    chart->setTitle(""); // Заголовок гістограми
//    chart->setAnimationOptions(QChart::SeriesAnimations);

//    // Зміна кольорів фону, сітки та вісей
//    chart->setBackgroundBrush(QColor(54, 54, 54)); // Колір фону
//    chart->setPlotAreaBackgroundBrush(QColor(54, 54, 54)); // Колір фону області графіку
//    chart->setPlotAreaBackgroundVisible(true);

//    QValueAxis *axisX = new QValueAxis();
//    axisX->setLabelsColor(QColor(150, 150, 150)); // Колір міток на осі X
//    axisX->setLinePenColor(QColor(150, 150, 150)); // Колір лінії осі X
//    axisX->setGridLineColor(QColor(150, 150, 150, 77)); // Колір сітки осі X з напівпрозорістю

//    chart->addAxis(axisX, Qt::AlignBottom);
//    lowerSeries->attachAxis(axisX);

//    // Отримуємо мінімальне та максимальне значення для відображення на осі X
//    int tickMinValue = qRound(minValue - binSize);
//    int tickMaxValue = qRound(maxValue + binSize);

//    // мінімальне значення на осі X не менше нуля
//    tickMinValue = qMax(0, tickMinValue);

//    // Кількість міток на вісі X
//    int tickCount = qMin(10, numberOfBins); // Задаємо максимальну кількість міток, наприклад, 10
//    axisX->setTickCount(tickCount);

//    // Функція для генерації тексту мітки вісі X
//    axisX->setLabelFormat("%d"); // Відображення цілих чисел

//    // Встановлюємо мінімальне та максимальне значення осі X
//    axisX->setMin(tickMinValue);
//    axisX->setMax(tickMaxValue);

//    // Налаштування осі Y
//    QValueAxis *axisY = new QValueAxis();
//    axisY->setLabelFormat("%d"); // Формат міток на осі Y (цілі числа)
//    axisY->setLinePenColor(QColor(150, 150, 150)); // Колір лінії осі Y
//    axisY->setGridLineColor(QColor(150, 150, 150, 77)); // Колір сітки осі Y з напівпрозорістю
//    axisY->setRange(0, maxAxisValue + 10);
//    axisY->setLabelsColor(QColor(150, 150, 150)); // Колір міток на осі Y

//    chart->addAxis(axisY, Qt::AlignLeft);
//    lowerSeries->attachAxis(axisY);

//    // Приховати легенду (підпис "Counts")
//    chart->legend()->hide();

//    // Очистити horizontalFrame
//    QLayoutItem *item;
//    while ((item = ui->horizontalFrame->layout()->takeAt(0)) != nullptr) {
//        delete item->widget();
//        delete item;
//    }

//    // Відображення графіка
//    QChartView *chartView = new QChartView(chart);
//    chartView->setRenderHint(QPainter::Antialiasing);
//    ui->horizontalFrame->layout()->addWidget(chartView);

//    // Вивід кількості значень у векторі counts
//    qDebug() << "Counts vector size:" << counts.size();

//    // Вивід кількості унікальних значень у векторі counts
//    qDebug() << "Unique counts values:" << valuesList.size();

//    // Вивід унікальних значень і їх кількості
//    QSet<int> uniqueValuesSet(valuesList.begin(), valuesList.end());
//    QList<int> uniqueValues = uniqueValuesSet.values();
//    std::sort(uniqueValues.begin(), uniqueValues.end());
//    for (int value : uniqueValues) {
//        int count = std::count(valuesList.begin(), valuesList.end(), value);
//        qDebug() << "Value:" << value << ", Count:" << count;
//    }
//}

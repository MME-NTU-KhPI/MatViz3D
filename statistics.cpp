#include "statistics.h"
#include "ui_statistics.h"

#include <QtCharts>
#include <QVector>
#include <algorithm>
#include <vector>
#include <QSet>
#include <cmath>
#include <vector>
#include <queue>
#include <unordered_map>


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






//Functions for 2D properties


//void Statistics::setVoxelCounts(int16_t*** voxels, int numCubes)
//{
//    // Отримання розмірів масиву
//    int sizeX = numCubes;
//    int sizeY = numCubes;
//    int sizeZ = numCubes;

//    // Розділення масиву на шари по координаті Z
//    for (int z = 0; z < sizeZ; ++z) {
//        // Вивід номеру шару
//        qDebug() << "Layer" << z;

//        // Вивід значень шару
//        for (int y = 0; y < sizeY; ++y) {
//            QString row;
//            for (int x = 0; x < sizeX; ++x) {
//                // Додавання числа до рядка
//                row.append(QString::number(voxels[x][y][z]) + " ");
//            }
//            // Вивід рядка в QDebug
//            qDebug() << row;
//        }

//        // Розділювач між шарами
//        qDebug() << "-------------------------";
//    }
//}




// Структура для представлення координати пікселя
struct Point {
    int x, y;
    Point(int _x, int _y) : x(_x), y(_y) {}
};

// Структура для представлення об'єкта
struct Object {
    int label;
    int size;
    Object(int _label, int _size) : label(_label), size(_size) {}
};

std::vector<Object> label_connected_regions(const std::vector<std::vector<int>>& image) {
    int rows = image.size();
    int cols = image[0].size();
    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false)); // Масив для відстеження відвіданих пікселів
    std::unordered_map<int, int> grainAreas; // Мапа для зберігання площі кожного зерна

    std::vector<Object> objects; // Вектор для зберігання об'єктів

    // Проходження по зображенню
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (image[i][j] && !visited[i][j]) { // Якщо поточний піксель належить об'єкту та ще не відвіданий
                int size = 0; // Ініціалізуємо розмір об'єкту
                int color = image[i][j]; // Колір поточного зерна
                std::queue<Point> q; // Черга для обходу об'єкту

                q.push(Point(i, j)); // Додаємо поточний піксель у чергу
                visited[i][j] = true; // Позначаємо його як відвіданий

                // Обхід об'єкту в ширину
                while (!q.empty()) {
                    Point p = q.front();
                    q.pop();
                    size++; // Збільшуємо розмір об'єкту
                    // Перевірка сусідніх пікселів
                    std::vector<Point> neighbors = {{p.x - 1, p.y}, {p.x + 1, p.y}, {p.x, p.y - 1}, {p.x, p.y + 1}};
                    for (const auto& neighbor : neighbors) {
                        int ni = neighbor.x;
                        int nj = neighbor.y;
                        if (ni >= 0 && ni < rows && nj >= 0 && nj < cols && image[ni][nj] == color && !visited[ni][nj]) {
                            q.push(neighbor);
                            visited[ni][nj] = true;
                        }
                    }
                }

                // Зберігаємо площу кожного зерна за його кольором
                grainAreas[color] += size;
            }
        }
    }

    // Додаємо знайдені зерна до вектора об'єктів
    for (const auto& pair : grainAreas) {
        objects.emplace_back(pair.first, pair.second);
    }

    return objects;
}

void Statistics::setVoxelCounts(int16_t*** voxels, int numCubes)
{
    // Отримання розмірів масиву
    int sizeX = numCubes;
    int sizeY = numCubes;
    int sizeZ = numCubes;

    // Створення масиву для зберігання площі кожного зерна на кожному шарі
    std::vector<std::unordered_map<int, int>> grainAreasByLayer(sizeZ);

    // Виконання обробки кожного шару
    for (int z = 0; z < sizeZ; ++z) {
        // Створення масиву для поточного шару
        std::vector<std::vector<int>> layerImage(sizeY, std::vector<int>(sizeX));

        // Заповнення масиву значеннями з масиву voxels
        for (int y = 0; y < sizeY; ++y) {
            for (int x = 0; x < sizeX; ++x) {
                // Просто копіюємо значення з масиву voxels
                layerImage[y][x] = voxels[x][y][z];
            }
        }

        // Вивід поточного шару у консоль для перевірки
        qDebug() << "Layer" << z << "Contents:";
        for (int y = 0; y < sizeY; ++y) {
            QString line;
            for (int x = 0; x < sizeX; ++x) {
                line += QString::number(layerImage[y][x]) + " ";
            }
            qDebug() << line;
        }

        // Застосування функції маркування з'єднаних областей для поточного шару
        std::vector<Object> objects = label_connected_regions(layerImage);

        // Вивід площі кожного нового зерна у консоль
        for (const auto& obj : objects) {
            // Перевірка, чи мітка об'єкта вже була розглянута на попередніх шарах
            if (grainAreasByLayer[z].find(obj.label) == grainAreasByLayer[z].end()) {
                // Якщо мітка об'єкта нова для поточного шару, виводимо її площу
                qDebug() << "Layer" << z << ", Grain" << obj.label << "Area:" << obj.size;
                // Додаємо мітку та площу до мапи поточного шару
                grainAreasByLayer[z][obj.label] = obj.size;
            }
        }
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

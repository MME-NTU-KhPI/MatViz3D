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

Statistics::Statistics(const QVector<int>& colorCounts, QWidget *parent)
    : QWidget(parent), ui(new Ui::Statistics), m_colorCounts(colorCounts)
{
    ui->setupUi(this);

    setVoxelCounts(m_colorCounts);
}

Statistics::~Statistics()
{
    delete ui;
}

//Functions for 3D properties






//Functions for 2D properties

void Statistics::setVoxelCounts(const QVector<int>& counts)
{

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

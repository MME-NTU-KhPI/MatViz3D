#include "statistics.h"
#include "ui_statistics.h"

#include <QVector>
#include <algorithm>
#include <vector>


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


void Statistics::setVoxelCounts(const QVector<int>& counts)
{
    // Створення нової стовпчастої діаграми
    QBarSeries *series = new QBarSeries();

    // Створення категорій для стовпчиків
    QMap<int, int> frequencyMap;
    for (int i = 0; i < counts.size(); i++) {
        int voxelCount = counts[i];
        frequencyMap[voxelCount]++;
    }

    // Отримання мінімального та максимального значення категорій
    int minValue = *std::min_element(frequencyMap.constBegin(), frequencyMap.constEnd());
    int maxValue = *std::max_element(frequencyMap.constBegin(), frequencyMap.constEnd());

    // Створення повного списку категорій
    QStringList categories;
    for (int i = minValue; i <= maxValue; i++) {
        categories << QString::number(i);
    }

    // Додавання стовпчиків до серії
    QBarSet *barSet = new QBarSet("Counts");
    for (int i = minValue; i <= maxValue; i++) {
        *barSet << frequencyMap.value(i, 0);
    }
    series->append(barSet);

    // Створення діаграми та налаштування властивостей
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(""); // Прибрати заголовок діаграми
    chart->setAnimationOptions(QChart::SeriesAnimations);

    // Встановлення ширини стовпчика
    qreal value = 0.8; // Значення від 0.0 до 1.0, де 1.0 - повна ширина
    series->setBarWidth(value);

    // Налаштування осі X з категоріями
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->setCategories(categories);
    axisX->setRange(categories.first(), categories.last());
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    // Налаштування осі Y
    QValueAxis *axisY = new QValueAxis();
    axisY->setLabelFormat("%i"); // Формат міток на осі Y

    // Знаходження максимального значення
    int maxCount = 0;
    for (int i = 0; i < barSet->count(); i++) {
        int value = barSet->at(i);
        if (value > maxCount) {
            maxCount = value;
        }
    }

    axisY->setRange(0, maxCount); // Встановити діапазон осі Y
    axisY->setLabelsVisible(true); // Відображати значення на осі Y

    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // Приховати легенду (підпис "Counts")
    chart->legend()->hide();

    // Очистити horizontalFrame
    QLayoutItem *item;
    while ((item = ui->horizontalFrame->layout()->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    // Відображення діаграми
    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    ui->horizontalFrame->layout()->addWidget(chartView);

    // Виведемо кількість значень у векторі counts
    qDebug() << "Counts vector size:" << counts.size();

    // Виведемо кількість унікальних значень у векторі counts
    qDebug() << "Unique counts values:" << frequencyMap.size();

    // Виведемо значення і їх кількість
    for (auto it = frequencyMap.constBegin(); it != frequencyMap.constEnd(); ++it) {
        qDebug() << "Value:" << it.key() << ", Count:" << it.value();
    }
}











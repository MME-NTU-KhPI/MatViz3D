#include "statistics.h"
#include "ui_statistics.h"

#include <QVector>
#include <algorithm>
#include <vector>
#include <QSet>

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
    QList<int> valuesList(counts.begin(), counts.end());
    int minValue = *std::min_element(valuesList.constBegin(), valuesList.constEnd());
    int maxValue = *std::max_element(valuesList.constBegin(), valuesList.constEnd());

    // Додавання стовпчиків до серії
    QBarSet *barSet = new QBarSet("Counts");
    for (int i = minValue; i <= maxValue; i++) {
        int count = std::count(valuesList.begin(), valuesList.end(), i);
        *barSet << count; // Змінюємо тут, щоб додати значення в стовпчик
    }
    series->append(barSet);

    // Виведення унікальних значень і їх кількості
    QSet<int> uniqueValuesSet(valuesList.begin(), valuesList.end());
    QList<int> uniqueValues = uniqueValuesSet.values();
    std::sort(uniqueValues.begin(), uniqueValues.end()); // Впорядкування у порядку зростання

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
    QStringList categories;
    for (int i = minValue; i <= maxValue; i++) {
        categories << QString::number(i);
    }
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    // Налаштування осі Y
    QValueAxis *axisY = new QValueAxis();
    axisY->setLabelFormat("%i"); // Формат міток на осі Y

    // Знаходимо максимальне значення серед усіх стовпчиків
    int maxCount = 0;
    const QList<QBarSet*> barSets = series->barSets();
    if (!barSets.isEmpty()) {
        const QBarSet *set = barSets.at(0);
        for (int i = 0; i < set->count(); i++) {
            maxCount = std::max(maxCount, static_cast<int>(set->at(i)));
        }
    }

    // Збільшуємо максимальне значення на 10% для запасу
    axisY->setRange(0, maxCount /*+ maxCount * 0.1*/);

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
    qDebug() << "Unique counts values:" << uniqueValues.size();

    // Виведемо значення і їх кількість
    for (int value : uniqueValues) {
        int count = std::count(valuesList.begin(), valuesList.end(), value);
        qDebug() << "Value:" << value << ", Count:" << count;
    }
}


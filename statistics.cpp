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


//_________________________________________________________

//void Statistics::setVoxelCounts(const QVector<int>& counts)
//{
//    // Створення нової стовпчастої діаграми
//    QBarSeries *series = new QBarSeries();

//    // Створення категорій для стовпчиків
//    QList<int> valuesList(counts.begin(), counts.end());
//    int minValue = *std::min_element(valuesList.constBegin(), valuesList.constEnd());
//    int maxValue = *std::max_element(valuesList.constBegin(), valuesList.constEnd());

//    // Додавання стовпчиків до серії
//    QBarSet *barSet = new QBarSet("Counts");
//    for (int i = minValue; i <= maxValue; i++) {
//        int count = std::count(valuesList.begin(), valuesList.end(), i);
//        *barSet << count;
//    }
//    series->append(barSet);

//    // Виведення унікальних значень і їх кількості
//    QSet<int> uniqueValuesSet(valuesList.begin(), valuesList.end());
//    QList<int> uniqueValues = uniqueValuesSet.values();
//    std::sort(uniqueValues.begin(), uniqueValues.end()); // Впорядкування у порядку зростання

//    // Створення діаграми та налаштування властивостей
//    QChart *chart = new QChart();
//    chart->addSeries(series);
//    chart->setTitle(""); // Заголовок діаграми
//    chart->setAnimationOptions(QChart::SeriesAnimations);

//    // Встановлення ширини стовпчика
//    qreal value = 0.8; // Значення від 0.0 до 1.0, де 1.0 - повна ширина
//    series->setBarWidth(value);

//    // Налаштування осі X з категоріями
//    QBarCategoryAxis *axisX = new QBarCategoryAxis();
//    QStringList categories;
//    for (int i = minValue; i <= maxValue; i++) {
//        categories << QString::number(i);
//    }
//    axisX->append(categories);
//    chart->addAxis(axisX, Qt::AlignBottom);
//    series->attachAxis(axisX);

//    // Налаштування осі Y
//    QValueAxis *axisY = new QValueAxis();
//    axisY->setLabelFormat("%i"); // Формат міток на осі Y

//    // Знаходимо максимальне значення серед усіх стовпчиків
//    int maxCount = 0;
//    const QList<QBarSet*> barSets = series->barSets();
//    if (!barSets.isEmpty()) {
//        const QBarSet *set = barSets.at(0);
//        for (int i = 0; i < set->count(); i++) {
//            maxCount = std::max(maxCount, static_cast<int>(set->at(i)));
//        }
//    }

//    // Збільшити максимальне значення на 10% для запасу
//    axisY->setRange(0, maxCount /*+ maxCount * 0.1*/);

//    chart->addAxis(axisY, Qt::AlignLeft);
//    series->attachAxis(axisY);

//    // Приховати легенду (підпис "Counts")
//    chart->legend()->hide();

//    // Очистити horizontalFrame
//    QLayoutItem *item;
//    while ((item = ui->horizontalFrame->layout()->takeAt(0)) != nullptr) {
//        delete item->widget();
//        delete item;
//    }

//    // Відображення діаграми
//    QChartView *chartView = new QChartView(chart);
//    chartView->setRenderHint(QPainter::Antialiasing);
//    ui->horizontalFrame->layout()->addWidget(chartView);

//    // Вивід кількості значень у векторі counts
//    qDebug() << "Counts vector size:" << counts.size();

//    // Вивід кількості унікальних значень у векторі counts
//    qDebug() << "Unique counts values:" << uniqueValues.size();

//    // Вивід значеннь і їх кількість
//    for (int value : uniqueValues) {
//        int count = std::count(valuesList.begin(), valuesList.end(), value);
//        qDebug() << "Value:" << value << ", Count:" << count;
//    }
//}

//_________________________________________________________

//________________________1________________________________

//void Statistics::setVoxelCounts(const QVector<int>& counts)
//{
//    // Створення нової гістограми
//    QBarSeries *series = new QBarSeries();

//    // Створення категорій для стовпчиків
//    QList<int> valuesList(counts.begin(), counts.end());
//    int minValue = *std::min_element(valuesList.constBegin(), valuesList.constEnd());
//    int maxValue = *std::max_element(valuesList.constBegin(), valuesList.constEnd());
//    int numberOfBins = qSqrt(counts.size()); // Кількість бінів (стовпчиків) у гістограмі

//    // Визначення розміру біна (ширини стовпця)
//    double binSize = static_cast<double>(maxValue - minValue) / numberOfBins;

//    // Додавання стовпчиків до серії
//    QVector<int> binCounts(numberOfBins, 0);
//    for (int value : valuesList) {
//        int binIndex = (value - minValue) / binSize;
//        if (binIndex >= 0 && binIndex < numberOfBins) {
//            binCounts[binIndex]++;
//        }
//    }

//    int maxBinCount = *std::max_element(binCounts.constBegin(), binCounts.constEnd());
//    int maxAxisValue = (maxBinCount + 9) / 10 * 10; // Заокруглення вгору до ближчого кратного 10

//    QBarSet *barSet = new QBarSet("Counts");
//    for (int count : binCounts) {
//        barSet->append(count);
//    }
//    series->append(barSet);

//    // Створення гістограми та налаштування властивостей
//    QChart *chart = new QChart();
//    chart->addSeries(series);
//    chart->setTitle(""); // Заголовок гістограми
//    chart->setAnimationOptions(QChart::SeriesAnimations);

//    // Встановлення ширини стовпчика
//    qreal value = 1.0; // Значення від 0.0 до 1.0, де 1.0 - повна ширина
//    series->setBarWidth(value);

//    // Налаштування осі X
//    QBarCategoryAxis *axisX = new QBarCategoryAxis();
//    QStringList categories; // Список категорій (міток) для осі X
//    for (int i = 0; i < numberOfBins; ++i) {
//        int binStart = qRound(minValue + i * binSize);
//        int binEnd = qRound(minValue + (i + 1) * binSize);
//        categories.append(QString("%1 - %2").arg(binStart).arg(binEnd));
//    }
//    axisX->append(categories);

//    chart->addAxis(axisX, Qt::AlignBottom);
//    series->attachAxis(axisX);

//    // Налаштування осі Y для відображення кількості входжень
//    QValueAxis *axisY = new QValueAxis();
//    axisY->setLabelFormat("%d"); // Формат міток на осі Y (цілі числа)
//    axisY->setRange(0, maxAxisValue);
//    chart->addAxis(axisY, Qt::AlignLeft);
//    series->attachAxis(axisY);

//    // Приховати легенду (підпис "Counts")
//    chart->legend()->hide();

//    // Очистити horizontalFrame
//    QLayoutItem *item;
//    while ((item = ui->horizontalFrame->layout()->takeAt(0)) != nullptr) {
//        delete item->widget();
//        delete item;
//    }

//    // Відображення гістограми
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

//    // Вивід значень і їх кількості
//    for (int i = 0; i < numberOfBins; ++i) {
//        int binStart = qRound(minValue + i * binSize);
//        int binEnd = qRound(minValue + (i + 1) * binSize);
//        int count = binCounts[i];
//        qDebug() << QString("%1 - %2").arg(binStart).arg(binEnd) << ", Count:" << count;
//    }
//}

//_________________________________________________________


//__________________________2.0______________________________


//void Statistics::setVoxelCounts(const QVector<int>& counts)
//{
//    // Створення нової гістограми
//    QBarSeries *series = new QBarSeries();

//    // Створення категорій для стовпчиків
//    QList<int> valuesList(counts.begin(), counts.end());
//    int minValue = *std::min_element(valuesList.constBegin(), valuesList.constEnd());
//    int maxValue = *std::max_element(valuesList.constBegin(), valuesList.constEnd());
//    int numberOfBins = qSqrt(counts.size()); // Кількість бінів (стовпчиків) у гістограмі

//    // Визначення розміру біна (ширини стовпця) та інтервалу між стовпцями
//    double binSize = static_cast<double>(maxValue - minValue) / numberOfBins;
//    double intervalSize = binSize * 0.1;

//    // Додавання стовпців до серії
//    QVector<int> binCounts(numberOfBins, 0);
//    for (int value : valuesList) {
//        int binIndex = (value - minValue) / (binSize + intervalSize);
//        if (binIndex >= 0 && binIndex < numberOfBins) {
//            binCounts[binIndex]++;
//        }
//    }

//    // Розрахунок ширини стовпців
//    qreal columnWidth = binSize / (binSize + intervalSize); // Коефіцієнт ширини стовпців

//    int maxBinCount = *std::max_element(binCounts.constBegin(), binCounts.constEnd());
//    int maxAxisValue = (maxBinCount + 9) / 10 * 10; // Заокруглення вгору до ближчого кратного 10

//    QBarSet *barSet = new QBarSet("Counts");
//    for (int count : binCounts) {
//        barSet->append(count);
//    }
//    series->append(barSet);

//    // Створення гістограми та налаштування властивостей
//    QChart *chart = new QChart();
//    chart->addSeries(series);
//    chart->setTitle(""); // Заголовок гістограми
//    chart->setAnimationOptions(QChart::SeriesAnimations);

//    // Налаштування осі X
//    QValueAxis *axisX = new QValueAxis();
//    chart->addAxis(axisX, Qt::AlignBottom);
//    series->attachAxis(axisX);

//    int tickCount = numberOfBins + 1; // Кількість міток на шкалі
//    axisX->setTickCount(tickCount);

//    double tickInterval = static_cast<double>(maxValue - minValue) / numberOfBins;
//    axisX->setTickInterval(tickInterval);

//    // Функція для генерації тексту мітки вісі X
//    axisX->setLabelFormat("%d"); // Відображення цілих чисел

//    // Встановлюємо мінімальне та максимальне значення осі X
//    axisX->setMin(minValue);
//    axisX->setMax(maxValue);

//    // Розрахунок ширини стовпців
//    series->setBarWidth(columnWidth);

//    // Налаштування осі Y для відображення кількості входжень
//    QValueAxis *axisY = new QValueAxis();
//    axisY->setLabelFormat("%d"); // Формат міток на осі Y (цілі числа)
//    axisY->setRange(0, maxAxisValue);
//    chart->addAxis(axisY, Qt::AlignLeft);
//    series->attachAxis(axisY);

//    // Приховати легенду (підпис "Counts")
//    chart->legend()->hide();

//    // Очистити horizontalFrame
//    QLayoutItem *item;
//    while ((item = ui->horizontalFrame->layout()->takeAt(0)) != nullptr) {
//        delete item->widget();
//        delete item;
//    }

//    // Відображення гістограми
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

//    // Вивід значень і їх кількості
//    for (int i = 0; i < numberOfBins; ++i) {
//        int binStart = qRound(minValue + i * binSize);
//        int binEnd = qRound(minValue + (i + 1) * binSize);
//        int count = binCounts[i];
//        qDebug() << QString("%1 - %2").arg(binStart).arg(binEnd) << ", Count:" << count;
//    }
//}



//________________________2.1________________________________


//void Statistics::setVoxelCounts(const QVector<int>& counts)
//{
//    // Створення нової гістограми
//    QBarSeries *series = new QBarSeries();

//    // Створення категорій для стовпчиків
//    QList<int> valuesList(counts.begin(), counts.end());
//    int minValue = *std::min_element(valuesList.constBegin(), valuesList.constEnd());
//    int maxValue = *std::max_element(valuesList.constBegin(), valuesList.constEnd());
//    int numberOfBins = qSqrt(counts.size()); // Кількість бінів (стовпчиків) у гістограмі

//    // Визначення розміру біна (ширини стовпця)
//    double binSize = static_cast<double>(maxValue - minValue) / numberOfBins;

//    // Додавання стовпців до серії
//    QVector<int> binCounts(numberOfBins, 0);
//    for (int value : valuesList) {
//        int binIndex = (value - minValue) / binSize;
//        if (binIndex >= 0 && binIndex < numberOfBins) {
//            binCounts[binIndex]++;
//        }
//    }

//    // Розрахунок ширини стовпців
//    qreal columnWidth = binSize; // Кожен стовпець відповідає своєму інтервалу

//    int maxBinCount = *std::max_element(binCounts.constBegin(), binCounts.constEnd());
//    int maxAxisValue = (maxBinCount + 9) / 10 * 10; // Заокруглення вгору до ближчого кратного 10

//    QBarSet *barSet = new QBarSet("Counts");
//    for (int count : binCounts) {
//        barSet->append(count);
//    }
//    series->append(barSet);

//    // Створення гістограми та налаштування властивостей
//    QChart *chart = new QChart();
//    chart->addSeries(series);
//    chart->setTitle(""); // Заголовок гістограми
//    chart->setAnimationOptions(QChart::SeriesAnimations);

//    // Налаштування осі X
//    QValueAxis *axisX = new QValueAxis();
//    chart->addAxis(axisX, Qt::AlignBottom);
//    series->attachAxis(axisX);

//    int tickCount = numberOfBins; // Кількість міток на шкалі (по одній на кожен інтервал)
//    axisX->setTickCount(tickCount);

//    // Функція для генерації тексту мітки вісі X
//    axisX->setLabelFormat("%d"); // Відображення цілих чисел

//    // Встановлюємо мінімальне та максимальне значення осі X
//    axisX->setMin(minValue);
//    axisX->setMax(maxValue);


//    // Розрахунок ширини стовпців
//    series->setBarWidth(columnWidth);

//    // Налаштування осі Y для відображення кількості входжень
//    QValueAxis *axisY = new QValueAxis();
//    axisY->setLabelFormat("%d"); // Формат міток на осі Y (цілі числа)
//    axisY->setRange(0, maxAxisValue);
//    chart->addAxis(axisY, Qt::AlignLeft);
//    series->attachAxis(axisY);

//    // Приховати легенду (підпис "Counts")
//    chart->legend()->hide();

//    // Очистити horizontalFrame
//    QLayoutItem *item;
//    while ((item = ui->horizontalFrame->layout()->takeAt(0)) != nullptr) {
//        delete item->widget();
//        delete item;
//    }

//    // Відображення гістограми
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

//    // Вивід значень і їх кількості
//    for (int i = 0; i < numberOfBins; ++i) {
//        int binStart = qRound(minValue + i * binSize);
//        int binEnd = qRound(minValue + (i + 1) * binSize);
//        int count = binCounts[i];
//        qDebug() << QString("%1 - %2").arg(binStart).arg(binEnd) << ", Count:" << count;
//    }
//}


//_________________________________________________________


//__________________________3______________________________

//void Statistics::setVoxelCounts(const QVector<int>& counts)
//{
//    // Створення нового Line Chart
//    QLineSeries *series = new QLineSeries();

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

//    // Додавання точок до Line Chart
//    QVector<QPointF> points;
//    for (int i = 0; i < numberOfBins; ++i) {
//        int binStart = qRound(minValue + i * binSize);
//        int binEnd = qRound(minValue + (i + 1) * binSize);
//        points.append(QPointF(binStart, binCounts[i]));
//        points.append(QPointF(binEnd, binCounts[i]));
//    }

//    // Встановлення точок до Line Chart
//    for (const QPointF &point : points) {
//        series->append(point);
//    }

//    // Створення графіка та налаштування властивостей
//    QChart *chart = new QChart();
//    chart->addSeries(series);
//    chart->setTitle(""); // Заголовок гістограми
//    chart->setAnimationOptions(QChart::SeriesAnimations);

//    // Налаштування осі X
//    QValueAxis *axisX = new QValueAxis();
//    chart->addAxis(axisX, Qt::AlignBottom);
//    series->attachAxis(axisX);

//    int tickCount = numberOfBins; // Кількість міток на шкалі
//    axisX->setTickCount(tickCount);

//    // Функція для генерації тексту мітки вісі X
//    axisX->setLabelFormat("%.0f"); // Відображення цілих чисел

//    // Встановлюємо мінімальне та максимальне значення осі X
//    axisX->setMin(minValue);
//    axisX->setMax(maxValue);

//    // Налаштування осі Y для відображення кількості входжень
//    QValueAxis *axisY = new QValueAxis();
//    axisY->setLabelFormat("%d"); // Формат міток на осі Y (цілі числа)
//    axisY->setRange(0, maxAxisValue);
//    chart->addAxis(axisY, Qt::AlignLeft);
//    series->attachAxis(axisY);

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

//    // Вивід значень і їх кількості
//    for (int i = 0; i < numberOfBins; ++i) {
//        int binStart = qRound(minValue + i * binSize);
//        int binEnd = qRound(minValue + (i + 1) * binSize);
//        int count = binCounts[i];
//        qDebug() << QString("%1 - %2").arg(binStart).arg(binEnd) << ", Count:" << count;
//    }
//}


//_________________________________________________________


//__________________________4______________________________

void Statistics::setVoxelCounts(const QVector<int>& counts)
{
    // Створення нового Area Chart
    QAreaSeries *series = new QAreaSeries();

    QList<int> valuesList(counts.begin(), counts.end());

    int minValue = *std::min_element(valuesList.constBegin(), valuesList.constEnd());
    int maxValue = *std::max_element(valuesList.constBegin(), valuesList.constEnd());

    int numberOfBins = static_cast<int>(std::round(std::sqrt(valuesList.size())));

    // Визначення розміру біна (ширини стовпця)
    double binSize = static_cast<double>(maxValue - minValue) / numberOfBins;

    QVector<int> binCounts(numberOfBins, 0);
    for (int value : valuesList) {
        int binIndex = qFloor((value - minValue) / binSize);
        if (binIndex >= 0 && binIndex < numberOfBins) {
            binCounts[binIndex]++;
        }
    }

    int maxBinCount = *std::max_element(binCounts.constBegin(), binCounts.constEnd());
    int maxAxisValue = (maxBinCount + 9) / 10 * 10; // Заокруглення вгору до ближчого кратного 10

    // Додавання точок до Area Chart
    QVector<QPointF> lowerPoints;
    QVector<QPointF> upperPoints;
    for (int i = 0; i < numberOfBins; ++i) {
        int binStart = qRound(minValue + i * binSize);
        int binEnd = qRound(minValue + (i + 1) * binSize);
        lowerPoints.append(QPointF(binStart, 0));
        lowerPoints.append(QPointF(binEnd, 0));
        upperPoints.append(QPointF(binStart, binCounts[i]));
        upperPoints.append(QPointF(binEnd, binCounts[i]));
    }

    // Встановлення точок для нижньої та верхньої межі області
    QLineSeries *lowerSeries = new QLineSeries();
    QLineSeries *upperSeries = new QLineSeries();
    lowerSeries->append(lowerPoints);
    upperSeries->append(upperPoints);
    series->setLowerSeries(lowerSeries);
    series->setUpperSeries(upperSeries);

    // Встановлення кольору області і його напівпрозорості
    QColor areaColor = QColor(Qt::blue);
    areaColor.setAlphaF(0.5); // Встановлюємо напівпрозорість в 50%
    series->setColor(areaColor);

    // Створення та налаштування обводки
    QPen pen = series->pen();
    pen.setWidth(2); // Ширина обводки, можна налаштувати за бажанням
    pen.setColor(Qt::blue); // Колір обводки, співпадає з кольором області
    series->setPen(pen);

    // Створення графіка та налаштування властивостей
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(""); // Заголовок гістограми
    chart->setAnimationOptions(QChart::SeriesAnimations);

    // Налаштування осі X
    QValueAxis *axisX = new QValueAxis();
    chart->addAxis(axisX, Qt::AlignBottom);
    lowerSeries->attachAxis(axisX);

    // Отримуємо мінімальне та максимальне значення для відображення на осі X
    int tickMinValue = qRound(minValue - binSize);
    int tickMaxValue = qRound(maxValue + binSize);

    // мінімальне значення на осі X не менше нуля
    tickMinValue = qMax(0, tickMinValue);

    // Кількість міток на вісі X
    int tickCount = qMin(10, numberOfBins); // Задаємо максимальну кількість міток, наприклад, 10
    axisX->setTickCount(tickCount);

    // Функція для генерації тексту мітки вісі X
    axisX->setLabelFormat("%d"); // Відображення цілих чисел

    // Встановлюємо мінімальне та максимальне значення осі X
    axisX->setMin(tickMinValue);
    axisX->setMax(tickMaxValue);

    // Налаштування осі Y
    QValueAxis *axisY = new QValueAxis();
    axisY->setLabelFormat("%d"); // Формат міток на осі Y (цілі числа)
    axisY->setRange(0, maxAxisValue + 10);
    chart->addAxis(axisY, Qt::AlignLeft);
    lowerSeries->attachAxis(axisY);

    // Приховати легенду (підпис "Counts")
    chart->legend()->hide();

    // Очистити horizontalFrame
    QLayoutItem *item;
    while ((item = ui->horizontalFrame->layout()->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    // Відображення графіка
    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    ui->horizontalFrame->layout()->addWidget(chartView);

    // Вивід кількості значень у векторі counts
    qDebug() << "Counts vector size:" << counts.size();

    // Вивід кількості унікальних значень у векторі counts
    qDebug() << "Unique counts values:" << valuesList.size();

    // Вивід унікальних значень і їх кількості
    QSet<int> uniqueValuesSet(valuesList.begin(), valuesList.end());
    QList<int> uniqueValues = uniqueValuesSet.values();
    std::sort(uniqueValues.begin(), uniqueValues.end());
    for (int value : uniqueValues) {
        int count = std::count(valuesList.begin(), valuesList.end(), value);
        qDebug() << "Value:" << value << ", Count:" << count;
    }

//    // Вивід значень і їх кількості
//    for (int i = 0; i < numberOfBins; ++i) {
//        int binStart = qRound(minValue + i * binSize);
//        int binEnd = qRound(minValue + (i + 1) * binSize);
//        int count = binCounts[i];
//        qDebug() << QString("%1 - %2").arg(binStart).arg(binEnd) << ", Count:" << count;
//    }
}



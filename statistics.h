#ifndef STATISTICS_H
#define STATISTICS_H

#include <QWidget>
#include <QVector>

#include <QtCharts>
#include <QChartView>
#include <QBarSet>
#include <QBarSeries>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QLineSeries>

namespace Ui {
class Statistics;
}

class Statistics : public QWidget
{
    Q_OBJECT

public:
    explicit Statistics(QWidget *parent = nullptr);
    // Statistics(int16_t*** voxels, int numCubes, QWidget *parent = nullptr);
    ~Statistics();
    void setVoxelCounts(int16_t ***voxels, int numCubes);

private:
    Ui::Statistics *ui;

};

#endif // STATISTICS_H

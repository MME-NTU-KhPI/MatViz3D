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
    Statistics(const QVector<int>& colorCounts, QWidget *parent = nullptr);
    ~Statistics();
    void setVoxelCounts(const QVector<int>& counts);

private:
    Ui::Statistics *ui;
    QVector<int> m_colorCounts;

};

#endif // STATISTICS_H

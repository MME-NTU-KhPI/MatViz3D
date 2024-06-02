#ifndef STATISTICS_H
#define STATISTICS_H

#include <QWidget>
#include <QVector>
#include <omp.h>
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
    void surfaceArea3D(int16_t ***voxels, int numCubes);
    void calcVolume3D(int16_t ***voxels, int numCubes);
    void calcNormVolume3D();
    void calcMomentInertia();
    void calcESR();
private:
    Ui::Statistics *ui;
    std::map<int, double> surface_area_3D;
    std::map<int, double> volume_3D;
    std::map<int, double> norm_volume_3D;
    std::map<int, double> moment_inertia_3D;
    std::map<int, double> ESR_3D;
};

#endif // STATISTICS_H

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

// Прототип структури Object
struct Object;

namespace Ui {
class Statistics;
}

class Statistics : public QWidget
{
    Q_OBJECT

public:
    explicit Statistics(QWidget *parent = nullptr);
    // Statistics(int32_t*** voxels, int numCubes, QWidget *parent = nullptr);
    ~Statistics();
    void layersProcessing(int32_t ***voxels, int numCubes);
    void buildHistogram(const QVector<float>& counts, QString selectedTitleProperty);
    void selectProperty();
    void clearLayout(QLayout *layout);
    QChart* createChart(const QString& selectedTitleProperty);
    QValueAxis* createAxisX();
    QValueAxis* createAxisY();
    void adjustAxisX(QValueAxis *axisX, const QVector<float>& counts);
    QAreaSeries* createHistogramSeries(const QVector<float>& counts);
    void setPropertyBoxText(const QString &text);

    void surfaceArea3D(int32_t ***voxels, int numCubes);
    void calcVolume3D(int32_t ***voxels, int numCubes);
    void calcNormVolume3D();
    void calcMomentInertia();
    void calcESR();

private slots:
    void on_saveChartAsIMGButton_clicked();

private:
    Ui::Statistics *ui;
    QVector<Object> allObjects;
    void updatePropertyBox();

    std::map<int, double> surface_area_3D;
    std::map<int, double> volume_3D;
    std::map<int, double> norm_volume_3D;
    std::map<int, double> moment_inertia_3D;
    std::map<int, double> ESR_3D;

};

// Оголошення структури Object
struct Object {
    int label;
    int size;
    int perimeter;
    double normArea;
    double ecr;
    double shape_factor;
    int volume_3D;
    double norm_volume_3D;
    double surface_area_3D;
    double moment_inertia_3D;
    double ESR_3D;

    // Default constructor
    Object() : label(0), size(0), perimeter(0), normArea(0.0f), ecr(0.0f),
        shape_factor(0.0), volume_3D(0), norm_volume_3D(0.0),
        surface_area_3D(0.0), moment_inertia_3D(0.0), ESR_3D(0.0) {}

    // Parameterized constructor
    Object(int _volume3d, double _normvolume3d, double _surfacearea3d,
           double _momentinertia3d, double _esr3d)
        : volume_3D(_volume3d),
        norm_volume_3D(_normvolume3d), surface_area_3D(_surfacearea3d),
        moment_inertia_3D(_momentinertia3d), ESR_3D(_esr3d) {}

    Object(int label, int size, int perimeter, double normArea, double shape_factor, double ecr)
        : label(label), size(size), perimeter(perimeter), normArea(normArea), shape_factor(shape_factor), ecr(ecr) {}

};


#endif // STATISTICS_H

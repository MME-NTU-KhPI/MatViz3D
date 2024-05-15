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
    // Statistics(int16_t*** voxels, int numCubes, QWidget *parent = nullptr);
    ~Statistics();
    void layersProcessing(int16_t ***voxels, int numCubes);
    void buildHistogram(const QVector<float>& counts);
    void selectProperty();
    void clearLayout(QLayout *layout);

private:
    Ui::Statistics *ui;
    QVector<Object> allObjects;

};

// Оголошення структури Object
struct Object {
    int label;
    int size;
    int perimeter;
    float normArea; // Нормалізована площа
    float ecr;
    float shape_factor;
    Object(int _label, int _size, int _perimeter, float _normArea, float _shape_factor, float _ecr) : label(_label), size(_size), perimeter(_perimeter), normArea(_normArea), shape_factor(_shape_factor), ecr(_ecr) {}
};

#endif // STATISTICS_H

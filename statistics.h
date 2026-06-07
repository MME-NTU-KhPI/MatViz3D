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
#include "grain_analyzer.h"

struct Object;

namespace Ui {
class Statistics;
}

class Statistics : public QWidget
{
    Q_OBJECT

public:
    explicit Statistics(QWidget *parent = nullptr);
    ~Statistics();

    /**
     * @brief Main entry point.
     * Replaces: layersProcessing + calcVolume3D + calcESR +
     *           calcNormVolume3D + calcMomentInertia + surfaceArea3D
     *
     * Delegates 3D computation to GrainAnalyzer (no UI code there).
     * Keeps 2D layer analysis here (needs Qt types internally).
     */
    void analyzeAndDisplay(int32_t*** voxels, int numCubes);

    /**
     * @brief Compatibility wrapper — calls analyzeAndDisplay().
     * Keep while mainwindow.cpp still calls layersProcessing().
     */
    void layersProcessing(int32_t*** voxels, int numCubes);

    // ── UI helpers ────────────────────────────────────────────────────────
    void         buildHistogram(const QVector<float>& counts,
                        const QString& title);
    void         selectProperty();
    void         clearLayout(QLayout* layout);
    void         setPropertyBoxText(const QString& text);

    QChart*      createChart(const QString& title);
    QValueAxis*  createAxisX();
    QValueAxis*  createAxisY();
    void         adjustAxisX(QValueAxis* axisX,
                     const QVector<float>& counts);
    QAreaSeries* createHistogramSeries(const QVector<float>& counts);

private slots:
    void on_saveChartAsIMGButton_clicked();

private:
    Ui::Statistics* ui;

    // 3D grain stats — computed by GrainAnalyzer
    std::map<int32_t, GrainAnalyzer::GrainStats> m_grainStats;

    // 2D per-layer objects (ECR, area, perimeter, shape factor)
    QList<Object> allObjects2D;

    void updatePropertyBox();
    void processLayers2D(int32_t*** voxels, int numCubes);
};

// ── Object: 2D per-layer grain metrics ───────────────────────────────────
struct Object
{
    int    label        = 0;
    int    size         = 0;
    int    perimeter    = 0;
    double normArea     = 0.0;
    double ecr          = 0.0;
    double shape_factor = 0.0;

    Object() = default;

    Object(int label, int size, int perimeter,
           double normArea, double shape_factor, double ecr)
        : label(label), size(size), perimeter(perimeter),
        normArea(normArea), ecr(ecr), shape_factor(shape_factor) {}
};

#endif // STATISTICS_H

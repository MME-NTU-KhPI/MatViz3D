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
    std::map<int32_t, GrainAnalyzer::GrainStats3D> m_grainStats;

    // 2D per-layer grain stats — computed by GrainAnalyzer
    std::vector<GrainAnalyzer::GrainStats2D> m_grainStats2D;

    void updatePropertyBox();
};

#endif // STATISTICS_H

#ifndef STATISTICSCONTROLLER_H
#define STATISTICSCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVector>
#include <map>
#include <vector>
#include "grain_analyzer.h"

class StatisticsController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList availableProperties READ availableProperties NOTIFY modeChanged)
    Q_PROPERTY(QString     mode                READ mode               NOTIFY modeChanged)

    Q_PROPERTY(QVariantList histogramPoints READ histogramPoints NOTIFY histogramChanged)
    Q_PROPERTY(QString      chartTitle      READ chartTitle      NOTIFY histogramChanged)

    Q_PROPERTY(double axisXMin    READ axisXMin    NOTIFY histogramChanged)
    Q_PROPERTY(double axisXMax    READ axisXMax    NOTIFY histogramChanged)
    Q_PROPERTY(int    axisYMax    READ axisYMax    NOTIFY histogramChanged)
    Q_PROPERTY(bool   hasData     READ hasData     NOTIFY histogramChanged)

    // Tallest bin. axisYMax is padded for a tidy axis, so bar shading has to
    // normalise against this instead, or the tallest bar never reaches full
    // intensity.
    Q_PROPERTY(int    histogramPeak READ histogramPeak NOTIFY histogramChanged)

    Q_PROPERTY(QVariantList descriptiveStats READ descriptiveStats NOTIFY histogramChanged)

    Q_PROPERTY(QString axisXLabel READ axisXLabel NOTIFY histogramChanged)

    Q_PROPERTY(int binCount READ binCount WRITE setBinCount NOTIFY binCountChanged)

public:
    explicit StatisticsController(QObject* parent = nullptr);

    Q_INVOKABLE void analyze();

    Q_INVOKABLE void setMode(const QString& mode);

    Q_INVOKABLE void selectProperty(const QString& propertyName);

    Q_INVOKABLE void exportCSV(const QString& filePath);

    // Vector export of the histogram, written from the same bin data the chart
    // draws (the QML Shapes can only be grabbed as pixels). withStats appends
    // the descriptive-statistics panel, matching what the window shows.
    Q_INVOKABLE bool exportSvg(const QUrl& fileUrl, bool dark, bool withStats);

    // QUrl (from FileDialog) -> native path, for Item.grabToImage's saveToFile().
    Q_INVOKABLE QString toLocalFile(const QUrl& fileUrl) const;

    Q_INVOKABLE void setBinCount(int count);

    QStringList  availableProperties() const;
    QString      mode()            const { return m_mode; }
    QVariantList histogramPoints() const { return m_points; }
    QString      chartTitle()      const { return m_title; }
    double       axisXMin()        const { return m_axisXMin; }
    double       axisXMax()        const { return m_axisXMax; }
    int          axisYMax()        const { return m_axisYMax; }
    bool         hasData()         const { return !m_points.isEmpty(); }
    int          histogramPeak()   const { return m_histPeak; }
    QVariantList descriptiveStats() const { return m_descStats; }
    QString      axisXLabel()       const { return m_axisXLabel; }
    int          binCount()         const { return m_binCount; }

signals:
    void modeChanged();
    void histogramChanged();
    void analysisFinished();
    void binCountChanged();

private:
    QString svgHistogram(bool dark, bool withStats) const;

    QVector<float> collectValues(const QString& propertyName, QString& titleOut) const;

    void buildHistogram(const QVector<float>& values);

    void computeDescriptiveStats(const QVector<float>& values);

    std::map<int32_t, GrainAnalyzer::GrainStats3D> m_stats3D;
    std::vector<GrainAnalyzer::GrainStats2D>       m_stats2D;

    QString      m_mode = "3D";
    QVariantList m_points;
    QVariantList m_descStats;
    QString      m_axisXLabel;
    QString      m_title;
    double       m_axisXMin = 0.0;
    double       m_axisXMax = 1.0;
    int          m_axisYMax = 10;
    int          m_histPeak = 0;
    int          m_binCount = 0;
    QVector<float> m_lastValues;
};

#endif // STATISTICSCONTROLLER_H

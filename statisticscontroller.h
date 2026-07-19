#ifndef STATISTICSCONTROLLER_H
#define STATISTICSCONTROLLER_H

#include <QObject>
#include <QStringList>
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

public:
    explicit StatisticsController(QObject* parent = nullptr);

    Q_INVOKABLE void analyze();

    Q_INVOKABLE void setMode(const QString& mode);

    Q_INVOKABLE void selectProperty(const QString& propertyName);

    Q_INVOKABLE void exportCSV(const QString& filePath);

    QStringList  availableProperties() const;
    QString      mode()            const { return m_mode; }
    QVariantList histogramPoints() const { return m_points; }
    QString      chartTitle()      const { return m_title; }
    double       axisXMin()        const { return m_axisXMin; }
    double       axisXMax()        const { return m_axisXMax; }
    int          axisYMax()        const { return m_axisYMax; }
    bool         hasData()         const { return !m_points.isEmpty(); }

signals:
    void modeChanged();
    void histogramChanged();
    void analysisFinished();

private:
    QVector<float> collectValues(const QString& propertyName, QString& titleOut) const;

    void buildHistogram(const QVector<float>& values);

    std::map<int32_t, GrainAnalyzer::GrainStats3D> m_stats3D;
    std::vector<GrainAnalyzer::GrainStats2D>       m_stats2D;

    QString      m_mode = "3D";
    QVariantList m_points;
    QString      m_title;
    double       m_axisXMin = 0.0;
    double       m_axisXMax = 1.0;
    int          m_axisYMax = 10;
};

#endif // STATISTICSCONTROLLER_H

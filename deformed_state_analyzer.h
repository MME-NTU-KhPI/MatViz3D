#ifndef DEFORMED_STATE_ANALYZER_H
#define DEFORMED_STATE_ANALYZER_H

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <vector>
#include <cstdint>

class DeformedStateAnalyzer
{
public:
    enum class StatMode {
        FullVolume,
        PerGrainMean
    };

    enum class Property {
        VonMisesStress,
        StressSX,
        StressSY,
        StressSZ,
        StressSXY,
        StressSYZ,
        StressSXZ,
        HydrostaticStress,
        StressTriaxiality,
        VonMisesStrain,
        StrainEpsX,
        StrainEpsY,
        StrainEpsZ,
        StrainEpsXY,
        StrainEpsYZ,
        StrainEpsXZ,
        DisplacementUSUM,
        DisplacementUX,
        DisplacementUY,
        DisplacementUZ
    };

    struct DescriptiveStats {
        int    n        = 0;
        double mean     = 0.0;
        double stdDev   = 0.0;
        double variance = 0.0;
        double cv       = 0.0; // coefficient of variation (%)
        double median   = 0.0;
        double q1       = 0.0;
        double q3       = 0.0;
        double iqr      = 0.0;
        double minVal   = 0.0;
        double maxVal   = 0.0;
        double p1       = 0.0; // 1st percentile
        double p99      = 0.0; // 99th percentile
        double skewness = 0.0;
        double kurtosis = 0.0;
    };

    struct HistogramResult {
        QVariantList barPoints;  // [{x: start, y: count}, {x: end, y: count}, ...]
        QVariantList kdePoints;  // [{x: val, y: density}, ...]
        double axisXMin  = 0.0;
        double axisXMax  = 1.0;
        int    axisYMax  = 10;
        int    histPeak  = 0;
        double kdeMax    = 0.0;
    };

    static QStringList availableProperties();
    static QString propertyLabel(Property prop);
    static QString propertyTitle(Property prop);
    static QString propertyUnits(Property prop);

    static QVector<float> extractValues(const std::vector<std::vector<float>>& results,
                                        int32_t*** voxels,
                                        int cubeSize,
                                        Property prop,
                                        StatMode mode);

    static DescriptiveStats computeStats(const QVector<float>& values);

    static HistogramResult computeHistogramAndKDE(const QVector<float>& values, int binCount);

    static QVariantList statsToVariantList(const DescriptiveStats& stats, Property prop);

    static QString generateSvg(const QString& title,
                               const QString& axisXLabel,
                               const HistogramResult& hist,
                               const DescriptiveStats& stats,
                               bool dark,
                               bool withStats);

    static bool exportToCsv(const QString& filePath,
                            const std::vector<std::vector<float>>& results,
                            int32_t*** voxels,
                            int cubeSize);
};

#endif // DEFORMED_STATE_ANALYZER_H

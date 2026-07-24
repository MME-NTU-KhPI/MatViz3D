#include "statisticscontroller.h"
#include "parameters.h"
#include <QDebug>
#include <algorithm>
#include <cmath>

StatisticsController::StatisticsController(QObject* parent)
    : QObject(parent)
{
}

void StatisticsController::analyze()
{
    int32_t*** voxels = Parameters::voxels;
    const int  n      = Parameters::instance()->getSize();

    if (!voxels || n <= 0) {
        qWarning() << "StatisticsController::analyze: no voxel data";
        return;
    }

    m_stats3D = GrainAnalyzer::analyze3D(voxels, n);
    m_stats2D = GrainAnalyzer::analyze2D(voxels, n);

    qDebug() << "StatisticsController: analyzed"
             << (int)m_stats3D.size() << "grains (3D),"
             << (int)m_stats2D.size() << "regions (2D)";

    emit analysisFinished();
}

void StatisticsController::setMode(const QString& mode)
{
    if (m_mode == mode) return;
    m_mode = mode;

    m_points.clear();
    m_title.clear();
    m_descStats.clear();
    m_axisXLabel.clear();

    emit modeChanged();
    emit histogramChanged();
}

QStringList StatisticsController::availableProperties() const
{
    if (m_mode == "2D")
        return { "Area", "Norm Area", "Perimeter", "ECR", "Shape factor" };

    return { "Volume", "Norm Volume", "Surface Area", "ESR", "Inertia Moment" };
}

QVector<float> StatisticsController::collectValues(const QString& prop,
                                                   QString& titleOut) const
{
    QVector<float> values;

    // ── 2D ──
    if (prop == "Area") {
        titleOut = "Distribution of grain area";
        for (const auto& o : m_stats2D) values.push_back(o.size);

    } else if (prop == "Norm Area") {
        titleOut = "Distribution of normalized grain area";
        for (const auto& o : m_stats2D) values.push_back(o.norm_area);

    } else if (prop == "Perimeter") {
        titleOut = "Distribution of grain perimeter";
        for (const auto& o : m_stats2D) values.push_back(o.perimeter);

    } else if (prop == "ECR") {
        titleOut = "Distribution of ECR";
        for (const auto& o : m_stats2D) values.push_back(o.ecr);

    } else if (prop == "Shape factor") {
        titleOut = "Distribution of shape factor";
        for (const auto& o : m_stats2D) values.push_back(o.shape_factor);

        // ── 3D ──
    } else if (prop == "Volume") {
        titleOut = "Distribution of grain volume";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.volume);

    } else if (prop == "Norm Volume") {
        titleOut = "Distribution of normalized grain volume";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.norm_volume);

    } else if (prop == "Surface Area") {
        titleOut = "Distribution of grain surface area";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.surface_area);

    } else if (prop == "ESR") {
        titleOut = "Distribution of ESR";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.esr);

    } else if (prop == "Inertia Moment") {
        titleOut = "Distribution of grain inertia moment";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.moment_inertia);

    } else {
        titleOut = "Choose a grain property";
    }

    values.erase(
        std::remove_if(values.begin(), values.end(),
                       [](float v) { return std::isinf(v) || std::isnan(v) || v == 0.0f; }),
        values.end());

    return values;
}

void StatisticsController::selectProperty(const QString& propertyName)
{
    QString title;
    QVector<float> values = collectValues(propertyName, title);

    m_lastValues = values;
    m_title = title;
    m_axisXLabel = propertyName;
    buildHistogram(values);
    computeDescriptiveStats(values);

    emit histogramChanged();
}

void StatisticsController::buildHistogram(const QVector<float>& values)
{
    m_points.clear();

    if (values.isEmpty()) {
        m_axisXMin = 0.0;
        m_axisXMax = 1.0;
        m_axisYMax = 10;
        return;
    }

    const float minV = *std::min_element(values.constBegin(), values.constEnd());
    const float maxV = *std::max_element(values.constBegin(), values.constEnd());

    int bins;
    if (m_binCount > 0) {
        bins = m_binCount;
    } else {
        bins = (int)std::round(std::sqrt((double)values.size())) / 2;
    }
    if (bins < 1) bins = 1;

    const float binWidth = (maxV - minV) / bins;
    if (binWidth <= 0.0f) {
        m_points.append(QVariantMap{ {"x", minV}, {"y", values.size()} });
        m_axisXMin = minV - 1.0;
        m_axisXMax = maxV + 1.0;
        m_axisYMax = values.size();
        return;
    }

    QVector<int> binCounts(bins, 0);
    for (float v : values) {
        int idx = (int)((v - minV) / binWidth);
        if (idx >= bins) idx = bins - 1;
        if (idx >= 0)    binCounts[idx]++;
    }

    int maxCount = 0;
    for (int i = 0; i < bins; ++i) {
        const double xStart = minV + i * binWidth;
        const double xEnd   = xStart + binWidth;
        const int    c      = binCounts[i];

        m_points.append(QVariantMap{ {"x", xStart}, {"y", c} });
        m_points.append(QVariantMap{ {"x", xEnd},   {"y", c} });

        if (c > maxCount) maxCount = c;
    }

    m_axisXMin = std::max(0.0, (double)minV - binWidth);
    m_axisXMax = (double)maxV + binWidth;
    m_axisYMax = (maxCount / 10 + 1) * 10 + 10;
}

namespace {

QString fmtNum(double v)
{
    if (v == 0.0) return QStringLiteral("0");
    const double a = std::abs(v);
    if (a >= 1e5 || a < 1e-3)
        return QString::number(v, 'e', 3);
    return QString::number(v, 'g', 5);
}

} // anonymous namespace

void StatisticsController::computeDescriptiveStats(const QVector<float>& values)
{
    m_descStats.clear();

    const int n = values.size();
    if (n == 0)
        return;

    double sum = 0.0;
    for (float v : values) sum += v;
    const double mean = sum / n;

    double m2 = 0.0, m3 = 0.0, m4 = 0.0;
    for (float v : values) {
        const double d  = v - mean;
        const double d2 = d * d;
        m2 += d2;
        m3 += d2 * d;
        m4 += d2 * d2;
    }
    m2 /= n;
    m3 /= n;
    m4 /= n;

    const double variance = (n > 1) ? (m2 * n) / (n - 1) : 0.0;
    const double stdDev   = std::sqrt(variance);

    const double cv = (std::abs(mean) > 1e-30) ? (stdDev / mean * 100.0) : 0.0;

    const double sigma = std::sqrt(m2);
    const double skew  = (sigma > 1e-30) ? m3 / (sigma * sigma * sigma) : 0.0;
    const double kurt  = (m2    > 1e-30) ? m4 / (m2 * m2) - 3.0         : 0.0;

    QVector<float> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    const double median = (n % 2 == 1)
                              ? sorted[n / 2]
                              : 0.5 * (sorted[n / 2 - 1] + sorted[n / 2]);

    auto add = [this](const QString& label, const QString& value) {
        m_descStats.append(QVariantMap{ {"label", label}, {"value", value} });
    };

    add(QStringLiteral("N"),        QString::number(n));
    add(QStringLiteral("Mean"),     fmtNum(mean));
    add(QStringLiteral("Std"),      fmtNum(stdDev));
    add(QStringLiteral("CV"),       QString::number(cv, 'f', 1) + QStringLiteral(" %"));
    add(QStringLiteral("Median"),   fmtNum(median));
    add(QStringLiteral("Min"),      fmtNum(sorted.first()));
    add(QStringLiteral("Max"),      fmtNum(sorted.last()));
    add(QStringLiteral("Skewness"), QString::number(skew, 'f', 3));
    add(QStringLiteral("Kurtosis"), QString::number(kurt, 'f', 3));
}

void StatisticsController::exportCSV(const QString& filePath)
{
    if (m_mode == "2D")
        GrainAnalyzer::writeToCSV2D(m_stats2D, filePath);
    else
        GrainAnalyzer::writeToCSV3D(m_stats3D, filePath);
}

void StatisticsController::setBinCount(int count)
{
    if (m_binCount == count) return;
    m_binCount = count;
    emit binCountChanged();

    if (!m_lastValues.isEmpty()) {
        buildHistogram(m_lastValues);
        emit histogramChanged();
    }
}

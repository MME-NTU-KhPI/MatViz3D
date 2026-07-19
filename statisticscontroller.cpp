#include "statisticscontroller.h"
#include "parameters.h"
#include <QDebug>
#include <algorithm>
#include <cmath>

StatisticsController::StatisticsController(QObject* parent)
    : QObject(parent)
{
}

// ═══════════════════════════════════════════════════════════════════
// Анализ: берём воксели из Parameters и считаем метрики
// ═══════════════════════════════════════════════════════════════════
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

// ═══════════════════════════════════════════════════════════════════
// Режим 2D / 3D — меняет список доступных свойств
// ═══════════════════════════════════════════════════════════════════
void StatisticsController::setMode(const QString& mode)
{
    if (m_mode == mode) return;
    m_mode = mode;

    // Сбрасываем текущую гистограмму
    m_points.clear();
    m_title.clear();

    emit modeChanged();
    emit histogramChanged();
}

QStringList StatisticsController::availableProperties() const
{
    if (m_mode == "2D")
        return { "Area", "Norm Area", "Perimeter", "ECR", "Shape factor" };

    return { "Volume", "Norm Volume", "Surface Area", "ESR", "Inertia Moment" };
}

// ═══════════════════════════════════════════════════════════════════
// Сбор значений выбранного свойства
// ═══════════════════════════════════════════════════════════════════
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

    // Фильтр NaN / Inf / нулей — как в оригинале
    values.erase(
        std::remove_if(values.begin(), values.end(),
                       [](float v) { return std::isinf(v) || std::isnan(v) || v == 0.0f; }),
        values.end());

    return values;
}

// ═══════════════════════════════════════════════════════════════════
// Выбор свойства → пересчёт гистограммы
// ═══════════════════════════════════════════════════════════════════
void StatisticsController::selectProperty(const QString& propertyName)
{
    QString title;
    QVector<float> values = collectValues(propertyName, title);

    m_title = title;
    buildHistogram(values);

    emit histogramChanged();
}

// ═══════════════════════════════════════════════════════════════════
// Биннинг: значения → точки ступенчатой кривой (как QAreaSeries)
// ═══════════════════════════════════════════════════════════════════
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

    // Число бинов — как в оригинале: sqrt(N) / 2
    int bins = (int)std::ceil(1.0 + std::log2((double)values.size()));
    if (bins < 1) bins = 1;

    const float binWidth = (maxV - minV) / bins;
    if (binWidth <= 0.0f) {
        // Все значения одинаковые — один столбец
        m_points.append(QVariantMap{ {"x", minV}, {"y", values.size()} });
        m_axisXMin = minV - 1.0;
        m_axisXMax = maxV + 1.0;
        m_axisYMax = values.size();
        return;
    }

    // Подсчёт попаданий в бины
    QVector<int> binCounts(bins, 0);
    for (float v : values) {
        int idx = (int)((v - minV) / binWidth);
        if (idx >= bins) idx = bins - 1;      // граничный случай v == maxV
        if (idx >= 0)    binCounts[idx]++;
    }

    // Ступенчатая кривая: для каждого бина две точки (начало и конец)
    int maxCount = 0;
    for (int i = 0; i < bins; ++i) {
        const double xStart = minV + i * binWidth;
        const double xEnd   = xStart + binWidth;
        const int    c      = binCounts[i];

        m_points.append(QVariantMap{ {"x", xStart}, {"y", c} });
        m_points.append(QVariantMap{ {"x", xEnd},   {"y", c} });

        if (c > maxCount) maxCount = c;
    }

    // Границы осей — как в adjustAxisX оригинала
    m_axisXMin = std::max(0.0, (double)minV - binWidth);
    m_axisXMax = (double)maxV + binWidth;
    m_axisYMax = (maxCount / 10 + 1) * 10 + 10;
}

// ═══════════════════════════════════════════════════════════════════
// Экспорт в CSV — переиспользуем GrainAnalyzer
// ═══════════════════════════════════════════════════════════════════
void StatisticsController::exportCSV(const QString& filePath)
{
    if (m_mode == "2D")
        GrainAnalyzer::writeToCSV2D(m_stats2D, filePath);
    else
        GrainAnalyzer::writeToCSV3D(m_stats3D, filePath);
}

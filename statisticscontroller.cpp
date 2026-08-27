#include "statisticscontroller.h"
#include "parameters.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
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

    return {
        "Volume",
        "Norm Volume",
        "Surface Area",
        "ESR",
        "Inertia Moment (Mean)",
        "Principal Moment I1",
        "Principal Moment I2",
        "Principal Moment I3",
        "Semi-axis a",
        "Semi-axis b",
        "Semi-axis c",
        "Aspect Ratio (a/c)",
        "Inertial Sphericity (c/a)",
        "Fractional Anisotropy",
        "Inertia Ixx",
        "Inertia Iyy",
        "Inertia Izz",
        "Inertia Ixy",
        "Inertia Ixz",
        "Inertia Iyz"
    };
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

    } else if (prop == "Inertia Moment" || prop == "Inertia Moment (Mean)") {
        titleOut = "Distribution of mean principal moment of inertia";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.moment_inertia);

    } else if (prop == "Principal Moment I1") {
        titleOut = "Distribution of maximum principal moment of inertia (I1)";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.I1);

    } else if (prop == "Principal Moment I2") {
        titleOut = "Distribution of intermediate principal moment of inertia (I2)";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.I2);

    } else if (prop == "Principal Moment I3") {
        titleOut = "Distribution of minimum principal moment of inertia (I3)";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.I3);

    } else if (prop == "Semi-axis a") {
        titleOut = "Distribution of equivalent ellipsoid major semi-axis (a)";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.semi_a);

    } else if (prop == "Semi-axis b") {
        titleOut = "Distribution of equivalent ellipsoid intermediate semi-axis (b)";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.semi_b);

    } else if (prop == "Semi-axis c") {
        titleOut = "Distribution of equivalent ellipsoid minor semi-axis (c)";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.semi_c);

    } else if (prop == "Aspect Ratio (a/c)") {
        titleOut = "Distribution of grain elongation aspect ratio (a/c)";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.aspect_ratio);

    } else if (prop == "Inertial Sphericity (c/a)") {
        titleOut = "Distribution of inertial sphericity index (c/a)";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.sphericity_inertia);

    } else if (prop == "Fractional Anisotropy") {
        titleOut = "Distribution of grain fractional anisotropy (FA)";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.fractional_anisotropy);

    } else if (prop == "Inertia Ixx") {
        titleOut = "Distribution of inertia tensor component Ixx";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.Ixx);

    } else if (prop == "Inertia Iyy") {
        titleOut = "Distribution of inertia tensor component Iyy";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.Iyy);

    } else if (prop == "Inertia Izz") {
        titleOut = "Distribution of inertia tensor component Izz";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.Izz);

    } else if (prop == "Inertia Ixy") {
        titleOut = "Distribution of inertia tensor off-diagonal component Ixy";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.Ixy);

    } else if (prop == "Inertia Ixz") {
        titleOut = "Distribution of inertia tensor off-diagonal component Ixz";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.Ixz);

    } else if (prop == "Inertia Iyz") {
        titleOut = "Distribution of inertia tensor off-diagonal component Iyz";
        for (const auto& [id, s] : m_stats3D) values.push_back((float)s.Iyz);

    } else {
        titleOut = "Choose a grain property";
    }

    values.erase(
        std::remove_if(values.begin(), values.end(),
                       [](float v) { return std::isinf(v) || std::isnan(v); }),
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
    m_histPeak = 0;

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

    // Case 1: All values are identical
    if (std::abs(maxV - minV) < 1e-12f) {
        m_points.append(QVariantMap{ {"x", (double)minV - 0.5}, {"y", values.size()} });
        m_points.append(QVariantMap{ {"x", (double)minV + 0.5}, {"y", values.size()} });
        if (minV == 0.0f) {
            m_axisXMin = -1.0;
            m_axisXMax = 1.0;
        } else if (minV > 0.0f) {
            m_axisXMin = 0.0;
            m_axisXMax = minV + 1.0;
        } else {
            m_axisXMin = minV - 1.0;
            m_axisXMax = 0.0;
        }
        m_axisYMax = values.size();
        m_histPeak = values.size();
        return;
    }

    // Case 2: Range spans negative and positive values (e.g. Ixy, Ixz, Iyz) -> Strict 0 point
    if (minV < 0.0f && maxV > 0.0f)
    {
        const double maxAbs = std::max(std::abs(minV), std::abs(maxV));
        const int halfBins = std::max(2, (bins + 1) / 2);
        const double binWidth = maxAbs / halfBins;
        const int totalBins = halfBins * 2;

        QVector<int> binCounts(totalBins, 0);
        for (float v : values) {
            int idx = (int)std::floor(v / binWidth) + halfBins;
            if (idx < 0) idx = 0;
            if (idx >= totalBins) idx = totalBins - 1;
            binCounts[idx]++;
        }

        int maxCount = 0;
        for (int i = 0; i < totalBins; ++i) {
            const double xStart = (i - halfBins) * binWidth;
            const double xEnd   = xStart + binWidth;
            const int    c      = binCounts[i];

            m_points.append(QVariantMap{ {"x", xStart}, {"y", c} });
            m_points.append(QVariantMap{ {"x", xEnd},   {"y", c} });

            if (c > maxCount) maxCount = c;
        }

        const double axisExtent = (halfBins + 1) * binWidth;
        m_axisXMin = -axisExtent;
        m_axisXMax = +axisExtent;
        m_axisYMax = (maxCount / 10 + 1) * 10 + 10;
        m_histPeak = maxCount;
        return;
    }

    // Case 3: Strictly non-negative or strictly non-positive
    const float binWidth = (maxV - minV) / bins;
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

    if (minV >= 0.0f) {
        m_axisXMin = std::max(0.0, (double)minV - binWidth);
        m_axisXMax = (double)maxV + binWidth;
    } else {
        m_axisXMin = (double)minV - binWidth;
        m_axisXMax = std::min(0.0, (double)maxV + binWidth);
    }
    m_axisYMax = (maxCount / 10 + 1) * 10 + 10;
    m_histPeak = maxCount;
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

// ─────────────────────────────────────────────────────────────────────────────
//  SVG export
//
//  The chart is drawn with QML Shapes, which can only be grabbed as pixels, so
//  vector output is written from the same bin data. Layout and colours mirror
//  StatisticsView.qml / ChartTheme.qml -- keep them in step if either changes.
//
//  NB: colours are plain #rrggbb with separate *-opacity attributes. rgba() is
//  CSS/SVG2 and is silently dropped by SVG 1.1 renderers such as QSvgRenderer
//  and Inkscape.
// ─────────────────────────────────────────────────────────────────────────────
namespace {

struct ChartSvgPalette {
    QString window, plot, plotBorder, grid, tick, title, axisTitle, axisLabel, series;
    double  gridAlpha;
};

ChartSvgPalette chartSvgPalette(bool dark)
{
    if (dark)
        return { "#282828", "#1e1e1e", "#4a4a4a", "#ffffff", "#969696",
                 "#d9d9d9", "#c6c6c6", "#969696", "#00897b", 0.20 };
    return     { "#f2f2f2", "#ffffff", "#b0b0b0", "#000000", "#555555",
                 "#1a1a1a", "#333333", "#555555", "#00564d", 0.13 };
}

QString n2(double v, int prec = 2) { return QString::number(v, 'f', prec); }

// Same rule as StatisticsView.qml's fmt().
QString fmtAxis(double v)
{
    if (std::abs(v) < 1e-9)
        return QStringLiteral("0");
    if (std::abs(v) >= 10000.0 || std::abs(v) < 0.001)
        return QString::number(v, 'e', 2);
    return QString::number(QString::number(v, 'g', 4).toDouble());
}

QString svgTxt(double x, double y, const QString& s, const QString& fill,
               int size = 12, bool bold = false, const QString& anchor = "start")
{
    return QString("<text x=\"%1\" y=\"%2\" fill=\"%3\" font-family=\"Segoe UI, Arial, sans-serif\" "
                   "font-size=\"%4\" %5text-anchor=\"%6\">%7</text>\n")
        .arg(n2(x), n2(y), fill).arg(size)
        .arg(bold ? "font-weight=\"bold\" " : "", anchor, s.toHtmlEscaped());
}

} // anonymous namespace

QString StatisticsController::svgHistogram(bool dark, bool withStats) const
{
    const ChartSvgPalette p = chartSvgPalette(dark);
    const bool stats = withStats && !m_descStats.isEmpty();

    const double left = 90.0, top = 62.0, plotW = 780.0, plotH = 430.0;
    const double statsW = stats ? 230.0 : 0.0;
    const double W = left + plotW + 40.0 + statsW;
    const double H = top + plotH + 78.0;
    const int    xTicks = 8, yTicks = 6;

    QString out = QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                          "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%2\" "
                          "viewBox=\"0 0 %1 %2\">\n"
                          "<rect width=\"%1\" height=\"%2\" fill=\"%3\"/>\n")
                      .arg(n2(W, 0), n2(H, 0), p.window);

    out += svgTxt(left + plotW / 2.0, 34.0, m_title, p.title, 18, true, "middle");

    out += QString("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" fill=\"%5\" "
                   "stroke=\"%6\" stroke-width=\"1\"/>\n")
               .arg(n2(left), n2(top), n2(plotW), n2(plotH), p.plot, p.plotBorder);

    // grid
    out += QString("<g stroke=\"%1\" stroke-opacity=\"%2\" stroke-width=\"1\">\n")
               .arg(p.grid, n2(p.gridAlpha));
    for (int i = 0; i <= xTicks; ++i) {
        const double x = left + plotW * i / xTicks;
        out += QString("<line x1=\"%1\" y1=\"%2\" x2=\"%1\" y2=\"%3\"/>\n")
                   .arg(n2(x), n2(top), n2(top + plotH));
    }
    for (int i = 0; i <= yTicks; ++i) {
        const double y = top + plotH - plotH * i / yTicks;
        out += QString("<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%2\"/>\n")
                   .arg(n2(left), n2(y), n2(left + plotW));
    }
    out += "</g>\n";

    // ticks
    out += QString("<g stroke=\"%1\" stroke-width=\"1\">\n").arg(p.tick);
    for (int i = 0; i <= xTicks; ++i) {
        const double x = left + plotW * i / xTicks;
        out += QString("<line x1=\"%1\" y1=\"%2\" x2=\"%1\" y2=\"%3\"/>\n")
                   .arg(n2(x), n2(top + plotH), n2(top + plotH + 6.0));
    }
    for (int i = 0; i <= yTicks; ++i) {
        const double y = top + plotH - plotH * i / yTicks;
        out += QString("<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%2\"/>\n")
                   .arg(n2(left - 6.0), n2(y), n2(left));
    }

    // Zero reference line if range spans 0
    if (m_axisXMin < 0.0 && m_axisXMax > 0.0) {
        const double xZero = left + (-m_axisXMin / (m_axisXMax - m_axisXMin)) * plotW;
        out += QString("<line x1=\"%1\" y1=\"%2\" x2=\"%1\" y2=\"%3\" stroke=\"%4\" stroke-width=\"1.5\"/>\n")
                   .arg(n2(xZero), n2(top), n2(top + plotH), p.tick);
    }
    out += "</g>\n";

    for (int i = 0; i <= xTicks; ++i) {
        const double x   = left + plotW * i / xTicks;
        const double val = m_axisXMin + (double(i) / xTicks) * (m_axisXMax - m_axisXMin);
        out += svgTxt(x, top + plotH + 22.0, fmtAxis(val), p.axisLabel, 11, false, "middle");
    }
    for (int i = 0; i <= yTicks; ++i) {
        const double y   = top + plotH - plotH * i / yTicks;
        const double val = (double(i) / yTicks) * m_axisYMax;
        out += svgTxt(left - 12.0, y + 4.0, QString::number(std::lround(val)),
                      p.axisLabel, 11, false, "end");
    }

    // bars, shaded by height (see histogramPeak)
    const double rx = m_axisXMax - m_axisXMin;
    if (rx > 0.0 && m_axisYMax > 0) {
        out += QString("<g stroke=\"%1\" stroke-width=\"1\" fill=\"%1\">\n").arg(p.series);
        for (int i = 0; i + 1 < m_points.size(); i += 2) {
            const QVariantMap a = m_points[i].toMap();
            const QVariantMap b = m_points[i + 1].toMap();
            const double c  = a["y"].toDouble();
            const double x0 = left + (a["x"].toDouble() - m_axisXMin) / rx * plotW;
            const double x1 = left + (b["x"].toDouble() - m_axisXMin) / rx * plotW;
            const double hh = c / m_axisYMax * plotH;
            const double frac = (m_histPeak > 0) ? c / m_histPeak : 0.0;
            out += QString("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" "
                           "fill-opacity=\"%5\"/>\n")
                       .arg(n2(x0), n2(top + plotH - hh),
                            n2(std::max(1.0, x1 - x0)), n2(hh),
                            n2(0.22 + 0.78 * frac));
        }
        out += "</g>\n";
    }

    // axis titles
    out += svgTxt(left + plotW / 2.0, H - 24.0,
                  m_axisXLabel.isEmpty() ? QStringLiteral("Value") : m_axisXLabel,
                  p.axisTitle, 14, false, "middle");
    out += QString("<text x=\"%1\" y=\"%2\" fill=\"%3\" font-family=\"Segoe UI, Arial, sans-serif\" "
                   "font-size=\"14\" text-anchor=\"middle\" transform=\"rotate(-90 %1 %2)\">"
                   "Frequency</text>\n")
               .arg(n2(left - 52.0), n2(top + plotH / 2.0), p.axisTitle);

    // descriptive statistics panel
    if (stats) {
        const double sx = left + plotW + 30.0, sy = top;
        const double sh = 30.0 + m_descStats.size() * 19.0;
        out += QString("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" rx=\"8\" fill=\"%5\" "
                       "stroke=\"%6\" stroke-width=\"1\"/>\n")
                   .arg(n2(sx), n2(sy), n2(statsW - 30.0), n2(sh), p.plot, p.plotBorder);
        out += svgTxt(sx + 14.0, sy + 20.0, QObject::tr("Statistics"), p.title, 13, true);
        double ry = sy + 40.0;
        for (const QVariant& v : m_descStats) {
            const QVariantMap m = v.toMap();
            out += svgTxt(sx + 14.0, ry, m["label"].toString(), p.axisLabel, 12);
            out += svgTxt(sx + statsW - 44.0, ry, m["value"].toString(), p.axisTitle, 12, true, "end");
            ry += 19.0;
        }
    }

    out += "</svg>\n";
    return out;
}

QString StatisticsController::toLocalFile(const QUrl& fileUrl) const
{
    return fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
}

bool StatisticsController::exportSvg(const QUrl& fileUrl, bool dark, bool withStats)
{
    if (!hasData()) {
        qWarning() << "StatisticsController::exportSvg: no histogram to export";
        return false;
    }

    const QString path = toLocalFile(fileUrl);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "StatisticsController::exportSvg: cannot write" << path << ":" << f.errorString();
        return false;
    }
    const QString svg = svgHistogram(dark, withStats);
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << svg;
    f.close();

    qDebug() << "StatisticsController: wrote SVG" << path << "(" << svg.size() << "chars,"
             << (dark ? "dark" : "light") << ")";
    return true;
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

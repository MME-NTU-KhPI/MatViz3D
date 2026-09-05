#include "deformed_state_analyzer.h"
#include "ansyswrapper.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

QStringList DeformedStateAnalyzer::availableProperties()
{
    return {
        QStringLiteral("von Mises Stress (SEQV)"),
        QStringLiteral("Normal Stress SX"),
        QStringLiteral("Normal Stress SY"),
        QStringLiteral("Normal Stress SZ"),
        QStringLiteral("Shear Stress SXY"),
        QStringLiteral("Shear Stress SYZ"),
        QStringLiteral("Shear Stress SXZ"),
        QStringLiteral("Hydrostatic Stress"),
        QStringLiteral("Stress Triaxiality"),
        QStringLiteral("von Mises Strain (EpsEQV)"),
        QStringLiteral("Normal Strain EpsX"),
        QStringLiteral("Normal Strain EpsY"),
        QStringLiteral("Normal Strain EpsZ"),
        QStringLiteral("Shear Strain EpsXY"),
        QStringLiteral("Shear Strain EpsYZ"),
        QStringLiteral("Shear Strain EpsXZ"),
        QStringLiteral("Displacement Magnitude (USUM)"),
        QStringLiteral("Displacement UX"),
        QStringLiteral("Displacement UY"),
        QStringLiteral("Displacement UZ")
    };
}

QString DeformedStateAnalyzer::propertyLabel(Property prop)
{
    switch (prop) {
    case Property::VonMisesStress:   return QStringLiteral("von Mises Stress");
    case Property::StressSX:          return QStringLiteral("SX");
    case Property::StressSY:          return QStringLiteral("SY");
    case Property::StressSZ:          return QStringLiteral("SZ");
    case Property::StressSXY:         return QStringLiteral("SXY");
    case Property::StressSYZ:         return QStringLiteral("SYZ");
    case Property::StressSXZ:         return QStringLiteral("SXZ");
    case Property::HydrostaticStress: return QStringLiteral("Hydrostatic Stress");
    case Property::StressTriaxiality: return QStringLiteral("Stress Triaxiality");
    case Property::VonMisesStrain:    return QStringLiteral("von Mises Strain");
    case Property::StrainEpsX:        return QStringLiteral("EpsX");
    case Property::StrainEpsY:        return QStringLiteral("EpsY");
    case Property::StrainEpsZ:        return QStringLiteral("EpsZ");
    case Property::StrainEpsXY:       return QStringLiteral("EpsXY");
    case Property::StrainEpsYZ:       return QStringLiteral("EpsYZ");
    case Property::StrainEpsXZ:       return QStringLiteral("EpsXZ");
    case Property::DisplacementUSUM:  return QStringLiteral("USUM");
    case Property::DisplacementUX:    return QStringLiteral("UX");
    case Property::DisplacementUY:    return QStringLiteral("UY");
    case Property::DisplacementUZ:    return QStringLiteral("UZ");
    }
    return QStringLiteral("Value");
}

QString DeformedStateAnalyzer::propertyTitle(Property prop)
{
    switch (prop) {
    case Property::VonMisesStress:   return QStringLiteral("Probability Density Function of von Mises Stress");
    case Property::StressSX:          return QStringLiteral("Probability Density Function of Normal Stress SX");
    case Property::StressSY:          return QStringLiteral("Probability Density Function of Normal Stress SY");
    case Property::StressSZ:          return QStringLiteral("Probability Density Function of Normal Stress SZ");
    case Property::StressSXY:         return QStringLiteral("Probability Density Function of Shear Stress SXY");
    case Property::StressSYZ:         return QStringLiteral("Probability Density Function of Shear Stress SYZ");
    case Property::StressSXZ:         return QStringLiteral("Probability Density Function of Shear Stress SXZ");
    case Property::HydrostaticStress: return QStringLiteral("Probability Density Function of Hydrostatic Stress");
    case Property::StressTriaxiality: return QStringLiteral("Probability Density Function of Stress Triaxiality");
    case Property::VonMisesStrain:    return QStringLiteral("Probability Density Function of von Mises Strain");
    case Property::StrainEpsX:        return QStringLiteral("Probability Density Function of Normal Strain EpsX");
    case Property::StrainEpsY:        return QStringLiteral("Probability Density Function of Normal Strain EpsY");
    case Property::StrainEpsZ:        return QStringLiteral("Probability Density Function of Normal Strain EpsZ");
    case Property::StrainEpsXY:       return QStringLiteral("Probability Density Function of Shear Strain EpsXY");
    case Property::StrainEpsYZ:       return QStringLiteral("Probability Density Function of Shear Strain EpsYZ");
    case Property::StrainEpsXZ:       return QStringLiteral("Probability Density Function of Shear Strain EpsXZ");
    case Property::DisplacementUSUM:  return QStringLiteral("Probability Density Function of Displacement Magnitude");
    case Property::DisplacementUX:    return QStringLiteral("Probability Density Function of Displacement UX");
    case Property::DisplacementUY:    return QStringLiteral("Probability Density Function of Displacement UY");
    case Property::DisplacementUZ:    return QStringLiteral("Probability Density Function of Displacement UZ");
    }
    return QStringLiteral("Deformed State Distribution");
}

QString DeformedStateAnalyzer::propertyUnits(Property prop)
{
    switch (prop) {
    case Property::VonMisesStress:
    case Property::StressSX:
    case Property::StressSY:
    case Property::StressSZ:
    case Property::StressSXY:
    case Property::StressSYZ:
    case Property::StressSXZ:
    case Property::HydrostaticStress:
        return QStringLiteral("Pa");
    case Property::StressTriaxiality:
    case Property::VonMisesStrain:
    case Property::StrainEpsX:
    case Property::StrainEpsY:
    case Property::StrainEpsZ:
    case Property::StrainEpsXY:
    case Property::StrainEpsYZ:
    case Property::StrainEpsXZ:
        return QStringLiteral("");
    case Property::DisplacementUSUM:
    case Property::DisplacementUX:
    case Property::DisplacementUY:
    case Property::DisplacementUZ:
        return QStringLiteral("m");
    }
    return QString();
}

QVector<float> DeformedStateAnalyzer::extractValues(const std::vector<std::vector<float>>& results,
                                                    int32_t*** voxels,
                                                    int cubeSize,
                                                    Property prop,
                                                    StatMode mode)
{
    QVector<float> values;
    if (results.empty()) return values;

    auto getRaw = [&](const std::vector<float>& row) -> float {
        if (row.empty()) return 0.0f;
        switch (prop) {
        case Property::StressSX:          return (SX < (int)row.size()) ? row[SX] : 0.0f;
        case Property::StressSY:          return (SY < (int)row.size()) ? row[SY] : 0.0f;
        case Property::StressSZ:          return (SZ < (int)row.size()) ? row[SZ] : 0.0f;
        case Property::StressSXY:         return (SXY < (int)row.size()) ? row[SXY] : 0.0f;
        case Property::StressSYZ:         return (SYZ < (int)row.size()) ? row[SYZ] : 0.0f;
        case Property::StressSXZ:         return (SXZ < (int)row.size()) ? row[SXZ] : 0.0f;
        case Property::VonMisesStress:    return (SEQV < (int)row.size()) ? row[SEQV] : 0.0f;
        case Property::HydrostaticStress: {
            float sx = (SX < (int)row.size()) ? row[SX] : 0.0f;
            float sy = (SY < (int)row.size()) ? row[SY] : 0.0f;
            float sz = (SZ < (int)row.size()) ? row[SZ] : 0.0f;
            return (sx + sy + sz) / 3.0f;
        }
        case Property::StressTriaxiality: {
            float sx = (SX < (int)row.size()) ? row[SX] : 0.0f;
            float sy = (SY < (int)row.size()) ? row[SY] : 0.0f;
            float sz = (SZ < (int)row.size()) ? row[SZ] : 0.0f;
            float seqv = (SEQV < (int)row.size()) ? row[SEQV] : 0.0f;
            float sh = (sx + sy + sz) / 3.0f;
            if (std::abs(seqv) < 1e-12f) return 0.0f;
            return sh / seqv;
        }
        case Property::StrainEpsX:        return (EpsX < (int)row.size()) ? row[EpsX] : 0.0f;
        case Property::StrainEpsY:        return (EpsY < (int)row.size()) ? row[EpsY] : 0.0f;
        case Property::StrainEpsZ:        return (EpsZ < (int)row.size()) ? row[EpsZ] : 0.0f;
        case Property::StrainEpsXY:       return (EpsXY < (int)row.size()) ? row[EpsXY] : 0.0f;
        case Property::StrainEpsYZ:       return (EpsYZ < (int)row.size()) ? row[EpsYZ] : 0.0f;
        case Property::StrainEpsXZ:       return (EpsXZ < (int)row.size()) ? row[EpsXZ] : 0.0f;
        case Property::VonMisesStrain:    return (EpsEQV < (int)row.size()) ? row[EpsEQV] : 0.0f;
        case Property::DisplacementUX:    return (UX < (int)row.size()) ? row[UX] : 0.0f;
        case Property::DisplacementUY:    return (UY < (int)row.size()) ? row[UY] : 0.0f;
        case Property::DisplacementUZ:    return (UZ < (int)row.size()) ? row[UZ] : 0.0f;
        case Property::DisplacementUSUM:  return (USUM < (int)row.size()) ? row[USUM] : 0.0f;
        }
        return 0.0f;
    };

    if (mode == StatMode::FullVolume || !voxels || cubeSize <= 0) {
        values.reserve(results.size());
        for (const auto& row : results) {
            float v = getRaw(row);
            if (!std::isnan(v) && !std::isinf(v))
                values.push_back(v);
        }
        return values;
    }

    // PerGrainMean mode: average values within each grain ID
    std::map<int32_t, double> grainSum;
    std::map<int32_t, int>    grainCount;

    for (const auto& row : results) {
        if (row.size() <= Z) continue;
        int ix = std::clamp(static_cast<int>(std::round(row[X])), 0, cubeSize - 1);
        int iy = std::clamp(static_cast<int>(std::round(row[Y])), 0, cubeSize - 1);
        int iz = std::clamp(static_cast<int>(std::round(row[Z])), 0, cubeSize - 1);

        int32_t gid = voxels[ix][iy][iz];
        if (gid <= 0) continue;

        float v = getRaw(row);
        if (!std::isnan(v) && !std::isinf(v)) {
            grainSum[gid] += v;
            grainCount[gid]++;
        }
    }

    values.reserve(grainSum.size());
    for (const auto& [gid, sum] : grainSum) {
        int cnt = grainCount[gid];
        if (cnt > 0)
            values.push_back(static_cast<float>(sum / cnt));
    }

    return values;
}

DeformedStateAnalyzer::DescriptiveStats DeformedStateAnalyzer::computeStats(const QVector<float>& values)
{
    DescriptiveStats s;
    const int n = values.size();
    s.n = n;
    if (n == 0) return s;

    double sum = 0.0;
    for (float v : values) sum += v;
    const double mean = sum / n;
    s.mean = mean;

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

    s.variance = (n > 1) ? (m2 * n) / (n - 1) : 0.0;
    s.stdDev   = std::sqrt(s.variance);
    s.cv       = (std::abs(mean) > 1e-30) ? (s.stdDev / std::abs(mean) * 100.0) : 0.0;

    const double sigma = std::sqrt(m2);
    s.skewness = (sigma > 1e-30) ? m3 / (sigma * sigma * sigma) : 0.0;
    s.kurtosis = (m2 > 1e-30) ? m4 / (m2 * m2) - 3.0 : 0.0;

    QVector<float> sorted = values;
    std::sort(sorted.begin(), sorted.end());

    s.minVal = sorted.first();
    s.maxVal = sorted.last();

    auto quantile = [&](double q) -> double {
        if (n == 1) return sorted[0];
        double idx = q * (n - 1);
        int low = static_cast<int>(std::floor(idx));
        int high = static_cast<int>(std::ceil(idx));
        double frac = idx - low;
        return sorted[low] * (1.0 - frac) + sorted[high] * frac;
    };

    s.median = quantile(0.50);
    s.q1     = quantile(0.25);
    s.q3     = quantile(0.75);
    s.iqr    = s.q3 - s.q1;
    s.p1     = quantile(0.01);
    s.p99    = quantile(0.99);

    return s;
}

DeformedStateAnalyzer::HistogramResult DeformedStateAnalyzer::computeHistogramAndKDE(const QVector<float>& values, int binCount)
{
    HistogramResult res;
    if (values.isEmpty()) {
        res.axisXMin = 0.0;
        res.axisXMax = 1.0;
        res.axisYMax = 10;
        return res;
    }

    const int n = values.size();
    const float minV = *std::min_element(values.constBegin(), values.constEnd());
    const float maxV = *std::max_element(values.constBegin(), values.constEnd());

    int bins = (binCount > 0) ? binCount : std::clamp((int)std::round(std::sqrt((double)n) / 2.0), 5, 80);

    // Single value case
    if (std::abs(maxV - minV) < 1e-12f) {
        res.barPoints.append(QVariantMap{ {"x", (double)minV - 0.5}, {"y", (double)n} });
        res.barPoints.append(QVariantMap{ {"x", (double)minV + 0.5}, {"y", (double)n} });
        res.kdePoints.append(QVariantMap{ {"x", (double)minV - 0.5}, {"y", 0.0} });
        res.kdePoints.append(QVariantMap{ {"x", (double)minV}, {"y", 1.0} });
        res.kdePoints.append(QVariantMap{ {"x", (double)minV + 0.5}, {"y", 0.0} });

        res.axisXMin = (minV == 0.0f) ? -1.0 : ((minV > 0) ? 0.0 : minV - 1.0);
        res.axisXMax = (minV == 0.0f) ? 1.0 : ((minV > 0) ? minV + 1.0 : 0.0);
        res.axisYMax = n;
        res.histPeak = n;
        res.kdeMax   = 1.0;
        return res;
    }

    // Range spans positive and negative
    if (minV < 0.0f && maxV > 0.0f) {
        const double maxAbs = std::max(std::abs(minV), std::abs(maxV));
        const int halfBins = std::max(2, (bins + 1) / 2);
        const double binWidth = maxAbs / halfBins;
        const int totalBins = halfBins * 2;

        QVector<int> binCounts(totalBins, 0);
        for (float v : values) {
            int idx = static_cast<int>(std::floor(v / binWidth)) + halfBins;
            idx = std::clamp(idx, 0, totalBins - 1);
            binCounts[idx]++;
        }

        int maxCount = 0;
        for (int i = 0; i < totalBins; ++i) {
            const double xStart = (i - halfBins) * binWidth;
            const double xEnd   = xStart + binWidth;
            const int    c      = binCounts[i];

            res.barPoints.append(QVariantMap{ {"x", xStart}, {"y", c} });
            res.barPoints.append(QVariantMap{ {"x", xEnd},   {"y", c} });
            if (c > maxCount) maxCount = c;
        }

        const double axisExtent = (halfBins + 1) * binWidth;
        res.axisXMin = -axisExtent;
        res.axisXMax = +axisExtent;
        res.axisYMax = (maxCount / 10 + 1) * 10 + 10;
        res.histPeak = maxCount;
    } else {
        // Strictly non-negative or non-positive
        const float binWidth = (maxV - minV) / bins;
        QVector<int> binCounts(bins, 0);
        for (float v : values) {
            int idx = static_cast<int>((v - minV) / binWidth);
            idx = std::clamp(idx, 0, bins - 1);
            binCounts[idx]++;
        }

        int maxCount = 0;
        for (int i = 0; i < bins; ++i) {
            const double xStart = minV + i * binWidth;
            const double xEnd   = xStart + binWidth;
            const int    c      = binCounts[i];

            res.barPoints.append(QVariantMap{ {"x", xStart}, {"y", c} });
            res.barPoints.append(QVariantMap{ {"x", xEnd},   {"y", c} });
            if (c > maxCount) maxCount = c;
        }

        if (minV >= 0.0f) {
            res.axisXMin = std::max(0.0, (double)minV - binWidth);
            res.axisXMax = (double)maxV + binWidth;
        } else {
            res.axisXMin = (double)minV - binWidth;
            res.axisXMax = std::min(0.0, (double)maxV + binWidth);
        }
        res.axisYMax = (maxCount / 10 + 1) * 10 + 10;
        res.histPeak = maxCount;
    }

    // ── Gaussian Kernel Density Estimation (KDE) ──────────────────────────
    // Robust Silverman's rule of thumb: h = 0.9 * min(sigma, IQR / 1.34) * N^(-1/5)
    double sum = 0.0, sumSq = 0.0;
    for (float v : values) { sum += v; sumSq += v * v; }
    double mean = sum / n;
    double var = (n > 1) ? (sumSq - n * mean * mean) / (n - 1) : 0.0;
    double sigma = std::sqrt(std::max(0.0, var));

    QVector<float> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    int q1Idx = static_cast<int>(std::floor(0.25 * (n - 1)));
    int q3Idx = static_cast<int>(std::ceil(0.75 * (n - 1)));
    double iqr = (n > 1) ? static_cast<double>(sorted[q3Idx] - sorted[q1Idx]) : 0.0;

    double spread = sigma;
    if (iqr > 1e-12 && (iqr / 1.34) < spread) {
        spread = iqr / 1.34;
    }
    double h = 0.9 * spread * std::pow(static_cast<double>(n), -0.2);
    if (h < 1e-12) h = (res.axisXMax - res.axisXMin) / (bins * 2.0);

    const int numKdePoints = 160;
    const double kdeStep = (res.axisXMax - res.axisXMin) / (numKdePoints - 1);
    const double invSqrt2Pi = 1.0 / std::sqrt(2.0 * M_PI);
    const double invH = 1.0 / h;

    // Effective bin width for scaling probability density to frequency count units
    // (such that the integral under the KDE curve equals the total count N)
    const double effectiveBinWidth = (res.axisXMax - res.axisXMin) / bins;

    double kdeMax = 0.0;
    for (int k = 0; k < numKdePoints; ++k) {
        double x = res.axisXMin + k * kdeStep;
        double density = 0.0;
        for (float v : values) {
            double u = (x - v) * invH;
            density += std::exp(-0.5 * u * u);
        }
        density *= (invSqrt2Pi * invH / n);
        // Scale probability density to expected count per bin width
        double countDensity = density * n * effectiveBinWidth;
        res.kdePoints.append(QVariantMap{ {"x", x}, {"y", countDensity} });
        if (countDensity > kdeMax) kdeMax = countDensity;
    }
    res.kdeMax = kdeMax;

    // Ensure axisYMax covers both histogram peak and KDE curve peak comfortably
    int neededPeak = static_cast<int>(std::ceil(std::max(static_cast<double>(res.histPeak), kdeMax)));
    if (neededPeak > res.axisYMax - 10) {
        res.axisYMax = (neededPeak / 10 + 1) * 10 + 10;
    }

    return res;
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
}

QVariantList DeformedStateAnalyzer::statsToVariantList(const DescriptiveStats& stats, Property prop)
{
    QVariantList list;
    auto add = [&](const QString& label, const QString& val) {
        list.append(QVariantMap{ {"label", label}, {"value", val} });
    };

    const QString unit = propertyUnits(prop);
    const QString unitSuffix = unit.isEmpty() ? QString() : (" " + unit);

    add(QStringLiteral("Count (N)"),    QString::number(stats.n));
    add(QStringLiteral("Mean (μ)"),     fmtNum(stats.mean) + unitSuffix);
    add(QStringLiteral("Std Dev (σ)"),  fmtNum(stats.stdDev) + unitSuffix);
    add(QStringLiteral("CV (%)"),       QString::number(stats.cv, 'f', 1) + QStringLiteral(" %"));
    add(QStringLiteral("Median"),       fmtNum(stats.median) + unitSuffix);
    add(QStringLiteral("IQR (Q3 - Q1)"),fmtNum(stats.iqr) + unitSuffix);
    add(QStringLiteral("Min"),          fmtNum(stats.minVal) + unitSuffix);
    add(QStringLiteral("Max"),          fmtNum(stats.maxVal) + unitSuffix);
    add(QStringLiteral("1st Percentile"), fmtNum(stats.p1) + unitSuffix);
    add(QStringLiteral("99th Percentile"),fmtNum(stats.p99) + unitSuffix);
    add(QStringLiteral("Skewness (γ1)"),QString::number(stats.skewness, 'f', 3));
    add(QStringLiteral("Kurtosis (β2)"),QString::number(stats.kurtosis, 'f', 3));

    return list;
}

namespace {
struct SvgTheme {
    QString bg, plotBg, border, grid, tick, title, axisTitle, label, barFill, kdeStroke;
    double gridAlpha;
};

SvgTheme getSvgTheme(bool dark)
{
    if (dark) {
        return { "#282828", "#1e1e1e", "#4a4a4a", "#ffffff", "#969696",
                 "#d9d9d9", "#c6c6c6", "#969696", "#00897b", "#4fc3f7", 0.18 };
    }
    return     { "#f2f2f2", "#ffffff", "#b0b0b0", "#000000", "#555555",
                 "#1a1a1a", "#333333", "#555555", "#00564d", "#0288d1", 0.12 };
}

QString n2s(double v, int prec = 2) { return QString::number(v, 'f', prec); }

QString fmtSvgAxis(double v)
{
    if (std::abs(v) < 1e-9) return QStringLiteral("0");
    if (std::abs(v) >= 10000.0 || std::abs(v) < 0.001)
        return QString::number(v, 'e', 2);
    return QString::number(QString::number(v, 'g', 4).toDouble());
}

QString svgText(double x, double y, const QString& text, const QString& color, int size = 12, bool bold = false, const QString& anchor = "start")
{
    return QString("<text x=\"%1\" y=\"%2\" fill=\"%3\" font-family=\"Segoe UI, Montserrat, Arial, sans-serif\" "
                   "font-size=\"%4\" %5text-anchor=\"%6\">%7</text>\n")
        .arg(n2s(x), n2s(y), color).arg(size)
        .arg(bold ? "font-weight=\"bold\" " : "", anchor, text.toHtmlEscaped());
}
}

QString DeformedStateAnalyzer::generateSvg(const QString& title,
                                          const QString& axisXLabel,
                                          const HistogramResult& hist,
                                          const DescriptiveStats& stats,
                                          bool dark,
                                          bool withStats)
{
    const SvgTheme theme = getSvgTheme(dark);
    const double left = 90.0, top = 65.0, plotW = 760.0, plotH = 420.0;
    const double statsW = withStats ? 240.0 : 0.0;
    const double W = left + plotW + 40.0 + statsW;
    const double H = top + plotH + 80.0;
    const int xTicks = 8, yTicks = 6;

    QString svg = QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                          "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%2\" viewBox=\"0 0 %1 %2\">\n"
                          "<rect width=\"%1\" height=\"%2\" fill=\"%3\"/>\n")
                      .arg(n2s(W, 0), n2s(H, 0), theme.bg);

    // Title
    svg += svgText(left + plotW / 2.0, 36.0, title, theme.title, 18, true, "middle");

    // Plot background
    svg += QString("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" fill=\"%5\" stroke=\"%6\" stroke-width=\"1\"/>\n")
               .arg(n2s(left), n2s(top), n2s(plotW), n2s(plotH), theme.plotBg, theme.border);

    // Gridlines & Ticks
    svg += QString("<g stroke=\"%1\" stroke-opacity=\"%2\" stroke-width=\"1\">\n").arg(theme.grid, n2s(theme.gridAlpha));
    for (int i = 0; i <= xTicks; ++i) {
        const double x = left + plotW * i / xTicks;
        svg += QString("<line x1=\"%1\" y1=\"%2\" x2=\"%1\" y2=\"%3\"/>\n").arg(n2s(x), n2s(top), n2s(top + plotH));
    }
    for (int i = 0; i <= yTicks; ++i) {
        const double y = top + plotH - plotH * i / yTicks;
        svg += QString("<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%2\"/>\n").arg(n2s(left), n2s(y), n2s(left + plotW));
    }
    svg += "</g>\n";

    // Tick marks
    svg += QString("<g stroke=\"%1\" stroke-width=\"1\">\n").arg(theme.tick);
    for (int i = 0; i <= xTicks; ++i) {
        const double x = left + plotW * i / xTicks;
        svg += QString("<line x1=\"%1\" y1=\"%2\" x2=\"%1\" y2=\"%3\"/>\n").arg(n2s(x), n2s(top + plotH), n2s(top + plotH + 6.0));
    }
    for (int i = 0; i <= yTicks; ++i) {
        const double y = top + plotH - plotH * i / yTicks;
        svg += QString("<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%2\"/>\n").arg(n2s(left - 6.0), n2s(y), n2s(left));
    }
    if (hist.axisXMin < 0.0 && hist.axisXMax > 0.0) {
        const double xZero = left + (-hist.axisXMin / (hist.axisXMax - hist.axisXMin)) * plotW;
        svg += QString("<line x1=\"%1\" y1=\"%2\" x2=\"%1\" y2=\"%3\" stroke=\"%4\" stroke-width=\"1.5\"/>\n")
                   .arg(n2s(xZero), n2s(top), n2s(top + plotH), theme.tick);
    }
    svg += "</g>\n";

    // Axis Labels
    for (int i = 0; i <= xTicks; ++i) {
        const double x   = left + plotW * i / xTicks;
        const double val = hist.axisXMin + (double(i) / xTicks) * (hist.axisXMax - hist.axisXMin);
        svg += svgText(x, top + plotH + 22.0, fmtSvgAxis(val), theme.label, 11, false, "middle");
    }
    for (int i = 0; i <= yTicks; ++i) {
        const double y   = top + plotH - plotH * i / yTicks;
        const double val = (double(i) / yTicks) * hist.axisYMax;
        svg += svgText(left - 12.0, y + 4.0, QString::number(std::lround(val)), theme.label, 11, false, "end");
    }

    // Histogram Bars
    const double rx = hist.axisXMax - hist.axisXMin;
    if (rx > 0.0 && hist.axisYMax > 0) {
        svg += QString("<g stroke=\"%1\" stroke-width=\"1\" fill=\"%1\">\n").arg(theme.barFill);
        for (int i = 0; i + 1 < hist.barPoints.size(); i += 2) {
            const QVariantMap a = hist.barPoints[i].toMap();
            const QVariantMap b = hist.barPoints[i + 1].toMap();
            const double c  = a["y"].toDouble();
            const double x0 = left + (a["x"].toDouble() - hist.axisXMin) / rx * plotW;
            const double x1 = left + (b["x"].toDouble() - hist.axisXMin) / rx * plotW;
            const double hh = (c / hist.axisYMax) * plotH;
            const double frac = (hist.histPeak > 0) ? c / hist.histPeak : 0.0;
            svg += QString("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" fill-opacity=\"%5\"/>\n")
                       .arg(n2s(x0), n2s(top + plotH - hh), n2s(std::max(1.0, x1 - x0)), n2s(hh), n2s(0.25 + 0.75 * frac));
        }
        svg += "</g>\n";
    }

    // KDE Overlay Curve
    if (!hist.kdePoints.isEmpty() && hist.axisYMax > 0) {
        svg += QString("<path d=\"");
        for (int i = 0; i < hist.kdePoints.size(); ++i) {
            const QVariantMap pt = hist.kdePoints[i].toMap();
            const double px = left + (pt["x"].toDouble() - hist.axisXMin) / rx * plotW;
            const double py = top + plotH - (pt["y"].toDouble() / hist.axisYMax) * plotH;
            svg += QString("%1%2,%3 ").arg((i == 0) ? "M " : "L ", n2s(px), n2s(py));
        }
        svg += QString("\" fill=\"none\" stroke=\"%1\" stroke-width=\"2.5\" stroke-linecap=\"round\"/>\n").arg(theme.kdeStroke);
    }

    // Axis titles
    svg += svgText(left + plotW / 2.0, H - 22.0, axisXLabel.isEmpty() ? QStringLiteral("Value") : axisXLabel, theme.axisTitle, 14, false, "middle");
    svg += QString("<text x=\"%1\" y=\"%2\" fill=\"%3\" font-family=\"Segoe UI, Montserrat, Arial, sans-serif\" "
                   "font-size=\"14\" text-anchor=\"middle\" transform=\"rotate(-90 %1 %2)\">Frequency</text>\n")
               .arg(n2s(left - 52.0), n2s(top + plotH / 2.0), theme.axisTitle);

    // Statistics card
    if (withStats) {
        const double sx = left + plotW + 25.0, sy = top;
        QVariantList statsList = statsToVariantList(stats, Property::VonMisesStress);
        const double sh = 34.0 + statsList.size() * 20.0;
        svg += QString("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" rx=\"8\" fill=\"%5\" stroke=\"%6\" stroke-width=\"1\"/>\n")
                   .arg(n2s(sx), n2s(sy), n2s(statsW - 25.0), n2s(sh), theme.plotBg, theme.border);
        svg += svgText(sx + 14.0, sy + 22.0, QStringLiteral("Deformed Statistics"), theme.title, 13, true);

        double ry = sy + 44.0;
        for (const QVariant& v : statsList) {
            const QVariantMap m = v.toMap();
            svg += svgText(sx + 14.0, ry, m["label"].toString(), theme.label, 11);
            svg += svgText(sx + statsW - 39.0, ry, m["value"].toString(), theme.title, 11, true, "end");
            ry += 20.0;
        }
    }

    svg += "</svg>\n";
    return svg;
}

bool DeformedStateAnalyzer::exportToCsv(const QString& filePath,
                                       const std::vector<std::vector<float>>& results,
                                       int32_t*** voxels,
                                       int cubeSize)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "DeformedStateAnalyzer::exportToCsv: cannot open" << filePath;
        return false;
    }

    QTextStream out(&file);
    out << "ID;X;Y;Z;GrainID;UX;UY;UZ;SX;SY;SZ;SXY;SYZ;SXZ;EpsX;EpsY;EpsZ;EpsXY;EpsYZ;EpsXZ;USUM;SEQV;EpsEQV;HydrostaticStress;StressTriaxiality\n";

    for (const auto& row : results) {
        if (row.size() <= Z) continue;
        int ix = std::clamp(static_cast<int>(std::round(row[X])), 0, cubeSize > 0 ? cubeSize - 1 : 0);
        int iy = std::clamp(static_cast<int>(std::round(row[Y])), 0, cubeSize > 0 ? cubeSize - 1 : 0);
        int iz = std::clamp(static_cast<int>(std::round(row[Z])), 0, cubeSize > 0 ? cubeSize - 1 : 0);
        int32_t gid = (voxels && cubeSize > 0) ? voxels[ix][iy][iz] : 0;

        float sx = (SX < (int)row.size()) ? row[SX] : 0.0f;
        float sy = (SY < (int)row.size()) ? row[SY] : 0.0f;
        float sz = (SZ < (int)row.size()) ? row[SZ] : 0.0f;
        float seqv = (SEQV < (int)row.size()) ? row[SEQV] : 0.0f;
        float sh = (sx + sy + sz) / 3.0f;
        float triax = (std::abs(seqv) > 1e-12f) ? sh / seqv : 0.0f;

        out << (row.size() > ID ? row[ID] : 0) << ";"
            << row[X] << ";" << row[Y] << ";" << row[Z] << ";"
            << gid << ";"
            << (row.size() > UX ? row[UX] : 0) << ";"
            << (row.size() > UY ? row[UY] : 0) << ";"
            << (row.size() > UZ ? row[UZ] : 0) << ";"
            << sx << ";" << sy << ";" << sz << ";"
            << (row.size() > SXY ? row[SXY] : 0) << ";"
            << (row.size() > SYZ ? row[SYZ] : 0) << ";"
            << (row.size() > SXZ ? row[SXZ] : 0) << ";"
            << (row.size() > EpsX ? row[EpsX] : 0) << ";"
            << (row.size() > EpsY ? row[EpsY] : 0) << ";"
            << (row.size() > EpsZ ? row[EpsZ] : 0) << ";"
            << (row.size() > EpsXY ? row[EpsXY] : 0) << ";"
            << (row.size() > EpsYZ ? row[EpsYZ] : 0) << ";"
            << (row.size() > EpsXZ ? row[EpsXZ] : 0) << ";"
            << (row.size() > USUM ? row[USUM] : 0) << ";"
            << seqv << ";"
            << (row.size() > EpsEQV ? row[EpsEQV] : 0) << ";"
            << sh << ";"
            << triax << "\n";
    }

    file.close();
    qDebug() << "DeformedStateAnalyzer: exported CSV to" << filePath << "with" << results.size() << "rows";
    return true;
}

#include "exportcontroller.h"
#include "openglwidgetqml.h"
#include "parameters.h"
#include "hdf5wrapper.h"
#include "loadstepmanager.h"
#include "parent_algorithm.h"
#include "stressanalysiscontroller.h"
#include "hdf5projectcontroller.h"
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QFileDialog>
#include <QDir>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QPainter>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <cmath>
#include <algorithm>
#include <QVector3D>

namespace {

/**
 * Replacing the background with white. The background color is taken from the corner pixel;
 * comparison with a tolerance—otherwise, anti-aliased pixels would be missed.
 */
QImage makeWhiteBackground(const QImage& src)
{
    if (src.isNull())
        return src;

    QImage img = src.convertToFormat(QImage::Format_RGB32);
    const QColor bg = img.pixelColor(0, 0);      // background = top-left corner

    const int tol = 6;                            // tolerance in units 0..255
    const int br = bg.red(), bgr = bg.green(), bb = bg.blue();

    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb p = line[x];
            if (std::abs(qRed(p)   - br)  <= tol &&
                std::abs(qGreen(p) - bgr) <= tol &&
                std::abs(qBlue(p)  - bb)  <= tol)
            {
                line[x] = qRgb(255, 255, 255);
            }
        }
    }
    return img;
}

} // anonymous namespace


ExportController::ExportController(QObject* parent)
    : QObject(parent)
{
}

// ═══════════════════════════════════════════════════════════════════
//  DPI & Colorbar Drawing Helpers
// ═══════════════════════════════════════════════════════════════════
namespace {

void setDpiMetadata(QImage& img, int dpi)
{
    if (dpi > 0) {
        const int dpm = qRound(dpi * (100.0 / 2.54)); // e.g. 300 dpi -> 11811 dpm
        img.setDotsPerMeterX(dpm);
        img.setDotsPerMeterY(dpm);
    }
}

void drawColorBarCard(QPainter& p, const QRectF& cardRect, qreal scale, bool vertical,
                      const QString& compName, double vmin, double vmax,
                      const QVector<QColor>& colors, bool darkTheme)
{
    auto formatVal = [](double v) -> QString {
        if (std::isnan(v)) return QStringLiteral("0");
        if (std::abs(v) >= 10000.0 || (std::abs(v) < 0.001 && v != 0.0)) {
            return QString::number(v, 'e', 3);
        }
        return QString::number(v, 'g', 5);
    };

    if (!vertical) {
        // ── Horizontal Color Bar Card ──────────────────────────────────────
        const qreal padX = 14.0 * scale;
        const qreal padY = 10.0 * scale;
        const qreal contentW = cardRect.width() - 2.0 * padX;
        const qreal titleH = 16.0 * scale;
        const qreal gap1 = 6.0 * scale;
        const qreal barH = 14.0 * scale;
        const qreal valuesH = 16.0 * scale;

        // Card background
        p.setPen(darkTheme ? QPen(QColor(80, 80, 80, 220), 1.0 * scale)
                           : QPen(QColor(180, 180, 180, 220), 1.0 * scale));
        p.setBrush(darkTheme ? QColor(35, 35, 35, 240)
                             : QColor(255, 255, 255, 245));
        p.drawRoundedRect(cardRect, 8.0 * scale, 8.0 * scale);

        // Title
        QFont titleFont("Segoe UI", qMax(8, qRound(10.5 * scale)), QFont::DemiBold);
        p.setFont(titleFont);
        p.setPen(darkTheme ? QColor(230, 230, 230) : QColor(25, 25, 25));
        QRectF titleRect(cardRect.x() + padX, cardRect.y() + padY, contentW, titleH);
        p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, compName);

        // Color Bar
        const qreal barX = cardRect.x() + padX;
        const qreal barY = cardRect.y() + padY + titleH + gap1;
        const qreal segW = contentW / 9.0;

        p.setPen(Qt::NoPen);
        for (int i = 0; i < 9; ++i) {
            p.setBrush(colors[i]);
            QRectF segRect(barX + i * segW, barY, segW + 0.5, barH);
            p.drawRect(segRect);
        }

        // Color Bar outline
        p.setPen(QPen(QColor(100, 100, 100, 200), 1.0 * scale));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(barX, barY, contentW, barH));

        // Ticks and Values below color bar
        const qreal tickLen = 3.5 * scale;
        const qreal tickY1 = barY + barH;
        const qreal tickY2 = tickY1 + tickLen;
        const qreal labelY = tickY2 + 1.0 * scale;

        QFont valFont("Segoe UI", qMax(7, qRound(9.0 * scale)), QFont::Normal);
        QFont valBoldFont("Segoe UI", qMax(7, qRound(9.0 * scale)), QFont::DemiBold);
        p.setPen(QPen(darkTheme ? QColor(160, 160, 160) : QColor(90, 90, 90), 1.0 * scale));

        const qreal tickFractions[5] = { 0.0, 0.25, 0.50, 0.75, 1.0 };
        const int levelIndices[5] = { 0, 2, 4, 6, 8 };

        for (int t = 0; t < 5; ++t) {
            const qreal tx = barX + tickFractions[t] * contentW;
            p.drawLine(QPointF(tx, tickY1), QPointF(tx, tickY2));
        }

        // Labels
        p.setPen(darkTheme ? QColor(220, 220, 220) : QColor(30, 30, 30));
        const qreal labelW = 85.0 * scale;

        for (int t = 0; t < 5; ++t) {
            const int lvl = levelIndices[t];
            const double val = vmin + (vmax - vmin) * (lvl / 8.0);
            const QString str = formatVal(val);

            if (t == 0 || t == 4) p.setFont(valBoldFont);
            else p.setFont(valFont);

            QRectF textRect;
            int alignFlags = Qt::AlignVCenter;
            if (t == 0) {
                textRect = QRectF(barX, labelY, labelW, valuesH);
                alignFlags |= Qt::AlignLeft;
            } else if (t == 4) {
                textRect = QRectF(barX + contentW - labelW, labelY, labelW, valuesH);
                alignFlags |= Qt::AlignRight;
            } else {
                const qreal cx = barX + tickFractions[t] * contentW;
                textRect = QRectF(cx - labelW / 2.0, labelY, labelW, valuesH);
                alignFlags |= Qt::AlignHCenter;
            }
            p.drawText(textRect, alignFlags, str);
        }
    } else {
        // ── Vertical Color Bar Card ────────────────────────────────────────
        const qreal padX = 12.0 * scale;
        const qreal padY = 10.0 * scale;
        const qreal contentW = cardRect.width() - 2.0 * padX;
        const qreal titleH = 16.0 * scale;
        const qreal gap1 = 6.0 * scale;
        const qreal rowH = 14.0 * scale;
        const qreal rowGap = 3.0 * scale;

        // Card background
        p.setPen(darkTheme ? QPen(QColor(80, 80, 80, 220), 1.0 * scale)
                           : QPen(QColor(180, 180, 180, 220), 1.0 * scale));
        p.setBrush(darkTheme ? QColor(35, 35, 35, 240)
                             : QColor(255, 255, 255, 245));
        p.drawRoundedRect(cardRect, 8.0 * scale, 8.0 * scale);

        // Title
        QFont titleFont("Segoe UI", qMax(8, qRound(10.5 * scale)), QFont::DemiBold);
        p.setFont(titleFont);
        p.setPen(darkTheme ? QColor(230, 230, 230) : QColor(25, 25, 25));
        QRectF titleRect(cardRect.x() + padX, cardRect.y() + padY, contentW, titleH);
        p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, compName);

        const qreal swatchW = 22.0 * scale;
        const qreal swatchH = 12.0 * scale;
        const qreal swatchX = cardRect.x() + padX;
        const qreal textX = swatchX + swatchW + 8.0 * scale;
        const qreal textW = contentW - swatchW - 8.0 * scale;

        QFont valFont("Segoe UI", qMax(7, qRound(9.0 * scale)), QFont::Normal);
        QFont valBoldFont("Segoe UI", qMax(7, qRound(9.0 * scale)), QFont::DemiBold);

        for (int i = 0; i < 9; ++i) {
            const int lvl = 8 - i; // top is level 8 (Max), bottom is level 0 (Min)
            const double val = vmin + (vmax - vmin) * (lvl / 8.0);
            const qreal ry = cardRect.y() + padY + titleH + gap1 + i * (rowH + rowGap);

            // Swatch
            p.setPen(QPen(QColor(120, 120, 120, 200), 0.5 * scale));
            p.setBrush(colors[lvl]);
            p.drawRoundedRect(QRectF(swatchX, ry + (rowH - swatchH) / 2.0, swatchW, swatchH), 2.0 * scale, 2.0 * scale);

            // Text
            if (lvl == 8 || lvl == 0) p.setFont(valBoldFont);
            else p.setFont(valFont);

            p.setPen(darkTheme ? QColor(220, 220, 220) : QColor(30, 30, 30));
            QString str = formatVal(val);
            if (lvl == 8) str += QStringLiteral(" (Max)");
            else if (lvl == 0) str += QStringLiteral(" (Min)");

            p.drawText(QRectF(textX, ry, textW, rowH), Qt::AlignLeft | Qt::AlignVCenter, str);
        }
    }
}

void drawOrientationGizmo(QPainter& p, const QPointF& origin, qreal scale, OpenGLWidgetQML* ogl, bool /*darkTheme*/)
{
    QPointF o(0, 0);
    QPointF ptX(1, 0), ptY(0, 1), ptZ(0, 0);
    if (ogl) {
        o   = ogl->projectAxisLabel(QVector3D(0, 0, 0));
        ptX = ogl->projectAxisLabel(QVector3D(1, 0, 0));
        ptY = ogl->projectAxisLabel(QVector3D(0, 1, 0));
        ptZ = ogl->projectAxisLabel(QVector3D(0, 0, 1));
    } else {
        // Fallback default dimetric view
        ptX = QPointF(0.595, 0.266);
        ptY = QPointF(0.0, 0.547);
        ptZ = QPointF(-0.595, 0.266);
    }

    auto norm = [](const QPointF& v) -> QPointF {
        qreal len = std::hypot(v.x(), v.y());
        if (len < 1e-6) return QPointF(0, 0);
        return QPointF(v.x() / len, v.y() / len);
    };

    QPointF vX = norm(ptX - o);
    QPointF vY = norm(ptY - o);
    QPointF vZ = norm(ptZ - o);

    const qreal axisLen = 30.0 * scale;
    const QPointF endX = origin + vX * axisLen;
    const QPointF endY = origin + vY * axisLen;
    const QPointF endZ = origin + vZ * axisLen;

    const QColor colX(235, 45, 45);
    const QColor colY(40, 185, 40);
    const QColor colZ(40, 120, 245);

    const qreal penW = qMax(1.5, 2.2 * scale);

    // Draw shafts
    p.setPen(QPen(colX, penW, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(origin, endX);

    p.setPen(QPen(colY, penW, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(origin, endY);

    p.setPen(QPen(colZ, penW, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(origin, endZ);

    // Arrow ticks at tips
    auto drawArrow = [&](const QPointF& tip, const QPointF& dir, const QColor& col) {
        QPointF perp(-dir.y(), dir.x());
        qreal aLen = 7.0 * scale;
        qreal aWidth = 4.0 * scale;
        QPointF back = tip - dir * aLen;
        p.setPen(QPen(col, qMax(1.2, 1.8 * scale), Qt::SolidLine, Qt::RoundCap));
        p.drawLine(tip, back + perp * aWidth);
        p.drawLine(tip, back - perp * aWidth);
    };

    drawArrow(endX, vX, colX);
    drawArrow(endY, vY, colY);
    drawArrow(endZ, vZ, colZ);

    // Labels X, Y, Z
    QFont labelFont("Segoe UI", qMax(8, qRound(10.5 * scale)), QFont::Bold);
    p.setFont(labelFont);
    const qreal textOffset = 13.0 * scale;
    const qreal boxSize = 24.0 * scale;

    auto drawLabel = [&](const QPointF& end, const QPointF& dir, const QColor& col, const QString& text) {
        QPointF pos = end + dir * textOffset;
        p.setPen(col);
        p.drawText(QRectF(pos.x() - boxSize / 2.0, pos.y() - boxSize / 2.0, boxSize, boxSize),
                   Qt::AlignCenter, text);
    };

    drawLabel(endX, vX, colX, QStringLiteral("X"));
    drawLabel(endY, vY, colY, QStringLiteral("Y"));
    drawLabel(endZ, vZ, colZ, QStringLiteral("Z"));
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════
//  Colorbar Overlay for Stress/Strain Screenshots
// ═══════════════════════════════════════════════════════════════════
void ExportController::overlayColorBar(QImage& img, bool vertical, QQuickItem* item)
{
    if (img.isNull()) return;

    OpenGLWidgetQML* ogl = qobject_cast<OpenGLWidgetQML*>(item);
    if (!ogl) ogl = OpenGLWidgetQML::getInstance();

    bool hasField = (ogl && ogl->hasField());
    if (!hasField) {
        LoadStepManager& lsm = LoadStepManager::getInstance();
        if (lsm.hasLoadStepData()) {
            hasField = true;
        } else if (StressAnalysisController* sa = StressAnalysisController::getInstance()) {
            if (sa->hasResult() || sa->showField()) hasField = true;
        } else if (Hdf5ProjectController* hpc = Hdf5ProjectController::getInstance()) {
            if (hpc->isOpen() && !hpc->loadSteps().isEmpty()) hasField = true;
        }
    }

    if (!hasField) {
        qDebug() << "[overlayColorBar] No active stress/strain field detected";
        return;
    }

    double vmin = 0.0;
    double vmax = 0.0;
    QString compName = QStringLiteral("von Mises (SEQV)");

    if (ogl && ogl->hasField()) {
        vmin = ogl->getFieldMin();
        vmax = ogl->getFieldMax();
        compName = ogl->getFieldComponentName();
    }

    if (vmin == 0.0 && vmax == 0.0) {
        LoadStepManager& lsm = LoadStepManager::getInstance();
        if (lsm.hasLoadStepData()) {
            StressAnalysisController* sa = StressAnalysisController::getInstance();
            int compEnum = (sa && sa->currentComponentEnum() >= 0) ? sa->currentComponentEnum() : SEQV;
            const auto& minVec = lsm.getLoadStepResultsMin();
            const auto& maxVec = lsm.getLoadStepResultsMax();
            if (compEnum >= 0 && compEnum < (int)minVec.size() && compEnum < (int)maxVec.size()) {
                vmin = minVec[compEnum];
                vmax = maxVec[compEnum];
            }
            if (sa) {
                const QStringList comps = sa->fieldComponents();
                if (sa->fieldComponentIndex() >= 0 && sa->fieldComponentIndex() < comps.size())
                    compName = comps[sa->fieldComponentIndex()];
            }
        } else if (StressAnalysisController* sa = StressAnalysisController::getInstance()) {
            vmin = sa->fieldMin();
            vmax = sa->fieldMax();
            const QStringList comps = sa->fieldComponents();
            if (sa->fieldComponentIndex() >= 0 && sa->fieldComponentIndex() < comps.size())
                compName = comps[sa->fieldComponentIndex()];
        }
    }

    QVector<QColor> colors;
    if (ogl) {
        colors = ogl->getColorMap(9);
    }
    if (colors.size() < 9) {
        auto palette = ogl ? ogl->getColorMapPalette() : OpenGLWidgetQML::ColorMapPalette::Rainbow;
        auto rawMap = matviz_cmap::createColorMap(9, palette);
        colors.resize(9);
        for (int i = 0; i < 9; ++i) colors[i] = QColor(rawMap[i][0], rawMap[i][1], rawMap[i][2]);
    }

    const qreal imgW = img.width();
    const qreal imgH = img.height();
    if (imgW < 120 || imgH < 120) return;

    const qreal scale = std::clamp(imgW / 1280.0, 0.75, 5.0);

    qreal cardW = 0, cardH = 0;
    if (!vertical) {
        cardW = std::min(400.0 * scale, imgW - 40.0 * scale);
        cardH = 68.0 * scale;
    } else {
        cardW = std::min(180.0 * scale, imgW - 40.0 * scale);
        cardH = 175.0 * scale;
    }

    const qreal cardX = 20.0 * scale;
    const qreal cardY = imgH - cardH - 20.0 * scale;

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    drawColorBarCard(p, QRectF(cardX, cardY, cardW, cardH), scale, vertical,
                     compName, vmin, vmax, colors, false);
}

// ═══════════════════════════════════════════════════════════════════
//  Process Screenshot: Background, Crop to Useful Content & Colorbar
// ═══════════════════════════════════════════════════════════════════
QImage ExportController::processScreenshot(const QImage& rawImg, bool replaceWhiteBg, bool autoCrop,
                                           bool verticalLegend, int targetDpi, QQuickItem* item,
                                           bool fieldActive)
{
    if (rawImg.isNull()) return rawImg;

    QImage img = replaceWhiteBg ? makeWhiteBackground(rawImg) : rawImg.convertToFormat(QImage::Format_RGB32);

    // 1. Determine active OpenGLWidgetQML
    OpenGLWidgetQML* ogl = qobject_cast<OpenGLWidgetQML*>(item);
    if (!ogl) ogl = OpenGLWidgetQML::getInstance();

    // 2. Check if stress/strain field is active across all controllers
    bool hasField = fieldActive;
    if (!hasField && ogl && ogl->hasField()) hasField = true;
    if (!hasField) {
        if (StressAnalysisController* sa = StressAnalysisController::getInstance()) {
            if (sa->hasResult() || sa->showField()) hasField = true;
        }
    }
    if (!hasField) {
        if (Hdf5ProjectController* hpc = Hdf5ProjectController::getInstance()) {
            if (hpc->isOpen() && !hpc->loadSteps().isEmpty()) hasField = true;
        }
    }
    if (!hasField) {
        LoadStepManager& lsm = LoadStepManager::getInstance();
        if (lsm.hasLoadStepData()) hasField = true;
    }

    double vmin = 0.0;
    double vmax = 0.0;
    QString compName = QStringLiteral("von Mises (SEQV)");

    if (StressAnalysisController* sa = StressAnalysisController::getInstance()) {
        vmin = sa->fieldMin();
        vmax = sa->fieldMax();
        const QStringList comps = sa->fieldComponents();
        if (sa->fieldComponentIndex() >= 0 && sa->fieldComponentIndex() < comps.size())
            compName = comps[sa->fieldComponentIndex()];
    }

    if (vmin == 0.0 && vmax == 0.0 && ogl && ogl->hasField()) {
        vmin = ogl->getFieldMin();
        vmax = ogl->getFieldMax();
        compName = ogl->getFieldComponentName();
    }

    if (vmin == 0.0 && vmax == 0.0) {
        LoadStepManager& lsm = LoadStepManager::getInstance();
        if (lsm.hasLoadStepData()) {
            StressAnalysisController* sa = StressAnalysisController::getInstance();
            int compEnum = (sa && sa->currentComponentEnum() >= 0) ? sa->currentComponentEnum() : SEQV;
            const auto& minVec = lsm.getLoadStepResultsMin();
            const auto& maxVec = lsm.getLoadStepResultsMax();
            if (compEnum >= 0 && compEnum < (int)minVec.size() && compEnum < (int)maxVec.size()) {
                vmin = minVec[compEnum];
                vmax = maxVec[compEnum];
            }
        }
    }

    QVector<QColor> colors;
    if (ogl) {
        colors = ogl->getColorMap(9);
    }
    if (colors.size() < 9) {
        auto palette = ogl ? ogl->getColorMapPalette() : OpenGLWidgetQML::ColorMapPalette::Rainbow;
        auto rawMap = matviz_cmap::createColorMap(9, palette);
        colors.resize(9);
        for (int i = 0; i < 9; ++i) colors[i] = QColor(rawMap[i][0], rawMap[i][1], rawMap[i][2]);
    }

    const int imgW = img.width();
    const int imgH = img.height();
    const qreal baseScale = imgW / 1280.0;
    const qreal scale = std::clamp(baseScale, 0.75, 5.0);

    const QColor bgColor = replaceWhiteBg ? QColor(255, 255, 255) : img.pixelColor(0, 0);
    const bool darkTheme = (bgColor.lightness() < 128);

    const qreal cardW = (!verticalLegend) ? 380.0 * scale : 180.0 * scale;
    const qreal cardH = (!verticalLegend) ? 68.0 * scale : 175.0 * scale;

    // If autoCrop is disabled, overlay gizmo at bottom-right and colorbar on bottom-left
    if (!autoCrop) {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        // Draw orientation gizmo at bottom-right
        const qreal gx = imgW - 55.0 * scale;
        const qreal gy = imgH - 50.0 * scale;
        drawOrientationGizmo(p, QPointF(gx, gy), scale, ogl, darkTheme);

        // Draw legend at bottom-left
        if (hasField) {
            const qreal cardX = 20.0 * scale;
            const qreal cardY = imgH - cardH - 20.0 * scale;
            drawColorBarCard(p, QRectF(cardX, cardY, cardW, cardH), scale, verticalLegend,
                             compName, vmin, vmax, colors, darkTheme);
        }
        p.end();
        setDpiMetadata(img, targetDpi);
        return img;
    }

    // ── STEP 1: CROP SCENE TO 3D MODEL ──────────────────────────────────────
    const int br = bgColor.red(), bg_g = bgColor.green(), bb = bgColor.blue();
    const int tol = 8;
    auto isBg = [&](QRgb p) {
        return std::abs(qRed(p)   - br)  <= tol &&
               std::abs(qGreen(p) - bg_g) <= tol &&
               std::abs(qBlue(p)  - bb)  <= tol;
    };

    // Exclude bottom-right corner region where raw OpenGL triad might have rendered
    const int cornerExclW = qRound(120.0 * scale);
    const int cornerExclH = qRound(120.0 * scale);
    const QRect cornerExclRect(imgW - cornerExclW, imgH - cornerExclH, cornerExclW, cornerExclH);

    int minX = imgW, maxX = -1, minY = imgH, maxY = -1;
    for (int y = 0; y < imgH; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < imgW; ++x) {
            if (cornerExclRect.contains(x, y)) continue;
            if (!isBg(line[x])) {
                minX = std::min(minX, x); maxX = std::max(maxX, x);
                minY = std::min(minY, y); maxY = std::max(maxY, y);
            }
        }
    }

    // Fallback if no non-background pixels found outside corner
    if (maxX < minX || maxY < minY) {
        minX = 0; maxX = imgW - 1;
        minY = 0; maxY = imgH - 1;
    }

    const int pad = qRound(25.0 * scale);
    const qreal gizmoSize = 85.0 * scale;
    const int extraBottom = qRound((hasField ? std::max(cardH, gizmoSize) : gizmoSize) + 20.0 * scale);

    int x1 = std::max(0, minX - pad);
    int y1 = std::max(0, minY - pad);
    int x2 = std::min(imgW - 1, maxX + pad);
    int y2 = std::min(imgH - 1, maxY + extraBottom);

    const int reqW = qRound((hasField ? cardW : 0) + gizmoSize + 60.0 * scale);
    if (x2 - x1 + 1 < reqW) {
        int diff = reqW - (x2 - x1 + 1);
        x1 = std::max(0, x1 - diff / 2);
        x2 = std::min(imgW - 1, x1 + reqW - 1);
        x1 = std::max(0, x2 - reqW + 1);
    }

    QImage resultImg = img.copy(x1, y1, x2 - x1 + 1, y2 - y1 + 1);

    const int neededH = (maxY - y1) + extraBottom;
    if (resultImg.height() < neededH) {
        QImage expanded(resultImg.width(), neededH, resultImg.format());
        expanded.fill(bgColor);
        QPainter expP(&expanded);
        expP.drawImage(0, 0, resultImg);
        expP.end();
        resultImg = expanded;
    }

    // ── STEP 2: DRAW GIZMO AXIS PLOT & LEGEND AFTER CROP ────────────────────
    QPainter p(&resultImg);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. Draw orientation gizmo at bottom-right
    const qreal gx = resultImg.width() - 55.0 * scale;
    const qreal gy = resultImg.height() - 50.0 * scale;
    drawOrientationGizmo(p, QPointF(gx, gy), scale, ogl, darkTheme);

    // 2. Draw legend at bottom-left
    if (hasField) {
        const qreal cardX = 15.0 * scale;
        const qreal cardY = resultImg.height() - cardH - 15.0 * scale;
        drawColorBarCard(p, QRectF(cardX, cardY, cardW, cardH), scale, verticalLegend,
                         compName, vmin, vmax, colors, darkTheme);
    }

    p.end();
    setDpiMetadata(resultImg, targetDpi);
    return resultImg;
}

// ═══════════════════════════════════════════════════════════════════
//  PNG — original scene colors
// ═══════════════════════════════════════════════════════════════════
void ExportController::saveAsImage(QQuickItem* item)
{
    saveAsImage(item, m_legendVertical, false);
}

void ExportController::saveAsImage(QQuickItem* item, bool verticalLegend, bool fieldActive)
{
    if (!item) {
        emit exportFailed(tr("Scene item is not available"));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        nullptr, tr("Save Image"), "",
        tr("PNG Images (*.png);;All Files (*.*)"));
    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".png", Qt::CaseInsensitive))
        fileName += ".png";

    // High DPI target size calculation
    QSize targetSize;
    if (m_highDpi && m_dpi > 96) {
        const qreal factor = m_dpi / 96.0;
        targetSize = (item->size() * factor).toSize();
        if (targetSize.width() > 8192 || targetSize.height() > 8192) {
            targetSize.scale(8192, 8192, Qt::KeepAspectRatio);
        }
    }

    auto grab = (!targetSize.isEmpty()) ? item->grabToImage(targetSize) : item->grabToImage();
    if (!grab) {
        emit exportFailed(tr("grabToImage() failed — item has no window"));
        return;
    }

    connect(grab.data(), &QQuickItemGrabResult::ready, this,
            [this, grab, fileName, verticalLegend, fieldActive, item]()
            {
                QImage rawImg = grab->image();
                if (rawImg.isNull()) {
                    emit exportFailed(tr("Captured image is empty"));
                    return;
                }
                QImage img = processScreenshot(rawImg, false, m_autoCrop, verticalLegend, m_dpi, item, fieldActive);
                if (!img.save(fileName)) {
                    emit exportFailed(tr("Failed to save image to ") + fileName);
                    return;
                }
                qDebug() << "Image saved:" << fileName << img.size() << "DPI:" << m_dpi;
                emit exportFinished(fileName);
            });
}

// ═══════════════════════════════════════════════════════════════════
//  Clipboard — background replaced with white, auto-cropped, 300 DPI
// ═══════════════════════════════════════════════════════════════════
void ExportController::copyToClipboard(QQuickItem* item)
{
    copyToClipboard(item, m_legendVertical, false);
}

void ExportController::copyToClipboard(QQuickItem* item, bool verticalLegend, bool fieldActive)
{
    if (!item) {
        emit exportFailed(tr("Scene item is not available"));
        return;
    }

    // High DPI target size calculation
    QSize targetSize;
    if (m_highDpi && m_dpi > 96) {
        const qreal factor = m_dpi / 96.0;
        targetSize = (item->size() * factor).toSize();
        if (targetSize.width() > 8192 || targetSize.height() > 8192) {
            targetSize.scale(8192, 8192, Qt::KeepAspectRatio);
        }
    }

    auto grab = (!targetSize.isEmpty()) ? item->grabToImage(targetSize) : item->grabToImage();
    if (!grab) {
        emit exportFailed(tr("grabToImage() failed — item has no window"));
        return;
    }

    connect(grab.data(), &QQuickItemGrabResult::ready, this,
            [this, grab, verticalLegend, fieldActive, item]()
            {
                QImage rawImg = grab->image();
                if (rawImg.isNull()) {
                    emit exportFailed(tr("Captured image is empty"));
                    return;
                }
                QImage img = processScreenshot(rawImg, true, m_autoCrop, verticalLegend, m_dpi, item, fieldActive);
                QGuiApplication::clipboard()->setImage(img);
                qDebug() << "Screenshot copied to clipboard" << img.size() << "DPI:" << m_dpi;
                emit exportFinished(QString());
            });
}

// ═══════════════════════════════════════════════════════════════════
//  SVG — Base64 raster inside an SVG container
// ═══════════════════════════════════════════════════════════════════
void ExportController::saveAsSVG(QQuickItem* item)
{
    if (!item) {
        emit exportFailed(tr("Scene item is not available"));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        nullptr, tr("Save as SVG"), "",
        tr("SVG Files (*.svg)"));
    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".svg", Qt::CaseInsensitive))
        fileName += ".svg";

    auto grab = item->grabToImage();
    if (!grab) {
        emit exportFailed(tr("grabToImage() failed — item has no window"));
        return;
    }

    connect(grab.data(), &QQuickItemGrabResult::ready, this,
            [this, grab, fileName]()
            {
                QImage img = makeWhiteBackground(grab->image());
                if (img.isNull()) {
                    emit exportFailed(tr("Captured image is empty"));
                    return;
                }
                overlayColorBar(img, m_legendVertical);

                // PNG -> bytes -> base64
                QByteArray png;
                QBuffer buffer(&png);
                buffer.open(QIODevice::WriteOnly);
                img.save(&buffer, "PNG");
                buffer.close();
                const QString b64 = QString::fromLatin1(png.toBase64());

                const int w = img.width();
                const int h = img.height();

                const QString svg =
                    QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                            "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                            "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
                            "width=\"%1\" height=\"%2\" viewBox=\"0 0 %1 %2\">\n"
                            "  <image width=\"%1\" height=\"%2\" "
                            "xlink:href=\"data:image/png;base64,%3\"/>\n"
                            "</svg>\n")
                        .arg(w).arg(h).arg(b64);

                QFile f(fileName);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    emit exportFailed(tr("Cannot open file for writing: ") + fileName);
                    return;
                }
                QTextStream(&f) << svg;
                f.close();

                qDebug() << "SVG saved:" << fileName << img.size();
                emit exportFinished(fileName);
            });
}

void ExportController::saveAsVectorSVG(QQuickItem* /*item*/)
{
    OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance();
    if (!ogl) { emit exportFailed(tr("Renderer is not available")); return; }

    QString fileName = QFileDialog::getSaveFileName(
        nullptr, tr("Save as vector SVG"), "", tr("SVG Files (*.svg)"));
    if (fileName.isEmpty()) return;
    if (!fileName.endsWith(".svg", Qt::CaseInsensitive)) fileName += ".svg";

    ogl->requestSvgExport(fileName);  // queued; written on the render thread next frame
    emit exportFinished(fileName);    // optimistic — see note below
}

// ═══════════════════════════════════════════════════════════════════
//  Shared access to voxels
// ═══════════════════════════════════════════════════════════════════
bool ExportController::fetchVoxels(int32_t***& voxelsOut, int& numCubesOut)
{
    OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance();
    if (!ogl) {
        emit exportFailed(tr("Renderer is not available"));
        return false;
    }

    voxelsOut   = ogl->getVoxels();
    numCubesOut = Parameters::instance()->getSize();

    if (!voxelsOut || numCubesOut <= 0) {
        emit exportFailed(tr("No structure generated — press START first"));
        return false;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  CSV — full voxel grid
// ═══════════════════════════════════════════════════════════════════
void ExportController::exportToCSV()
{
    int32_t*** voxels = nullptr;
    int numCubes = 0;
    if (!fetchVoxels(voxels, numCubes))
        return;

    QString fileName = QFileDialog::getSaveFileName(
        nullptr, tr("Save CSV File"), QDir::homePath(),
        tr("CSV Files (*.csv);;All Files (*)"));
    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".csv", Qt::CaseInsensitive))
        fileName += ".csv";

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit exportFailed(tr("Cannot open file for writing: ") + fileName);
        return;
    }

    QTextStream out(&file);
    out << "X;Y;Z;Color\n";

    for (int x = 0; x < numCubes; ++x)
        for (int y = 0; y < numCubes; ++y)
            for (int z = 0; z < numCubes; ++z)
                out << x << ";" << y << ";" << z << ";" << voxels[x][y][z] << "\n";

    file.close();

    qDebug() << "CSV saved:" << fileName
             << "| voxels:" << (qint64)numCubes * numCubes * numCubes;
    emit exportFinished(fileName);
}

// ═══════════════════════════════════════════════════════════════════
//  VRML — one cube per non-empty voxel
// ═══════════════════════════════════════════════════════════════════
void ExportController::exportToVRML()
{
    int32_t*** voxels = nullptr;
    int numCubes = 0;
    if (!fetchVoxels(voxels, numCubes))
        return;

    OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance();

    const std::vector<std::array<GLubyte, 4>> colors = ogl->generateDistinctColors();
    if (colors.empty()) {
        emit exportFailed(tr("Colour palette is empty"));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        nullptr, tr("Save VRML File"), QDir::homePath(),
        tr("VRML Files (*.wrl);;All Files (*)"));
    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".wrl", Qt::CaseInsensitive))
        fileName += ".wrl";

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit exportFailed(tr("Cannot open file for writing: ") + fileName);
        return;
    }

    QTextStream out(&file);
    out << "#VRML V2.0 utf8\n\n";

    qint64 written = 0;

    for (int x = 0; x < numCubes; ++x) {
        for (int y = 0; y < numCubes; ++y) {
            for (int z = 0; z < numCubes; ++z) {

                const int32_t id = voxels[x][y][z];

                if (id <= 0)
                    continue;

                const size_t idx = static_cast<size_t>(id - 1);
                if (idx >= colors.size())
                    continue;

                const double r = colors[idx][0] / 255.0;
                const double g = colors[idx][1] / 255.0;
                const double b = colors[idx][2] / 255.0;

                out << "Transform {\n"
                    << "  translation " << x << " " << y << " " << z << "\n"
                    << "  children Shape {\n"
                    << "    appearance Appearance {\n"
                    << "      material Material {\n"
                    << "        diffuseColor " << r << " " << g << " " << b << "\n"
                    << "      }\n"
                    << "    }\n"
                    << "    geometry Box { size 1.0 1.0 1.0 }\n"
                    << "  }\n"
                    << "}\n";

                ++written;
            }
        }
    }

    file.close();

    qDebug() << "VRML saved:" << fileName << "| boxes:" << written;
    emit exportFinished(fileName);
}

void ExportController::exportToHDF5()
{
    QString fileName = QFileDialog::getSaveFileName(
        nullptr, tr("Save HDF5 Project"), "",
        tr("HDF5 Files (*.h5 *.hdf5 *.hdf);;All Files (*.*)"));
    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".h5", Qt::CaseInsensitive) &&
        !fileName.endsWith(".hdf5", Qt::CaseInsensitive) &&
        !fileName.endsWith(".hdf", Qt::CaseInsensitive)) {
        fileName += ".hdf5";
    }

    HDF5Wrapper hdf5Wrapper(fileName.toStdString());

    int last_set = hdf5Wrapper.readInt("/", "last_set");
    if (last_set == -1)
    {
        last_set = 1;
        hdf5Wrapper.write("/", "last_set", last_set);
    }
    else
    {
        last_set += 1;
        hdf5Wrapper.update("/", "last_set", last_set);
    }

    std::string prefix = ("/" + QString::number(last_set)).toStdString();

    if (Parameters::voxels)
    {
        const int size = Parameters::instance()->getSize();
        const int points = Parameters::instance()->getPoints();
        hdf5Wrapper.write(prefix, "voxels", Parameters::voxels, size);
        hdf5Wrapper.write(prefix, "cubeSize", size);
        hdf5Wrapper.write(prefix, "numPoints", points);
        hdf5Wrapper.write(prefix, "seed", int(Parameters::seed));

        if (OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance()) {
            const auto& orientations = ogl->getGrainOrientations();
            if (!orientations.empty()) {
                std::vector<std::vector<float>> local_cs;
                local_cs.reserve(orientations.size());
                for (const auto& arr : orientations) {
                    local_cs.push_back({arr[0], arr[1], arr[2]});
                }
                hdf5Wrapper.write(prefix, "local_cs", local_cs);
            }
        }
    }

    if (StressAnalysisController* sa = StressAnalysisController::getInstance()) {
        if (sa->hasStiffness()) {
            saveStiffnessMatrixToHDF5(fileName, sa->lastStiffness(), sa->stiffnessIsFFT() ? "fft" : "ansys", Parameters::seed);
        }
    }

    Parameters::filename = fileName;
    qDebug() << "HDF5 project saved:" << fileName;
    emit exportFinished(fileName);
}

void ExportController::openHDF5()
{
    QString fileName = QFileDialog::getOpenFileName(
        nullptr, tr("Open MatViz3D HDF5 Project"), "",
        tr("HDF5 Files (*.h5 *.hdf5 *.hdf);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        qDebug() << "OpenHDF: no file selected";
        return;
    }

    LoadStepManager& lsm = LoadStepManager::getInstance();
    if (!lsm.LoadFromHDF5(fileName)) {
        emit exportFailed(tr("Failed to load HDF5 file: ") + fileName);
        return;
    }

    int cubeSize = lsm.getCubeSize();
    int numPoints = lsm.getNumPoints();
    int32_t*** vox = lsm.getVoxelPtr();

    if (cubeSize > 0 && vox) {
        Parameters* p = Parameters::instance();
        p->setSize(cubeSize);
        p->setPoints(numPoints);
        Parameters::filename = fileName;

        if (Parameters::voxels) {
            Parent_Algorithm::Delete3D<int32_t>(Parameters::voxels);
        }
        Parameters::voxels = Parent_Algorithm::Create3D<int32_t>(cubeSize, cubeSize, cubeSize);
        for (int i = 0; i < cubeSize; i++)
            for (int j = 0; j < cubeSize; j++)
                for (int k = 0; k < cubeSize; k++)
                    Parameters::voxels[i][j][k] = vox[i][j][k];

        if (OpenGLWidgetQML* ogl = OpenGLWidgetQML::getInstance()) {
            ogl->setNumColors(numPoints);
            ogl->setVoxels(Parameters::voxels, cubeSize);

            const auto& local_cs = lsm.getLocalCS();
            if (!local_cs.empty()) {
                std::vector<std::array<float, 3>> orientations;
                orientations.reserve(local_cs.size());
                for (const auto& row : local_cs) {
                    if (row.size() >= 3) {
                        orientations.push_back({row[0], row[1], row[2]});
                    }
                }
                ogl->setGrainOrientations(orientations);
            }
        }
    }

    if (StressAnalysisController* sa = StressAnalysisController::getInstance()) {
        sa->loadFromHDF5(fileName);
    }

    qDebug() << "HDF5 project opened successfully:" << fileName;
    emit exportFinished(fileName);
}

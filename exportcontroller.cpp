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

// ── Step 1 Helper: Obtain and prepare scene image from OGL ─────────
QImage obtainSceneImage(const QImage& rawImg, bool replaceWhiteBg, QQuickItem* item, qreal scale, QColor& outBgColor)
{
    if (rawImg.isNull()) return rawImg;

    QImage img = replaceWhiteBg ? makeWhiteBackground(rawImg) : rawImg.convertToFormat(QImage::Format_RGB32);
    outBgColor = replaceWhiteBg ? QColor(255, 255, 255) : img.pixelColor(0, 0);

    OpenGLWidgetQML* ogl = qobject_cast<OpenGLWidgetQML*>(item);
    if (!ogl) ogl = OpenGLWidgetQML::getInstance();

    // Erase bottom-right corner if corner axes were enabled, otherwise keep pristine
    if (!ogl || ogl->showCornerAxes()) {
        const int imgW = img.width();
        const int imgH = img.height();
        const qreal sx = (item && item->width() > 0) ? (qreal)imgW / item->width() : scale;
        const qreal sy = (item && item->height() > 0) ? (qreal)imgH / item->height() : scale;
        const int exclW = qRound(160.0 * sx);
        const int exclH = qRound(160.0 * sy);
        const QRect cornerExclRect(std::max(0, imgW - exclW), std::max(0, imgH - exclH), exclW, exclH);

        {
            QPainter pClear(&img);
            pClear.fillRect(cornerExclRect, outBgColor);
        }
    }

    qDebug() << "[obtainSceneImage] Raw size:" << rawImg.size()
             << "-> Processed size:" << img.size()
             << "bgColor:" << outBgColor.name();

    return img;
}

// ── Step 2 Helper: Crop image to useful 3D content ──────────────────
QImage cropToUsefulSize(const QImage& src, const QColor& bgColor, qreal scale, QRect* outCropRect = nullptr)
{
    if (src.isNull()) return src;

    const int imgW = src.width();
    const int imgH = src.height();
    const int br = bgColor.red(), bg_g = bgColor.green(), bb = bgColor.blue();
    const int tol = 8;
    auto isBg = [&](QRgb p) {
        return std::abs(qRed(p)   - br)   <= tol &&
               std::abs(qGreen(p) - bg_g) <= tol &&
               std::abs(qBlue(p)  - bb)   <= tol;
    };

    int minX = imgW, maxX = -1, minY = imgH, maxY = -1;
    for (int y = 0; y < imgH; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        for (int x = 0; x < imgW; ++x) {
            if (!isBg(line[x])) {
                minX = std::min(minX, x); maxX = std::max(maxX, x);
                minY = std::min(minY, y); maxY = std::max(maxY, y);
            }
        }
    }

    // Fallback if no non-background pixels found
    if (maxX < minX || maxY < minY) {
        minX = 0; maxX = imgW - 1;
        minY = 0; maxY = imgH - 1;
    }

    const int pad = qRound(16.0 * scale);
    const int mx1 = std::max(0, minX - pad);
    const int my1 = std::max(0, minY - pad);
    const int mx2 = std::min(imgW - 1, maxX + pad);
    const int my2 = std::min(imgH - 1, maxY + pad);
    const int cropW = mx2 - mx1 + 1;
    const int cropH = my2 - my1 + 1;

    const QRect cropRect(mx1, my1, cropW, cropH);
    if (outCropRect) {
        *outCropRect = cropRect;
    }

    qDebug() << "[cropToUsefulSize] Non-bg bounds:" << QRect(minX, minY, maxX - minX + 1, maxY - minY + 1)
             << "cropRect (with padding):" << cropRect << "crop size:" << QSize(cropW, cropH);

    return src.copy(cropRect);
}

// ── Step 3 Helper: Generate standalone image of colorbar (legend) ───
QImage generateColorBarImage(OpenGLWidgetQML* ogl, bool verticalLegend, qreal scale,
                             bool darkTheme, bool fieldActive, qreal maxAllowedW = -1.0)
{
    // Check if stress/strain field is active across all controllers
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

    if (!hasField) {
        qDebug() << "[generateColorBarImage] No active field detected, returning empty image";
        return QImage();
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

    qreal cardW = (!verticalLegend) ? 380.0 * scale : 180.0 * scale;
    const qreal cardH = (!verticalLegend) ? 68.0 * scale : 175.0 * scale;
    if (maxAllowedW > 0 && cardW > maxAllowedW) {
        cardW = maxAllowedW;
    }

    const int imgW = qMax(1, qCeil(cardW));
    const int imgH = qMax(1, qCeil(cardH));

    QImage cardImg(imgW, imgH, QImage::Format_ARGB32_Premultiplied);
    cardImg.fill(Qt::transparent);

    {
        QPainter p(&cardImg);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        drawColorBarCard(p, QRectF(0, 0, cardW, cardH), scale, verticalLegend,
                         compName, vmin, vmax, colors, darkTheme);
    }

    qDebug() << "[generateColorBarImage] Rendered colorbar image:" << cardImg.size()
             << "vertical:" << verticalLegend << "component:" << compName
             << "range: [" << vmin << "," << vmax << "]";

    return cardImg;
}

// ── Step 4 Helper: Generate standalone image of orientation gizmo ───
QImage generateGizmoImage(OpenGLWidgetQML* ogl, qreal scale, bool darkTheme)
{
    const qreal gizmoSize = 110.0 * scale;
    const int imgW = qMax(1, qCeil(gizmoSize));
    const int imgH = qMax(1, qCeil(gizmoSize));

    QImage gizmoImg(imgW, imgH, QImage::Format_ARGB32_Premultiplied);
    gizmoImg.fill(Qt::transparent);

    {
        QPainter p(&gizmoImg);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const QPointF origin(imgW / 2.0, imgH / 2.0);
        drawOrientationGizmo(p, origin, scale, ogl, darkTheme);
    }

    qDebug() << "[generateGizmoImage] Rendered gizmo image:" << gizmoImg.size() << "scale:" << scale;

    return gizmoImg;
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

    const qreal imgW = img.width();
    const qreal imgH = img.height();
    if (imgW < 120 || imgH < 120) return;

    const qreal imgDpi = (img.dotsPerMeterX() > 0) ? (img.dotsPerMeterX() * 0.0254) : 96.0;
    const qreal scale = std::clamp(imgDpi / 96.0, 0.75, 8.0);
    const bool darkTheme = (img.pixelColor(0, 0).lightness() < 128);

    const qreal maxW = imgW - 40.0 * scale;
    QImage colorbar = generateColorBarImage(ogl, vertical, scale, darkTheme, false, maxW);
    if (colorbar.isNull()) {
        qDebug() << "[overlayColorBar] No active stress/strain field detected";
        return;
    }

    const qreal cardX = 20.0 * scale;
    const qreal cardY = imgH - colorbar.height() - 20.0 * scale;

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.drawImage(QPointF(cardX, cardY), colorbar);
}

// ═══════════════════════════════════════════════════════════════════
//  Process Screenshot: Background, Crop to Useful Content & Colorbar
// ═══════════════════════════════════════════════════════════════════
QImage ExportController::processScreenshot(const QImage& rawImg, bool replaceWhiteBg, bool autoCrop,
                                           bool verticalLegend, int targetDpi, QQuickItem* item,
                                           bool fieldActive)
{
    qDebug() << "==================================================";
    qDebug() << "[processScreenshot] START: rawImg size =" << rawImg.size()
             << "replaceWhiteBg =" << replaceWhiteBg
             << "autoCrop =" << autoCrop
             << "verticalLegend =" << verticalLegend
             << "targetDpi =" << targetDpi
             << "fieldActive =" << fieldActive;

    if (rawImg.isNull()) {
        qWarning() << "[processScreenshot] rawImg is null, aborting";
        return rawImg;
    }

    // Scale is strictly DPI-proportional: 96 DPI = 1.0, 300 DPI = 3.125
    const qreal scale = (targetDpi > 0) ? std::clamp((qreal)targetDpi / 96.0, 0.75, 8.0)
                                        : std::clamp(rawImg.width() / 1280.0, 0.75, 5.0);
    qDebug() << "[processScreenshot] Calculated scale factor:" << scale;

    OpenGLWidgetQML* ogl = qobject_cast<OpenGLWidgetQML*>(item);
    if (!ogl) ogl = OpenGLWidgetQML::getInstance();

    // ── STEP 1: OBTAIN IMAGE FROM OGL ───────────────────────────────
    QColor bgColor;
    QImage sceneImg = obtainSceneImage(rawImg, replaceWhiteBg, item, scale, bgColor);
    const bool darkTheme = (bgColor.lightness() < 128);
    qDebug() << "[processScreenshot] Step 1 (Obtain Image from OGL):"
             << "sceneImg size =" << sceneImg.size()
             << "bgColor =" << bgColor.name()
             << "darkTheme =" << darkTheme;

    // ── STEP 2: CROP IT TO USEFUL SIZE ──────────────────────────────
    QRect cropRect(0, 0, sceneImg.width(), sceneImg.height());
    QImage croppedModel;
    if (autoCrop) {
        croppedModel = cropToUsefulSize(sceneImg, bgColor, scale, &cropRect);
        qDebug() << "[processScreenshot] Step 2 (Crop to Useful Size):"
                 << "autoCrop = true, cropRect =" << cropRect
                 << "croppedModel size =" << croppedModel.size();
    } else {
        croppedModel = sceneImg;
        qDebug() << "[processScreenshot] Step 2 (Crop to Useful Size):"
                 << "autoCrop = false, retaining full size =" << croppedModel.size();
    }

    // ── STEP 3: GENERATE IMAGE OF COLORBAR (LEGEND) ─────────────────
    QImage colorbarImg = generateColorBarImage(ogl, verticalLegend, scale, darkTheme, fieldActive);
    if (!colorbarImg.isNull()) {
        qDebug() << "[processScreenshot] Step 3 (Generate ColorBar):"
                 << "colorbar image generated, size =" << colorbarImg.size()
                 << "vertical =" << verticalLegend;
    } else {
        qDebug() << "[processScreenshot] Step 3 (Generate ColorBar):"
                 << "no active stress/strain field, colorbar omitted";
    }

    // ── STEP 4: GENERATE IMAGE OF GIZMO ─────────────────────────────
    QImage gizmoImg = generateGizmoImage(ogl, scale, darkTheme);
    qDebug() << "[processScreenshot] Step 4 (Generate Gizmo):"
             << "gizmo image generated, size =" << gizmoImg.size();

    // ── STEP 5: ADD EMPTY SPACE FOR COLORBAR & GIZMO ────────────────
    int finalW = croppedModel.width();
    int finalH = croppedModel.height();
    QPoint modelPos(0, 0);
    QPoint colorbarPos(0, 0);
    QPoint gizmoPos(0, 0);

    const bool hasColorBar = !colorbarImg.isNull();
    const qreal marginL = 16.0 * scale;
    const qreal marginR = 16.0 * scale;
    const qreal bottomPad = 14.0 * scale;
    const qreal gapAbove = 14.0 * scale;

    if (autoCrop) {
        // Extra bottom space is ONLY needed when a colorbar card is present.
        // When there is no colorbar, the gizmo overlays in the bottom-right corner of the
        // cropped model canvas without allocating an empty bottom strip.
        int extraBottom = 0;
        if (hasColorBar) {
            const qreal bottomItemH = std::max((qreal)colorbarImg.height(), 80.0 * scale);
            extraBottom = qRound(gapAbove + bottomItemH + bottomPad);
        }

        const qreal gizmoBoxW = 85.0 * scale;
        const qreal minReqW = (hasColorBar ? (marginL + colorbarImg.width() + 20.0 * scale) : marginL)
                            + gizmoBoxW + marginR;

        finalW = std::max(croppedModel.width(), qRound(minReqW));
        finalH = croppedModel.height() + extraBottom;

        // Center model horizontally if canvas was widened to accommodate bottom controls
        modelPos = QPoint((finalW - croppedModel.width()) / 2, 0);

        if (hasColorBar) {
            colorbarPos = QPoint(qRound(marginL), qRound(finalH - bottomPad - colorbarImg.height()));
        }

        const qreal gx = finalW - marginR - 45.0 * scale;
        const qreal gy = finalH - bottomPad - 42.0 * scale;
        gizmoPos = QPoint(qRound(gx - gizmoImg.width() / 2.0),
                          qRound(gy - gizmoImg.height() / 2.0));

        qDebug() << "[processScreenshot] Step 5 (Add Empty Space & Layout): autoCrop = true,"
                 << "hasColorBar =" << hasColorBar
                 << "extraBottom =" << extraBottom
                 << "final canvas size =" << QSize(finalW, finalH)
                 << "modelPos =" << modelPos
                 << "colorbarPos =" << colorbarPos
                 << "gizmoPos =" << gizmoPos;
    } else {
        finalW = sceneImg.width();
        finalH = sceneImg.height();
        modelPos = QPoint(0, 0);

        if (!colorbarImg.isNull()) {
            colorbarPos = QPoint(qRound(20.0 * scale),
                                 qRound(finalH - colorbarImg.height() - 20.0 * scale));
        }

        const qreal gx = finalW - 60.0 * scale;
        const qreal gy = finalH - 55.0 * scale;
        gizmoPos = QPoint(qRound(gx - gizmoImg.width() / 2.0),
                          qRound(gy - gizmoImg.height() / 2.0));

        qDebug() << "[processScreenshot] Step 5 (Add Empty Space & Layout): autoCrop = false (overlay mode),"
                 << "canvas size =" << QSize(finalW, finalH)
                 << "colorbarPos =" << colorbarPos
                 << "gizmoPos =" << gizmoPos;
    }

    // ── STEP 6: JOIN ALL IMAGES TOGETHER ────────────────────────────
    QImage resultImg(finalW, finalH, sceneImg.format());
    resultImg.fill(bgColor);

    {
        QPainter p(&resultImg);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        // 1. Draw cropped model image
        p.drawImage(modelPos, croppedModel);

        // 2. Draw colorbar image (if present)
        if (!colorbarImg.isNull()) {
            p.drawImage(colorbarPos, colorbarImg);
        }

        // 3. Draw gizmo image (if present)
        if (!gizmoImg.isNull()) {
            p.drawImage(gizmoPos, gizmoImg);
        }
    }

    setDpiMetadata(resultImg, targetDpi);
    qDebug() << "[processScreenshot] Step 6 (Join Images): Composite completed successfully, final size ="
             << resultImg.size() << "DPI =" << targetDpi;
    qDebug() << "==================================================";

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

    QQuickItem* axisOverlay = item ? item->findChild<QQuickItem*>("axisLabelOverlay") : nullptr;
    if (axisOverlay) axisOverlay->setVisible(false);

    OpenGLWidgetQML* oglWidget = qobject_cast<OpenGLWidgetQML*>(item);
    if (!oglWidget) oglWidget = OpenGLWidgetQML::getInstance();
    const bool prevAxes = oglWidget ? oglWidget->showCornerAxes() : true;
    if (oglWidget) oglWidget->setShowCornerAxes(false);

    auto grab = (!targetSize.isEmpty()) ? item->grabToImage(targetSize) : item->grabToImage();
    if (!grab) {
        if (axisOverlay) axisOverlay->setVisible(true);
        if (oglWidget) oglWidget->setShowCornerAxes(prevAxes);
        emit exportFailed(tr("grabToImage() failed — item has no window"));
        return;
    }

    connect(grab.data(), &QQuickItemGrabResult::ready, this,
            [this, grab, fileName, verticalLegend, fieldActive, item, axisOverlay, oglWidget, prevAxes]()
            {
                if (axisOverlay) axisOverlay->setVisible(true);
                if (oglWidget) oglWidget->setShowCornerAxes(prevAxes);
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

    QQuickItem* axisOverlay = item ? item->findChild<QQuickItem*>("axisLabelOverlay") : nullptr;
    if (axisOverlay) axisOverlay->setVisible(false);

    OpenGLWidgetQML* oglWidget = qobject_cast<OpenGLWidgetQML*>(item);
    if (!oglWidget) oglWidget = OpenGLWidgetQML::getInstance();
    const bool prevAxes = oglWidget ? oglWidget->showCornerAxes() : true;
    if (oglWidget) oglWidget->setShowCornerAxes(false);

    auto grab = (!targetSize.isEmpty()) ? item->grabToImage(targetSize) : item->grabToImage();
    if (!grab) {
        if (axisOverlay) axisOverlay->setVisible(true);
        if (oglWidget) oglWidget->setShowCornerAxes(prevAxes);
        emit exportFailed(tr("grabToImage() failed — item has no window"));
        return;
    }

    connect(grab.data(), &QQuickItemGrabResult::ready, this,
            [this, grab, verticalLegend, fieldActive, item, axisOverlay, oglWidget, prevAxes]()
            {
                if (axisOverlay) axisOverlay->setVisible(true);
                if (oglWidget) oglWidget->setShowCornerAxes(prevAxes);
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

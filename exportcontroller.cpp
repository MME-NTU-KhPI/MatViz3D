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
#include <QQuickWindow>
#include <QPointer>
#include <QTimer>
#include <memory>
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
#include <QPainterPath>
#include <QFontMetricsF>
#include <QMimeData>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <cmath>
#include <algorithm>
#include <QVector3D>

namespace {

/**
 * Replace the backdrop of a grabbed on-screen frame with `target` (white, or
 * fully transparent). Only the grabToImage() fallback needs this: the
 * off-screen render is cleared to the requested background directly. The
 * backdrop colour is read from the top-left pixel and matched with a
 * tolerance, so anti-aliased edge pixels, which are a mix of model and
 * backdrop, keep their mix -- the reason the off-screen path is preferred.
 */
QImage replaceBackground(const QImage& src, const QColor& target)
{
    if (src.isNull())
        return src;

    QImage img = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QRgb bg = img.pixel(0, 0);              // backdrop = top-left corner
    const QRgb repl = qPremultiply(target.rgba());

    const int tol = 6;                            // tolerance in units 0..255
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb p = line[x];
            if (std::abs(qRed(p)   - qRed(bg))   <= tol &&
                std::abs(qGreen(p) - qGreen(bg)) <= tol &&
                std::abs(qBlue(p)  - qBlue(bg))  <= tol &&
                std::abs(qAlpha(p) - qAlpha(bg)) <= tol)
                line[x] = repl;
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
                      const QVector<QColor>& colors, bool darkTheme, bool translucent = false)
{
    // On a transparent export the card keeps a backing, so its text reads on
    // whatever the figure is placed on, but lets the page show through a bit.
    const QColor cardFill = darkTheme ? QColor(35, 35, 35, translucent ? 205 : 240)
                                      : QColor(255, 255, 255, translucent ? 215 : 245);
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
        p.setBrush(cardFill);
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
        p.setBrush(cardFill);
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

void drawOrientationGizmo(QPainter& p, const QPointF& origin, qreal scale, OpenGLWidgetQML* ogl, bool darkTheme)
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

    const QPointF dirs[3] = { norm(ptX - o), norm(ptY - o), norm(ptZ - o) };
    const QColor  cols[3] = { QColor(235, 45, 45), QColor(40, 185, 40), QColor(40, 120, 245) };
    const QString names[3] = { QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z") };

    // A thin halo in the overlay's background colour under every stroke and
    // letter: on a transparent export the triad may land on any colour, and
    // the halo keeps it legible there without a card behind it.
    const QColor halo = darkTheme ? QColor(28, 28, 28, 230) : QColor(255, 255, 255, 235);
    const qreal  haloW = 2.4 * scale;

    const qreal axisLen = 30.0 * scale;
    const qreal penW    = qMax(1.5, 2.2 * scale);
    const qreal tickW   = qMax(1.2, 1.8 * scale);
    const qreal aLen    = 7.0 * scale;
    const qreal aWidth  = 4.0 * scale;

    QFont labelFont("Segoe UI", qMax(8, qRound(10.5 * scale)), QFont::Bold);
    const QFontMetricsF fm(labelFont);
    const qreal textOffset = 13.0 * scale;

    for (int pass = 0; pass < 2; ++pass) {
        const bool haloPass = (pass == 0);
        for (int i = 0; i < 3; ++i) {
            const QPointF end = origin + dirs[i] * axisLen;
            const QColor  c   = haloPass ? halo : cols[i];
            const qreal   extra = haloPass ? haloW : 0.0;

            p.setPen(QPen(c, penW + extra, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(origin, end);

            const QPointF perp(-dirs[i].y(), dirs[i].x());
            const QPointF back = end - dirs[i] * aLen;
            p.setPen(QPen(c, tickW + extra, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawLine(end, back + perp * aWidth);
            p.drawLine(end, back - perp * aWidth);

            // Label centred on a point beyond the tip, drawn as a path so it
            // can carry the halo as a stroke.
            const QPointF centre = end + dirs[i] * textOffset;
            const QRectF  tb = fm.tightBoundingRect(names[i]);
            QPainterPath path;
            path.addText(centre - tb.center(), labelFont, names[i]);
            if (haloPass) {
                p.strokePath(path, QPen(halo, haloW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            } else {
                p.fillPath(path, cols[i]);
            }
        }
    }
}

// ── Step 1 Helper: Obtain and prepare scene image from OGL ─────────
// Works in ARGB32_Premultiplied from here on, so a transparent background
// keeps its alpha through crop and composition.
QImage obtainSceneImage(const QImage& rawImg, const ExportController::ScreenshotOptions& o,
                        QQuickItem* item, qreal scale, QColor& outBgColor)
{
    using B = ExportController::Background;
    if (rawImg.isNull()) return rawImg;

    QImage img = rawImg.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    if (!o.fromGrab) {
        // Off-screen render: already on the requested background, no triad.
        outBgColor = o.background == B::White       ? QColor(255, 255, 255)
                   : o.background == B::Transparent ? QColor(0, 0, 0, 0)
                                                    : img.pixelColor(0, 0);
        return img;
    }

    // grabToImage() fallback: the on-screen frame with its backdrop and the
    // corner triad, both replaced by colour matching.
    if (o.background == B::White) {
        img = replaceBackground(img, QColor(255, 255, 255));
        outBgColor = QColor(255, 255, 255);
    } else if (o.background == B::Transparent) {
        img = replaceBackground(img, QColor(0, 0, 0, 0));
        outBgColor = QColor(0, 0, 0, 0);
    } else {
        outBgColor = img.pixelColor(0, 0);
    }

    OpenGLWidgetQML* ogl = qobject_cast<OpenGLWidgetQML*>(item);
    if (!ogl) ogl = OpenGLWidgetQML::getInstance();
    if (!ogl || ogl->showCornerAxes()) {
        const int imgW = img.width();
        const int imgH = img.height();
        const qreal sx = (item && item->width() > 0) ? (qreal)imgW / item->width() : scale;
        const qreal sy = (item && item->height() > 0) ? (qreal)imgH / item->height() : scale;
        const int exclW = qRound(160.0 * sx);
        const int exclH = qRound(160.0 * sy);
        QPainter pClear(&img);
        pClear.setCompositionMode(QPainter::CompositionMode_Source);
        pClear.fillRect(QRect(std::max(0, imgW - exclW), std::max(0, imgH - exclH), exclW, exclH),
                        outBgColor);
    }
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
    // A transparent background is found by coverage: any pixel the model
    // touches at all has alpha > 0. An opaque one by colour, with a tolerance.
    const bool byAlpha = (bgColor.alpha() == 0);
    auto isBg = [&](QRgb p) {
        if (byAlpha) return qAlpha(p) == 0;
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

    return src.copy(cropRect);
}

// ── Step 3 Helper: Generate standalone image of colorbar (legend) ───
QImage generateColorBarImage(OpenGLWidgetQML* ogl, bool verticalLegend, qreal scale,
                             bool darkTheme, bool fieldActive, qreal maxAllowedW = -1.0,
                             bool translucent = false)
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

    if (!hasField) return QImage();

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
                         compName, vmin, vmax, colors, darkTheme, translucent);
    }

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
QImage ExportController::processScreenshot(const QImage& rawImg, const ScreenshotOptions& o,
                                           QQuickItem* item)
{
    if (rawImg.isNull()) {
        qWarning() << "[processScreenshot] rawImg is null, aborting";
        return rawImg;
    }

    // Scale is strictly DPI-proportional: 96 DPI = 1.0, 300 DPI = 3.125
    const qreal scale = (o.dpi > 0) ? std::clamp((qreal)o.dpi / 96.0, 0.75, 8.0)
                                    : std::clamp(rawImg.width() / 1280.0, 0.75, 5.0);
    const bool transparent = (o.background == Background::Transparent);

    OpenGLWidgetQML* ogl = qobject_cast<OpenGLWidgetQML*>(item);
    if (!ogl) ogl = OpenGLWidgetQML::getInstance();

    // ── 1. Scene image on the requested background ───────────────────
    QColor bgColor;
    QImage sceneImg = obtainSceneImage(rawImg, o, item, scale, bgColor);

    // ── 2. Crop to the model ─────────────────────────────────────────
    QImage croppedModel = o.autoCrop ? cropToUsefulSize(sceneImg, bgColor, scale) : sceneImg;

    // ── 3, 4. Legend and gizmo, in the overlay style asked for ───────
    QImage colorbarImg = generateColorBarImage(ogl, o.verticalLegend, scale, o.darkOverlay,
                                               o.fieldActive, -1.0, transparent);
    QImage gizmoImg = generateGizmoImage(ogl, scale, o.darkOverlay);

    // ── 5. Layout ────────────────────────────────────────────────────
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

    if (o.autoCrop) {
        // Extra bottom space only for a colour bar; without one the gizmo
        // sits in the bottom-right corner of the cropped model.
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
        modelPos = QPoint((finalW - croppedModel.width()) / 2, 0);

        if (hasColorBar)
            colorbarPos = QPoint(qRound(marginL), qRound(finalH - bottomPad - colorbarImg.height()));

        const qreal gx = finalW - marginR - 45.0 * scale;
        const qreal gy = finalH - bottomPad - 42.0 * scale;
        gizmoPos = QPoint(qRound(gx - gizmoImg.width() / 2.0),
                          qRound(gy - gizmoImg.height() / 2.0));
    } else {
        if (hasColorBar)
            colorbarPos = QPoint(qRound(20.0 * scale),
                                 qRound(finalH - colorbarImg.height() - 20.0 * scale));
        const qreal gx = finalW - 60.0 * scale;
        const qreal gy = finalH - 55.0 * scale;
        gizmoPos = QPoint(qRound(gx - gizmoImg.width() / 2.0),
                          qRound(gy - gizmoImg.height() / 2.0));
    }

    // ── 6. Compose (premultiplied ARGB, source-over) ─────────────────
    QImage resultImg(finalW, finalH, QImage::Format_ARGB32_Premultiplied);
    resultImg.fill(bgColor);   // QColor(0,0,0,0) -> fully transparent
    {
        QPainter p(&resultImg);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(modelPos, croppedModel);
        if (hasColorBar)        p.drawImage(colorbarPos, colorbarImg);
        if (!gizmoImg.isNull()) p.drawImage(gizmoPos, gizmoImg);
    }

    // An opaque export is written without an alpha channel: smaller files,
    // and no viewer can show a stray semi-transparent pixel.
    if (!transparent)
        resultImg = resultImg.convertToFormat(QImage::Format_RGB32);

    setDpiMetadata(resultImg, o.dpi);
    qDebug().noquote() << QStringLiteral("[processScreenshot] %1 x %2 at %3 DPI, background %4, %5 overlay, %6")
                              .arg(resultImg.width()).arg(resultImg.height()).arg(o.dpi)
                              .arg(transparent ? "transparent" : o.background == Background::White ? "white" : "theme")
                              .arg(o.darkOverlay ? "dark" : "light")
                              .arg(o.fromGrab ? "grabbed frame" : "off-screen render");
    return resultImg;
}

// ═══════════════════════════════════════════════════════════════════
//  Scene capture: off-screen render, grabToImage() as the fallback
// ═══════════════════════════════════════════════════════════════════
QSize ExportController::exportPixelSize(QQuickItem* item) const
{
    if (!item) return QSize();
    qreal factor = item->window() ? item->window()->effectiveDevicePixelRatio() : 1.0;
    if (m_highDpi && m_dpi > 96) factor = m_dpi / 96.0;
    QSize size = (item->size() * factor).toSize();
    const int limit = std::max(1024, RenderOpenGL::maxCaptureDimension() > 0
                                         ? RenderOpenGL::maxCaptureDimension() : 8192);
    if (size.width() > limit || size.height() > limit)
        size.scale(limit, limit, Qt::KeepAspectRatio);
    return size;
}

void ExportController::captureScene(QQuickItem* item, Background background, SceneCallback done)
{
    OpenGLWidgetQML* ogl = qobject_cast<OpenGLWidgetQML*>(item);
    // MATVIZ_EXPORT_GRAB=1 forces the old path, for before / after comparisons.
    static const bool forceGrab = qEnvironmentVariableIntValue("MATVIZ_EXPORT_GRAB") != 0;
    if (ogl && !forceGrab) {
        const RenderOpenGL::CaptureBackground bg =
            background == Background::White       ? RenderOpenGL::CaptureBackground::White
          : background == Background::Transparent ? RenderOpenGL::CaptureBackground::Transparent
                                                       : RenderOpenGL::CaptureBackground::Theme;
        QPointer<QQuickItem> guard(item);
        // The request is served by the next frame, and a window that is not
        // exposed (minimised, covered on some platforms) renders none. After
        // a while give up on it and grab instead; whichever answers first wins.
        auto answered = std::make_shared<bool>(false);
        const bool queued = ogl->requestCapture(exportPixelSize(item), bg, this,
            [this, guard, done, answered](QImage img) {
                if (*answered) return;
                *answered = true;
                if (!img.isNull()) { done(img, true); return; }
                qWarning() << "[export] off-screen capture failed, falling back to grabToImage()";
                if (guard) grabScene(guard, done);
                else       done(QImage(), false);
            });
        if (queued) {
            QTimer::singleShot(10000, this, [this, guard, done, answered]() {
                if (*answered) return;
                *answered = true;
                qWarning() << "[export] no frame rendered the capture in 10 s, falling back to grabToImage()";
                if (guard) grabScene(guard, done);
                else       done(QImage(), false);
            });
            return;
        }
    }
    grabScene(item, done);
}

void ExportController::grabScene(QQuickItem* item, SceneCallback done)
{
    const QSize targetSize = exportPixelSize(item);

    QQuickItem* axisOverlay = item->findChild<QQuickItem*>("axisLabelOverlay");
    if (axisOverlay) axisOverlay->setVisible(false);

    OpenGLWidgetQML* oglWidget = qobject_cast<OpenGLWidgetQML*>(item);
    const bool prevAxes = oglWidget ? oglWidget->showCornerAxes() : true;
    if (oglWidget) oglWidget->setShowCornerAxes(false);

    auto restore = [axisOverlay, oglWidget, prevAxes]() {
        if (axisOverlay) axisOverlay->setVisible(true);
        if (oglWidget) oglWidget->setShowCornerAxes(prevAxes);
    };

    auto grab = (!targetSize.isEmpty()) ? item->grabToImage(targetSize) : item->grabToImage();
    if (!grab) {
        restore();
        emit exportFailed(tr("grabToImage() failed — item has no window"));
        return;
    }
    connect(grab.data(), &QQuickItemGrabResult::ready, this, [grab, restore, done]() {
        restore();
        done(grab->image(), false);
    });
}

// ═══════════════════════════════════════════════════════════════════
//  Options from the current settings
// ═══════════════════════════════════════════════════════════════════
ExportController::ScreenshotOptions ExportController::currentOptions(bool forClipboard, bool verticalLegend,
                                                                     bool fieldActive) const
{
    ScreenshotOptions o;
    Background bg = Background(m_background);
    if (bg == Background::Auto)   // the historical defaults
        bg = forClipboard ? Background::White : Background::Theme;
    o.background = bg;

    switch (OverlayStyle(m_overlayStyle)) {
    case OverlayStyle::Light: o.darkOverlay = false; break;
    case OverlayStyle::Dark:  o.darkOverlay = true;  break;
    case OverlayStyle::Auto:
        // Dark on the dark theme backdrop; light on white, and on transparent,
        // because a figure is far more often placed on a white page.
        o.darkOverlay = (bg == Background::Theme);
        break;
    }
    o.autoCrop       = m_autoCrop;
    o.verticalLegend = verticalLegend;
    o.dpi            = m_dpi;
    o.fieldActive    = fieldActive;
    return o;
}

// ═══════════════════════════════════════════════════════════════════
//  PNG
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

    saveImageTo(item, fileName, verticalLegend, fieldActive);
}

void ExportController::saveImageTo(QQuickItem* item, const QString& fileName,
                                   bool verticalLegend, bool fieldActive)
{
    if (!item) {
        emit exportFailed(tr("Scene item is not available"));
        return;
    }
    QPointer<QQuickItem> guard(item);
    const ScreenshotOptions opts = currentOptions(false, verticalLegend, fieldActive);
    captureScene(item, opts.background,
        [this, guard, fileName, opts](QImage rawImg, bool offscreen) {
            if (rawImg.isNull()) {
                emit exportFailed(tr("Captured image is empty"));
                return;
            }
            ScreenshotOptions o = opts;
            o.fromGrab = !offscreen;
            QImage img = processScreenshot(rawImg, o, guard);
            if (!img.save(fileName)) {
                emit exportFailed(tr("Failed to save image to ") + fileName);
                return;
            }
            qDebug() << "Image saved:" << fileName << img.size() << "DPI:" << m_dpi;
            emit exportFinished(fileName);
        });
}

// ═══════════════════════════════════════════════════════════════════
//  Clipboard
// ═══════════════════════════════════════════════════════════════════
void ExportController::putImageOnClipboard(const QImage& img)
{
    auto* mime = new QMimeData;
    // PNG keeps the alpha channel; the plain image is what applications that
    // only take the platform bitmap (CF_DIB, image/bmp) read.
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    buffer.close();
    mime->setData(QStringLiteral("image/png"), png);   // X11 / Wayland, macOS (public.png)
#ifdef Q_OS_WIN
    // Windows applications (Office, browsers, Inkscape) read the registered
    // clipboard format named "PNG"; Qt would otherwise register "image/png".
    mime->setData(QStringLiteral("application/x-qt-windows-mime;value=\"PNG\""), png);
#endif
    mime->setImageData(img);
    QGuiApplication::clipboard()->setMimeData(mime);   // takes ownership
}

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
    QPointer<QQuickItem> guard(item);
    const ScreenshotOptions opts = currentOptions(true, verticalLegend, fieldActive);
    captureScene(item, opts.background,
        [this, guard, opts](QImage rawImg, bool offscreen) {
            if (rawImg.isNull()) {
                emit exportFailed(tr("Captured image is empty"));
                return;
            }
            ScreenshotOptions o = opts;
            o.fromGrab = !offscreen;
            const QImage img = processScreenshot(rawImg, o, guard);
            putImageOnClipboard(img);
            qDebug() << "Screenshot copied to clipboard" << img.size() << "DPI:" << m_dpi;
            emit exportFinished(QString());
        });
}

void ExportController::debugExport(QQuickItem* item, const QString& prefix, std::function<void()> finished)
{
    if (!item) { qWarning() << "[export] MATVIZ_EXPORT: no 3D view"; if (finished) finished(); return; }

    // One capture at a time (a second request would replace the first), so
    // the cases run as a chain.
    struct Case { QString name; bool clipboard; Background bg; int overlay; bool raw; };
    auto cases = std::make_shared<QList<Case>>(QList<Case>{
        { QStringLiteral("save"),        false, Background(m_background), m_overlayStyle, false },
        { QStringLiteral("copy"),        true,  Background(m_background), m_overlayStyle, false },
        { QStringLiteral("transparent"), false, Background::Transparent,  m_overlayStyle, false },
        { QStringLiteral("transparent_dark"), false, Background::Transparent, int(OverlayStyle::Dark), false },
        { QStringLiteral("raw_transparent"),  false, Background::Transparent, m_overlayStyle, true },
    });
    QPointer<QQuickItem> guard(item);
    auto next = std::make_shared<std::function<void()>>();
    *next = [this, cases, guard, prefix, finished, next]() {
        if (cases->isEmpty() || !guard) {
            if (finished) finished();
            *next = nullptr;   // break the cycle
            return;
        }
        const Case c = cases->takeFirst();
        const int savedBg = m_background, savedOverlay = m_overlayStyle;
        m_background = int(c.bg);
        m_overlayStyle = c.overlay;
        ScreenshotOptions opts = currentOptions(c.clipboard, m_legendVertical, false);
        m_background = savedBg;
        m_overlayStyle = savedOverlay;

        captureScene(guard, opts.background, [this, c, opts, guard, prefix, next](QImage raw, bool offscreen) {
            const QString file = prefix + QLatin1Char('_') + c.name + QStringLiteral(".png");
            if (c.raw) {
                raw.save(file);
            } else {
                ScreenshotOptions o = opts;
                o.fromGrab = !offscreen;
                const QImage img = processScreenshot(raw, o, guard);
                img.save(file);
                if (c.clipboard) {
                    putImageOnClipboard(img);
                    qDebug() << "[export] clipboard formats:"
                             << QGuiApplication::clipboard()->mimeData()->formats();
                }
            }
            qDebug() << "[export]" << c.name << raw.size() << (offscreen ? "off-screen" : "grab");
            if (*next) (*next)();
        });
    };
    (*next)();
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
                QImage img = replaceBackground(grab->image(), Qt::white)
                                 .convertToFormat(QImage::Format_RGB32);
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

    QString err;
    if (!writeVoxelsCSV(fileName, voxels, numCubes, &err)) {
        emit exportFailed(err);
        return;
    }

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

bool ExportController::writeGeometryHDF5(const QString& path, int32_t*** voxels, int size, QString* error)
{
    if (!voxels || size <= 0) {
        if (error) *error = QStringLiteral("no voxel grid to write");
        return false;
    }
    QString fileName = path;
    if (!fileName.endsWith(".h5", Qt::CaseInsensitive) &&
        !fileName.endsWith(".hdf5", Qt::CaseInsensitive) &&
        !fileName.endsWith(".hdf", Qt::CaseInsensitive)) {
        fileName += ".hdf5";
    }

    HDF5Wrapper hdf5(fileName.toStdString());
    int last_set = hdf5.readInt("/", "last_set");
    if (last_set == -1) { last_set = 1; hdf5.write("/", "last_set", last_set); }
    else                { last_set += 1; hdf5.update("/", "last_set", last_set); }
    const std::string prefix = ("/" + QString::number(last_set)).toStdString();

    hdf5.write(prefix, "voxels",    voxels, size);
    hdf5.write(prefix, "cubeSize",  size);
    hdf5.write(prefix, "numPoints", Parameters::instance()->getPoints());
    hdf5.write(prefix, "seed",      int(Parameters::seed));
    saveGeometryMetadataToHDF5(hdf5, prefix);
    qDebug().noquote() << "[HDF5] geometry written to" << fileName << "group" << QString::fromStdString(prefix);
    return true;
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
        saveGeometryMetadataToHDF5(hdf5Wrapper, prefix);
    }

    if (StressAnalysisController* sa = StressAnalysisController::getInstance()) {
        if (sa->hasStiffness() && sa->lastStiffness().ok) {
            const auto& r = sa->lastStiffness();
            const QString solver = sa->stiffnessIsFFT() ? QStringLiteral("fft") : QStringLiteral("ansys");
            std::vector<std::vector<float>> mat_S(6, std::vector<float>(6));
            std::vector<std::vector<float>> mat_C(6, std::vector<float>(6));
            std::vector<std::vector<float>> mat_P(6, std::vector<float>(6));
            for (int i = 0; i < 6; ++i)
                for (int j = 0; j < 6; ++j) {
                    mat_S[i][j] = float(r.S[i][j]);
                    mat_C[i][j] = float(r.C[i][j]);
                    mat_P[i][j] = float(r.P[i][j]);
                }
            std::vector<float> moduli(r.moduli, r.moduli + 6);

            hdf5Wrapper.write(prefix, "S_matrix",         mat_S);
            hdf5Wrapper.write(prefix, "C_matrix",         mat_C);
            hdf5Wrapper.write(prefix, "P_matrix",         mat_P);
            writeShearBasisAttribute(hdf5Wrapper, prefix);
            hdf5Wrapper.write(prefix, "Effective_Moduli", moduli);
            hdf5Wrapper.write(prefix, "solver",           solver);
            if (r.isFFT) hdf5Wrapper.write(prefix, "iterations_total", r.totalIterations);
        }
    }

    Parameters::filename = fileName;
    qDebug() << "HDF5 project saved:" << fileName;
    emit exportFinished(fileName);
}

void ExportController::openHDF5()
{
    // Opening replaces the grid, which a running generation or stress solve still works on
    // (Parameters::voxelsBusy()). Refused rather than queued: this is a file dialog the user
    // opened, and a solve can take minutes.
    const QString busy = tr("A structure is being generated or solved: open the project when that is done");
    if (Parameters::voxelsBusy()) {
        emit exportFailed(busy);
        return;
    }
    QString fileName = QFileDialog::getOpenFileName(
        nullptr, tr("Open MatViz3D HDF5 Project"), "",
        tr("HDF5 Files (*.h5 *.hdf5 *.hdf);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        qDebug() << "OpenHDF: no file selected";
        return;
    }
    if (Parameters::voxelsBusy()) {   // the dialog ran its own event loop
        emit exportFailed(busy);
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
        Parameters::percolation_rank6 = -1;

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

bool ExportController::writeVoxelsCSV(const QString& path, int32_t*** voxels,
                                      int numCubes, QString* error)
{
    if (!voxels || numCubes <= 0) {
        if (error) *error = "no structure to export";
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = "cannot open " + path + ": " + file.errorString();
        return false;
    }
    QTextStream out(&file);
    out << "X;Y;Z;Color\n";
    for (int x = 0; x < numCubes; ++x)
        for (int y = 0; y < numCubes; ++y)
            for (int z = 0; z < numCubes; ++z)
                out << x << ';' << y << ';' << z << ';' << voxels[x][y][z] << '\n';
    return true;
}
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
#include <QPdfWriter>
#include <QPaintEngine>
#include <QPageSize>
#include <QPageLayout>
#include <QFontDatabase>
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

namespace {
QString& fontFamilySlot()
{
    static QString family;
    return family;
}
} // namespace

void ExportController::registerFonts()
{
    const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-VariableFont_opsz,wght.ttf"));
    const QStringList fams = (id >= 0) ? QFontDatabase::applicationFontFamilies(id) : QStringList();
    if (!fams.isEmpty()) fontFamilySlot() = fams.first();
    else qWarning() << "[export] Inter could not be registered; labels use the default sans";
}

QString ExportController::exportFontFamily()
{
    const QString& f = fontFamilySlot();
    if (!f.isEmpty()) return f;
#ifdef Q_OS_WIN
    return QStringLiteral("Segoe UI");
#else
    return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
#endif
}

int ExportController::resolution() const
{
    if (!m_highDpi) return 0;
    if (m_dpi <= 150) return 1;
    if (m_dpi >= 600) return 3;
    return 2;
}

void ExportController::setResolution(int v)
{
    v = std::clamp(v, 0, 3);
    static const int kDpi[4] = { 96, 150, 300, 600 };
    const bool hi = v > 0;
    const int dpi = hi ? kDpi[v] : m_dpi;
    if (hi == m_highDpi && dpi == m_dpi) return;
    m_highDpi = hi;
    m_dpi = dpi;
    emit highDpiChanged();
    emit dpiChanged();
}

int ExportController::effectiveDpi(QQuickItem* item) const
{
    if (m_highDpi && m_dpi > 0) return m_dpi;
    const qreal dpr = (item && item->window()) ? item->window()->effectiveDevicePixelRatio() : 1.0;
    return qRound(96.0 * dpr);
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

/// A label font of `pt` points at the export scale (scale = DPI / 96, times
/// the overlay scale): pixel size pt * DPI / 72, so the text has that size in
/// points wherever the image is placed at its DPI.
QFont overlayFont(qreal pt, qreal scale, QFont::Weight weight)
{
    QFont f(ExportController::exportFontFamily());
    f.setPixelSize(std::max(6, qRound(pt * scale * 96.0 / 72.0)));
    f.setWeight(weight);
    f.setHintingPreference(QFont::PreferNoHinting);   // the same shapes at every size
    return f;
}

/// Text of the legend. On a PDF page it goes in as glyph outlines: Qt embeds
/// one instance of a variable font (Inter) per document, so as text every
/// label came out at the weight of whichever was drawn first.
void drawLabel(QPainter& p, const QRectF& r, int flags, const QString& text)
{
    QPaintEngine* e = p.paintEngine();
    if (!e || e->type() != QPaintEngine::Pdf) {
        p.drawText(r, flags, text);
        return;
    }
    const QFont f = p.font();
    const QFontMetricsF fm(f);
    const qreal w = fm.horizontalAdvance(text);
    qreal x = r.left();
    if (flags & Qt::AlignHCenter)      x = r.center().x() - w / 2.0;
    else if (flags & Qt::AlignRight)   x = r.right() - w;
    qreal y = r.top() + fm.ascent();
    if (flags & Qt::AlignVCenter)      y = r.center().y() + (fm.ascent() - fm.descent()) / 2.0;
    else if (flags & Qt::AlignBottom)  y = r.bottom() - fm.descent();
    QPainterPath path;
    path.addText(QPointF(x, y), f, text);
    p.fillPath(path, p.pen().color());
}

/// Tick values of a legend. When the range needs scientific notation, one
/// common power of ten goes into the title ("x10^6") and the ticks show
/// short mantissas: five labels of "1.234e+06" each crowded the bar.
struct LegendNumbers
{
    int exponent = 0;          // 0 = plain numbers
    QString titleSuffix;
    QString format(double v) const
    {
        if (std::isnan(v)) return QStringLiteral("0");
        const double m = exponent ? v / std::pow(10.0, exponent) : v;
        QString t = QString::number(m, 'g', exponent ? 3 : 4);
        if (t == QLatin1String("-0")) t = QStringLiteral("0");
        return t;
    }
};

LegendNumbers legendNumbers(double vmin, double vmax)
{
    LegendNumbers n;
    const double big = std::max(std::abs(vmin), std::abs(vmax));
    if (big > 0.0 && (big >= 1.0e4 || big < 1.0e-3)) {
        n.exponent = int(std::floor(std::log10(big)));
        static const char* sup[10] = { "\u2070", "\u00B9", "\u00B2", "\u00B3", "\u2074",
                                       "\u2075", "\u2076", "\u2077", "\u2078", "\u2079" };
        QString e;
        for (QChar c : QString::number(std::abs(n.exponent)))
            e += QString::fromUtf8(sup[c.digitValue()]);
        if (n.exponent < 0) e.prepend(QChar(0x207B));
        n.titleSuffix = QStringLiteral("  (\u00D710") + e + QLatin1Char(')');
    }
    return n;
}

void drawColorBarCard(QPainter& p, const QRectF& cardRect, qreal scale, bool vertical,
                      const QString& compName, double vmin, double vmax,
                      const QVector<QColor>& colors, bool darkTheme, bool translucent = false)
{
    // On a transparent export the card keeps a backing, so its text reads on
    // whatever the figure is placed on, but lets the page show through a bit.
    const QColor cardFill = darkTheme ? QColor(35, 35, 35, translucent ? 205 : 240)
                                      : QColor(255, 255, 255, translucent ? 215 : 245);
    const LegendNumbers numbers = legendNumbers(vmin, vmax);
    auto formatVal = [&numbers](double v) -> QString { return numbers.format(v); };
    const QString title = compName + numbers.titleSuffix;

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
        QFont titleFont = overlayFont(10.5, scale, QFont::DemiBold);
        p.setFont(titleFont);
        p.setPen(darkTheme ? QColor(230, 230, 230) : QColor(25, 25, 25));
        QRectF titleRect(cardRect.x() + padX, cardRect.y() + padY, contentW, titleH);
        drawLabel(p, titleRect, Qt::AlignLeft | Qt::AlignVCenter, title);

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

        QFont valFont = overlayFont(9.0, scale, QFont::Normal);
        QFont valBoldFont = overlayFont(9.0, scale, QFont::DemiBold);
        p.setPen(QPen(darkTheme ? QColor(160, 160, 160) : QColor(90, 90, 90), 1.0 * scale));

        const qreal tickFractions[5] = { 0.0, 0.25, 0.50, 0.75, 1.0 };
        const int levelIndices[5] = { 0, 2, 4, 6, 8 };

        // Which ticks get a label: all five, or fewer when neighbouring labels
        // would touch (a narrow figure, long numbers). Ends always.
        QString texts[5];
        qreal widths[5];
        for (int t = 0; t < 5; ++t) {
            texts[t] = formatVal(vmin + (vmax - vmin) * (levelIndices[t] / 8.0));
            widths[t] = QFontMetricsF((t == 0 || t == 4) ? valBoldFont : valFont).horizontalAdvance(texts[t]);
        }
        // Left edge and right edge of each label as drawn (ends aligned to the bar's ends).
        auto span = [&](int t, qreal& l, qreal& r) {
            const qreal cx = barX + tickFractions[t] * contentW;
            if (t == 0)      { l = cx;             r = cx + widths[t]; }
            else if (t == 4) { l = cx - widths[t]; r = cx; }
            else             { l = cx - widths[t] / 2.0; r = cx + widths[t] / 2.0; }
        };
        const qreal minGap = 6.0 * scale;
        auto fits = [&](std::initializer_list<int> ts) {
            qreal prevR = -1e9;
            for (int t : ts) {
                qreal l, r; span(t, l, r);
                if (l < prevR + minGap) return false;
                prevR = r;
            }
            return true;
        };
        bool show[5] = { true, true, true, true, true };
        if (!fits({0, 1, 2, 3, 4})) {
            show[1] = show[3] = false;
            if (!fits({0, 2, 4})) show[2] = false;
        }

        for (int t = 0; t < 5; ++t) {
            if (!show[t]) continue;
            const qreal tx = barX + tickFractions[t] * contentW;
            p.drawLine(QPointF(tx, tickY1), QPointF(tx, tickY2));
        }

        // Labels
        p.setPen(darkTheme ? QColor(220, 220, 220) : QColor(30, 30, 30));
        for (int t = 0; t < 5; ++t) {
            if (!show[t]) continue;
            p.setFont((t == 0 || t == 4) ? valBoldFont : valFont);
            qreal l, r; span(t, l, r);
            drawLabel(p, QRectF(l - 1.0, labelY, (r - l) + 2.0, valuesH),
                       Qt::AlignHCenter | Qt::AlignVCenter, texts[t]);
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
        QFont titleFont = overlayFont(10.5, scale, QFont::DemiBold);
        p.setFont(titleFont);
        p.setPen(darkTheme ? QColor(230, 230, 230) : QColor(25, 25, 25));
        QRectF titleRect(cardRect.x() + padX, cardRect.y() + padY, contentW, titleH);
        drawLabel(p, titleRect, Qt::AlignLeft | Qt::AlignVCenter, title);

        const qreal swatchW = 22.0 * scale;
        const qreal swatchH = 12.0 * scale;
        const qreal swatchX = cardRect.x() + padX;
        const qreal textX = swatchX + swatchW + 8.0 * scale;
        const qreal textW = contentW - swatchW - 8.0 * scale;

        QFont valFont = overlayFont(9.0, scale, QFont::Normal);
        QFont valBoldFont = overlayFont(9.0, scale, QFont::DemiBold);

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

            drawLabel(p, QRectF(textX, ry, textW, rowH), Qt::AlignLeft | Qt::AlignVCenter, str);
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

    QFont labelFont = overlayFont(10.5, scale, QFont::Bold);
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

// ── Step 3 Helper: what the legend shows ─────────────────────────────
struct LegendData
{
    bool valid = false;
    double vmin = 0.0, vmax = 0.0;
    QString name;
    QVector<QColor> colors;
};

LegendData gatherLegend(OpenGLWidgetQML* ogl, bool fieldActive)
{
    LegendData d;
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
    if (!hasField) return d;

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
    if (ogl) colors = ogl->getColorMap(9);
    if (colors.size() < 9) {
        auto palette = ogl ? ogl->getColorMapPalette() : OpenGLWidgetQML::ColorMapPalette::Rainbow;
        auto rawMap = matviz_cmap::createColorMap(9, palette);
        colors.resize(9);
        for (int i = 0; i < 9; ++i) colors[i] = QColor(rawMap[i][0], rawMap[i][1], rawMap[i][2]);
    }

    d.valid = true;
    d.vmin = vmin;
    d.vmax = vmax;
    d.name = compName;
    d.colors = colors;
    return d;
}

QSizeF legendCardSize(bool vertical, qreal scale, qreal maxAllowedW)
{
    qreal w = vertical ? 180.0 * scale : 380.0 * scale;
    const qreal h = vertical ? 175.0 * scale : 68.0 * scale;
    if (maxAllowedW > 0 && w > maxAllowedW) w = maxAllowedW;
    return QSizeF(w, h);
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

    const LegendData d = gatherLegend(ogl, false);
    if (!d.valid) {
        qDebug() << "[overlayColorBar] No active stress/strain field detected";
        return;
    }
    const QSizeF card = legendCardSize(vertical, scale, imgW - 40.0 * scale);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    drawColorBarCard(p, QRectF(QPointF(20.0 * scale, imgH - card.height() - 20.0 * scale), card),
                     scale, vertical, d.name, d.vmin, d.vmax, d.colors, darkTheme);
}

// ═══════════════════════════════════════════════════════════════════
//  Screenshot layout: background, crop, legend and gizmo positions
// ═══════════════════════════════════════════════════════════════════
ExportController::Composition ExportController::layoutScreenshot(const QImage& rawImg,
                                                                 const ScreenshotOptions& o,
                                                                 QQuickItem* item)
{
    Composition c;
    if (rawImg.isNull()) return c;

    // Scale is strictly DPI-proportional: 96 DPI = 1.0, 300 DPI = 3.125
    const qreal scale = (o.dpi > 0) ? std::clamp((qreal)o.dpi / 96.0, 0.75, 8.0)
                                    : std::clamp(rawImg.width() / 1280.0, 0.75, 5.0);
    c.transparent = (o.background == Background::Transparent);
    c.dpi = o.dpi > 0 ? o.dpi : 96;
    c.item = item;

    OpenGLWidgetQML* ogl = qobject_cast<OpenGLWidgetQML*>(item);
    if (!ogl) ogl = OpenGLWidgetQML::getInstance();

    // ── 1. Scene image on the requested background ───────────────────
    QImage sceneImg = obtainSceneImage(rawImg, o, item, scale, c.background);
    c.sceneSize = sceneImg.size();

    // ── 2. Crop to the model ─────────────────────────────────────────
    // The crop margin is measured in the capture's own pixels.
    const qreal captureScale = (item && item->width() > 0) ? sceneImg.width() / item->width() : scale;
    c.cropRect = QRect(QPoint(0, 0), sceneImg.size());
    QImage croppedModel = o.autoCrop
        ? cropToUsefulSize(sceneImg, c.background, o.targetWidthPx > 0 ? captureScale : scale, &c.cropRect)
        : sceneImg;

    // A figure of a given width: the model is resampled to it (the capture
    // was rendered larger, so this is a downscale), the overlays below keep
    // their size in points.
    const qreal os = scale * std::clamp(o.overlayScale, 0.3, 3.0);   // overlay scale
    c.overlayScale = os;
    qreal legendMaxW = -1.0;
    if (o.targetWidthPx > 0 && !croppedModel.isNull()) {
        if (croppedModel.width() != o.targetWidthPx) {
            c.resample = qreal(o.targetWidthPx) / croppedModel.width();
            croppedModel = croppedModel.scaledToWidth(o.targetWidthPx, Qt::SmoothTransformation);
        }
        // At most ~60 % of the figure: a 10 cm card under an 8.5 cm figure
        // (its size as on screen) is what a journal column cannot take.
        legendMaxW = std::min(o.targetWidthPx - (16.0 + 85.0 + 16.0 + 20.0) * os,
                              0.6 * o.targetWidthPx);
    }
    // Figure pixels per logical pixel of the view: what outline and line
    // widths scale with.
    c.lineScale = captureScale * c.resample;

    // ── 3, 4. Legend and gizmo, in the overlay style asked for ───────
    const LegendData legend = gatherLegend(ogl, o.fieldActive);
    c.hasLegend = legend.valid;
    c.vmin = legend.vmin;
    c.vmax = legend.vmax;
    c.legendName = legend.name;
    c.legendColors = legend.colors;
    c.verticalLegend = o.verticalLegend;
    c.translucentLegend = c.transparent;
    c.darkOverlay = o.darkOverlay;
    const QSizeF card = legendCardSize(o.verticalLegend, os, legendMaxW);

    // ── 5. Layout ────────────────────────────────────────────────────
    int finalW = croppedModel.width();
    int finalH = croppedModel.height();
    const qreal marginL = 16.0 * os;
    const qreal marginR = 16.0 * os;
    const qreal bottomPad = 14.0 * os;
    const qreal gapAbove = 14.0 * os;

    if (o.autoCrop) {
        // Extra bottom space only for a colour bar; without one the gizmo
        // sits in the bottom-right corner of the cropped model.
        int extraBottom = 0;
        if (c.hasLegend) {
            const qreal bottomItemH = std::max(card.height(), 80.0 * os);
            extraBottom = qRound(gapAbove + bottomItemH + bottomPad);
        }

        const qreal gizmoBoxW = 85.0 * os;
        const qreal minReqW = (c.hasLegend ? (marginL + card.width() + 20.0 * os) : marginL)
                            + gizmoBoxW + marginR;

        finalW = std::max(croppedModel.width(), qRound(minReqW));
        finalH = croppedModel.height() + extraBottom;
        c.modelPos = QPoint((finalW - croppedModel.width()) / 2, 0);

        if (c.hasLegend)
            c.legendRect = QRectF(QPointF(qRound(marginL), qRound(finalH - bottomPad - card.height())), card);
        c.gizmoCenter = QPointF(finalW - marginR - 45.0 * os, finalH - bottomPad - 42.0 * os);
    } else {
        if (c.hasLegend)
            c.legendRect = QRectF(QPointF(qRound(20.0 * os), qRound(finalH - card.height() - 20.0 * os)), card);
        c.gizmoCenter = QPointF(finalW - 60.0 * os, finalH - 55.0 * os);
    }
    c.size = QSize(finalW, finalH);
    c.model = croppedModel;
    return c;
}

void ExportController::paintOverlays(QPainter& p, const Composition& c)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    if (c.hasLegend)
        drawColorBarCard(p, c.legendRect, c.overlayScale, c.verticalLegend, c.legendName,
                         c.vmin, c.vmax, c.legendColors, c.darkOverlay, c.translucentLegend);
    OpenGLWidgetQML* ogl = qobject_cast<OpenGLWidgetQML*>(c.item.data());
    if (!ogl) ogl = OpenGLWidgetQML::getInstance();
    drawOrientationGizmo(p, c.gizmoCenter, c.overlayScale, ogl, c.darkOverlay);
    p.restore();
}

// ═══════════════════════════════════════════════════════════════════
//  PDF: the layout painted on a page of the figure's size
// ═══════════════════════════════════════════════════════════════════
bool ExportController::writePdf(const Composition& c, const QString& fileName,
                                const std::vector<PdfFace>* faces)
{
    if (c.size.isEmpty()) return false;
    QPdfWriter writer(fileName);
    writer.setCreator(QStringLiteral("MatViz3D"));
    writer.setTitle(c.hasLegend ? c.legendName : QStringLiteral("MatViz3D figure"));
    writer.setResolution(c.dpi);
    const QSizeF mm(c.size.width() * 25.4 / c.dpi, c.size.height() * 25.4 / c.dpi);
    writer.setPageLayout(QPageLayout(QPageSize(mm, QPageSize::Millimeter, QString(), QPageSize::ExactMatch),
                                     QPageLayout::Portrait, QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter));
    QPainter p;
    if (!p.begin(&writer)) return false;
    // The page in device units is the figure in pixels up to rounding of
    // the page size: map one onto the other exactly.
    p.scale(qreal(writer.width()) / c.size.width(), qreal(writer.height()) / c.size.height());

    if (!c.transparent)
        p.fillRect(QRectF(QPointF(0, 0), QSizeF(c.size)), c.background);

    const QRectF modelRect(QPointF(c.modelPos), QSizeF(c.model.size()));
    if (faces) {
        // Scene pixels -> figure pixels; clipped to the model's place, as the
        // image would be. Each polygon is also stroked thinly in its own
        // colour: viewers anti-alias filled polygons, and neighbours would
        // otherwise show hairline seams.
        p.save();
        p.setClipRect(modelRect);
        p.translate(QPointF(c.modelPos));
        p.scale(c.resample, c.resample);
        p.translate(-QPointF(c.cropRect.topLeft()));
        p.setRenderHint(QPainter::Antialiasing, true);
        const qreal seam = 0.6 / c.resample;
        const qreal edgeW = 1.5 * c.lineScale / c.resample;
        for (const PdfFace& f : *faces) {
            const QColor col = QColor::fromRgb(f.color);
            if (f.kind == 1) {
                p.setPen(QPen(col, f.width * c.lineScale / c.resample, Qt::SolidLine, Qt::FlatCap));
                p.drawLine(f.pts[0], f.pts[1]);
                continue;
            }
            if (f.kind == 2) {
                p.setPen(Qt::NoPen);
                p.setBrush(col);
                p.drawPolygon(f.pts, 3);
                continue;
            }
            p.setPen(QPen(col, seam, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(col);
            p.drawPolygon(f.pts, 4);
            if (f.edgeMask) {
                p.setPen(QPen(QColor::fromRgb(f.edgeColor), edgeW, Qt::SolidLine, Qt::RoundCap));
                for (int e = 0; e < 4; ++e)
                    if (f.edgeMask & (1 << e)) p.drawLine(f.pts[e], f.pts[(e + 1) % 4]);
            }
        }
        p.restore();
    } else {
        // Lossless: without the hint QPdfWriter stores the model as a JPEG
        // whenever that is smaller (and it always is for a rendering).
        // Alpha goes in as a soft mask.
        p.save();
        p.setRenderHint(QPainter::LosslessImageRendering, true);
        p.drawImage(modelRect, c.model);
        p.restore();
    }
    paintOverlays(p, c);
    return p.end();
}

// ═══════════════════════════════════════════════════════════════════
//  Process Screenshot: the layout painted into an image
// ═══════════════════════════════════════════════════════════════════
QImage ExportController::processScreenshot(const QImage& rawImg, const ScreenshotOptions& o,
                                           QQuickItem* item)
{
    if (rawImg.isNull()) {
        qWarning() << "[processScreenshot] rawImg is null, aborting";
        return rawImg;
    }
    const Composition c = layoutScreenshot(rawImg, o, item);

    // Compose (premultiplied ARGB, source-over)
    QImage resultImg(c.size, QImage::Format_ARGB32_Premultiplied);
    resultImg.fill(c.background);   // QColor(0,0,0,0) -> fully transparent
    {
        QPainter p(&resultImg);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(c.modelPos, c.model);
        paintOverlays(p, c);
    }

    // An opaque export is written without an alpha channel: smaller files,
    // and no viewer can show a stray semi-transparent pixel.
    if (!c.transparent)
        resultImg = resultImg.convertToFormat(QImage::Format_RGB32);

    setDpiMetadata(resultImg, o.dpi);
    qDebug().noquote() << QStringLiteral("[processScreenshot] %1 x %2 at %3 DPI (%7 cm wide), background %4, %5 overlay, %6")
                              .arg(resultImg.width()).arg(resultImg.height()).arg(o.dpi)
                              .arg(c.transparent ? "transparent" : o.background == Background::White ? "white" : "theme")
                              .arg(o.darkOverlay ? "dark" : "light")
                              .arg(o.fromGrab ? "grabbed frame" : "off-screen render")
                              .arg(o.dpi > 0 ? resultImg.width() * 2.54 / o.dpi : 0.0, 0, 'f', 2);
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
    if (m_widthCm > 0.0 && item->width() > 0) {
        // Render 1.5x the figure's width: the crop then still leaves more
        // pixels than the figure needs, and the model is downscaled to it.
        const qreal targetPx = m_widthCm / 2.54 * effectiveDpi(item);
        factor = 1.5 * targetPx / item->width();
    }
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
        const QSize px = exportPixelSize(item);
        // A window that is not exposed renders no frame: render headless.
        if (!item->window() || !item->window()->isExposed()) {
            QImage img = ogl->renderHeadless(px, bg);
            if (!img.isNull()) { done(img, true); return; }
        }
        // The request is served by the next frame, and a window that is not
        // exposed (minimised, covered on some platforms, display off) renders
        // none. After a while give up on it and render headless, or grab;
        // whichever answers first wins.
        auto answered = std::make_shared<bool>(false);
        const bool queued = ogl->requestCapture(px, bg, this,
            [this, guard, done, answered](QImage img) {
                if (*answered) return;
                *answered = true;
                if (!img.isNull()) { done(img, true); return; }
                qWarning() << "[export] off-screen capture failed, falling back to grabToImage()";
                if (guard) grabScene(guard, done);
                else       done(QImage(), false);
            });
        if (queued) {
            QTimer::singleShot(10000, this, [this, guard, done, answered, px, bg]() {
                if (*answered) return;
                *answered = true;
                OpenGLWidgetQML* w = qobject_cast<OpenGLWidgetQML*>(guard.data());
                QImage img = w ? w->renderHeadless(px, bg) : QImage();
                if (!img.isNull()) {
                    qWarning() << "[export] no frame rendered the capture in 10 s, rendered it headless";
                    done(img, true);
                    return;
                }
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
    // grabToImage() multiplies the size it is given by the window's device
    // pixel ratio, so ask for the pixel size divided by it: on a 2x screen the
    // fallback otherwise returned twice the pixels its DPI tag accounts for.
    QSize targetSize = exportPixelSize(item);
    const qreal dpr = item->window() ? item->window()->effectiveDevicePixelRatio() : 1.0;
    if (dpr > 0 && !targetSize.isEmpty())
        targetSize = (QSizeF(targetSize) / dpr).toSize();

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
ExportController::ScreenshotOptions ExportController::currentOptions(QQuickItem* item, bool forClipboard,
                                                                     bool verticalLegend, bool fieldActive) const
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
    // The DPI the image really has: with High DPI off it is the screen's
    // (the overlays were drawn at 300 DPI on a 96 DPI frame before).
    o.dpi            = effectiveDpi(item);
    o.fieldActive    = fieldActive;
    if (m_widthCm > 0.0) {
        o.targetWidthPx = qRound(m_widthCm / 2.54 * o.dpi);
        o.overlayScale  = 0.8;   // values 7.2 pt, title 8.4 pt
    }
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

    QString selected;
    QString fileName = QFileDialog::getSaveFileName(
        nullptr, tr("Save Image"), "",
        tr("PNG image (*.png);;PDF document (*.pdf)"), &selected);
    if (fileName.isEmpty())
        return;

    const bool pdf = fileName.endsWith(".pdf", Qt::CaseInsensitive)
                  || (!fileName.endsWith(".png", Qt::CaseInsensitive) && selected.contains("pdf"));
    if (pdf) {
        if (!fileName.endsWith(".pdf", Qt::CaseInsensitive)) fileName += ".pdf";
        savePdfTo(item, fileName, verticalLegend, fieldActive, m_pdfVectorModel);
        return;
    }
    if (!fileName.endsWith(".png", Qt::CaseInsensitive))
        fileName += ".png";

    saveImageTo(item, fileName, verticalLegend, fieldActive);
}

void ExportController::saveAsPdf(QQuickItem* item, bool verticalLegend, bool fieldActive)
{
    if (!item) {
        emit exportFailed(tr("Scene item is not available"));
        return;
    }
    QString fileName = QFileDialog::getSaveFileName(nullptr, tr("Save as PDF"), "",
                                                    tr("PDF document (*.pdf)"));
    if (fileName.isEmpty())
        return;
    if (!fileName.endsWith(".pdf", Qt::CaseInsensitive))
        fileName += ".pdf";
    savePdfTo(item, fileName, verticalLegend, fieldActive, m_pdfVectorModel);
}

void ExportController::saveImageTo(QQuickItem* item, const QString& fileName,
                                   bool verticalLegend, bool fieldActive)
{
    if (!item) {
        emit exportFailed(tr("Scene item is not available"));
        return;
    }
    if (fileName.endsWith(".pdf", Qt::CaseInsensitive)) {
        savePdfTo(item, fileName, verticalLegend, fieldActive, m_pdfVectorModel);
        return;
    }
    QPointer<QQuickItem> guard(item);
    const ScreenshotOptions opts = currentOptions(item, false, verticalLegend, fieldActive);
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
            qDebug() << "Image saved:" << fileName << img.size() << "DPI:" << o.dpi;
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
    const ScreenshotOptions opts = currentOptions(item, true, verticalLegend, fieldActive);
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
            qDebug() << "Screenshot copied to clipboard" << img.size() << "DPI:" << o.dpi;
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
        { QStringLiteral("image"),       false, Background(m_background), m_overlayStyle, false },   // -> .svg
        { QStringLiteral("pdf"),         false, Background(m_background), m_overlayStyle, false },   // -> .pdf
        { QStringLiteral("pdf_vector"),  false, Background(m_background), m_overlayStyle, false },   // -> .pdf
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
        ScreenshotOptions opts = currentOptions(guard, c.clipboard, m_legendVertical, false);
        m_background = savedBg;
        m_overlayStyle = savedOverlay;

        captureScene(guard, opts.background, [this, c, opts, guard, prefix, next](QImage raw, bool offscreen) {
            const QString file = prefix + QLatin1Char('_') + c.name + QStringLiteral(".png");
            if (c.raw) {
                raw.save(file);
            } else {
                ScreenshotOptions o = opts;
                o.fromGrab = !offscreen;
                if (c.name.startsWith(QLatin1String("pdf"))) {
                    const Composition comp = layoutScreenshot(raw, o, guard);
                    std::vector<PdfFace> faces;
                    OpenGLWidgetQML* ogl = qobject_cast<OpenGLWidgetQML*>(guard.data());
                    const bool vector = c.name == QLatin1String("pdf_vector") && ogl;
                    if (vector)
                        for (const svgx::ShadedFace& f : ogl->vectorFaces(comp.sceneSize))
                            faces.push_back({ { f.pts[0], f.pts[1], f.pts[2], f.pts[3] },
                                              f.color, f.edgeColor, f.edgeMask, f.kind, f.width });
                    writePdf(comp, prefix + QLatin1Char('_') + c.name + QStringLiteral(".pdf"),
                             vector ? &faces : nullptr);
                    qDebug() << "[export]" << c.name << (vector ? faces.size() : 0) << "polygons";
                } else {
                const QImage img = processScreenshot(raw, o, guard);
                if (c.name == QLatin1String("image"))   // the SVG container of Save as SVG (image)
                    writeEmbeddedSvg(img, o.dpi, prefix + QStringLiteral("_image.svg"));
                else
                    img.save(file);
                if (c.clipboard) {
                    putImageOnClipboard(img);
                    qDebug() << "[export] clipboard formats:"
                             << QGuiApplication::clipboard()->mimeData()->formats();
                }
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
        nullptr, tr("Save as SVG (image)"), "",
        tr("SVG Files (*.svg)"));
    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".svg", Qt::CaseInsensitive))
        fileName += ".svg";
    saveAsSVGTo(item, fileName, m_legendVertical);
}

bool ExportController::writeEmbeddedSvg(const QImage& img, int dpiIn, const QString& fileName)
{
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    buffer.close();

    const int w = img.width(), h = img.height();
    const double dpi = dpiIn > 0 ? dpiIn : 96.0;
    const QString svg =
        QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
                "width=\"%1mm\" height=\"%2mm\" viewBox=\"0 0 %3 %4\">\n"
                "  <image width=\"%3\" height=\"%4\" "
                "xlink:href=\"data:image/png;base64,%5\"/>\n"
                "</svg>\n")
            .arg(w * 25.4 / dpi, 0, 'f', 2).arg(h * 25.4 / dpi, 0, 'f', 2)
            .arg(w).arg(h).arg(QString::fromLatin1(png.toBase64()));

    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream(&f) << svg;
    return true;
}

void ExportController::saveAsSVGTo(QQuickItem* item, const QString& fileName, bool verticalLegend)
{
    if (!item) { emit exportFailed(tr("Scene item is not available")); return; }
    // The same capture, crop, legend and gizmo as Save; only the container
    // differs. Width and height in mm, so the figure keeps its physical size.
    QPointer<QQuickItem> guard(item);
    const ScreenshotOptions opts = currentOptions(item, false, verticalLegend, false);
    captureScene(item, opts.background,
        [this, guard, fileName, opts](QImage rawImg, bool offscreen) {
            if (rawImg.isNull()) { emit exportFailed(tr("Captured image is empty")); return; }
            ScreenshotOptions o = opts;
            o.fromGrab = !offscreen;
            const QImage img = processScreenshot(rawImg, o, guard);

            if (!writeEmbeddedSvg(img, o.dpi, fileName)) {
                emit exportFailed(tr("Cannot open file for writing: ") + fileName);
                return;
            }
            qDebug() << "SVG saved:" << fileName << img.size() << "DPI:" << o.dpi;
            emit exportFinished(fileName);
        });
}

void ExportController::savePdfTo(QQuickItem* item, const QString& fileName, bool verticalLegend,
                                 bool fieldActive, bool vectorModel)
{
    if (!item) { emit exportFailed(tr("Scene item is not available")); return; }
    QPointer<QQuickItem> guard(item);
    const ScreenshotOptions opts = currentOptions(item, false, verticalLegend, fieldActive);
    captureScene(item, opts.background,
        [this, guard, fileName, opts, vectorModel](QImage rawImg, bool offscreen) {
            if (rawImg.isNull()) { emit exportFailed(tr("Captured image is empty")); return; }
            ScreenshotOptions o = opts;
            o.fromGrab = !offscreen;
            const Composition c = layoutScreenshot(rawImg, o, guard);
            std::vector<PdfFace> faces;
            OpenGLWidgetQML* ogl = qobject_cast<OpenGLWidgetQML*>(guard.data());
            const bool vector = vectorModel && ogl;
            if (vector) {
                for (const svgx::ShadedFace& f : ogl->vectorFaces(c.sceneSize)) {
                    PdfFace pf;
                    for (int k = 0; k < 4; ++k) pf.pts[k] = f.pts[k];
                    pf.color = f.color;
                    pf.edgeColor = f.edgeColor;
                    pf.edgeMask = f.edgeMask;
                    pf.kind = f.kind;
                    pf.width = f.width;
                    faces.push_back(pf);
                }
            }
            if (!writePdf(c, fileName, vector ? &faces : nullptr)) {
                emit exportFailed(tr("Failed to write PDF ") + fileName);
                return;
            }
            qDebug().noquote() << QStringLiteral("PDF saved: %1, %2 x %3 mm at %4 DPI, model %5")
                                      .arg(fileName)
                                      .arg(c.size.width() * 25.4 / c.dpi, 0, 'f', 1)
                                      .arg(c.size.height() * 25.4 / c.dpi, 0, 'f', 1).arg(c.dpi)
                                      .arg(vector ? QStringLiteral("as %1 polygons").arg(faces.size())
                                                  : QStringLiteral("as an image"));
            emit exportFinished(fileName);
        });
}

bool ExportController::headlessScreenshot(const HeadlessShot& shot, QString* error)
{
    auto fail = [error](const QString& m) { if (error) *error = m; return false; };
    if (!Parameters::voxels) return fail(QStringLiteral("no structure to render"));
    const int n = Parameters::instance()->getSize();

    // A 3D view without a window: the item computes the mesh and the camera
    // exactly as on screen, and renders through renderHeadless().
    OpenGLWidgetQML view;
    view.setSize(QSizeF(shot.viewSize));
    view.setShowCornerAxes(false);
    if (shot.outlines > 0) {
        view.setOutlineKind(shot.outlines);
        view.setPlotWireFrame(true);
    }
    view.setVoxels(Parameters::voxels, static_cast<short>(n));

    ExportController ec;
    ec.setDpi(shot.dpi > 0 ? shot.dpi : 300);
    ec.setHighDpi(true);
    ec.setWidthCm(shot.widthCm);
    ec.setBackground(shot.background);
    ScreenshotOptions o = ec.currentOptions(&view, false, false, false);
    const QImage raw = view.renderHeadless(ec.exportPixelSize(&view),
                                           o.background == Background::White       ? RenderOpenGL::CaptureBackground::White
                                         : o.background == Background::Transparent ? RenderOpenGL::CaptureBackground::Transparent
                                                                                   : RenderOpenGL::CaptureBackground::Theme);
    if (raw.isNull()) return fail(QStringLiteral("no OpenGL context for a headless render"));

    if (shot.file.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)) {
        const Composition c = layoutScreenshot(raw, o, &view);
        std::vector<PdfFace> faces;
        if (shot.pdfVector)
            for (const svgx::ShadedFace& f : view.vectorFaces(c.sceneSize))
                faces.push_back({ { f.pts[0], f.pts[1], f.pts[2], f.pts[3] },
                                  f.color, f.edgeColor, f.edgeMask, f.kind, f.width });
        if (!writePdf(c, shot.file, shot.pdfVector ? &faces : nullptr))
            return fail(QStringLiteral("cannot write ") + shot.file);
        qInfo().noquote() << QStringLiteral("Screenshot: %1 (%2 x %3 mm at %4 DPI, model %5)")
                                 .arg(shot.file).arg(c.size.width() * 25.4 / c.dpi, 0, 'f', 1)
                                 .arg(c.size.height() * 25.4 / c.dpi, 0, 'f', 1).arg(c.dpi)
                                 .arg(shot.pdfVector ? QStringLiteral("%1 polygons").arg(faces.size())
                                                     : QStringLiteral("image"));
        return true;
    }
    const QImage img = processScreenshot(raw, o, &view);
    if (!img.save(shot.file)) return fail(QStringLiteral("cannot write ") + shot.file);
    qInfo().noquote() << QStringLiteral("Screenshot: %1 (%2 x %3 px at %4 DPI)")
                             .arg(shot.file).arg(img.width()).arg(img.height()).arg(o.dpi);
    return true;
}

void ExportController::saveItemImage(QQuickItem* item, const QString& fileName)
{
    if (!item || !item->window()) { emit exportFailed(tr("Item is not available")); return; }
    const int dpi = effectiveDpi(item);
    // grabToImage() multiplies the size it is given by the device pixel ratio.
    const qreal dpr = item->window()->effectiveDevicePixelRatio();
    const QSize target = (item->size() * (dpi / 96.0) / std::max<qreal>(dpr, 0.5)).toSize();
    auto grab = target.isEmpty() ? item->grabToImage() : item->grabToImage(target);
    if (!grab) { emit exportFailed(tr("grabToImage() failed — item has no window")); return; }
    connect(grab.data(), &QQuickItemGrabResult::ready, this, [this, grab, fileName, dpi]() {
        QImage img = grab->image();
        setDpiMetadata(img, dpi);
        if (!img.save(fileName)) { emit exportFailed(tr("Failed to save image to ") + fileName); return; }
        qDebug() << "Image saved:" << fileName << img.size() << "DPI:" << dpi;
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
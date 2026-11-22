#ifndef EXPORTCONTROLLER_H
#define EXPORTCONTROLLER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QColor>
#include <QPointer>
#include <QRectF>
#include <QVector>
#include <functional>
#include <vector>
#include <algorithm>

class QQuickItem;
class QPainter;

/// A voxel face for a vector PDF: corners in scene pixels, shaded colour,
/// outline colour and edge mask (bit e = edge from corner e to e + 1).
/// kind 0 face, 1 line pts[0] -> pts[1] of `width` logical pixels, 2 triangle.
struct PdfFace { QPointF pts[4]; QRgb color; QRgb edgeColor; int edgeMask; int kind = 0; float width = 0.0f; };

/**
 * @brief Scene export: PNG, clipboard, SVG, CSV, VRML.
 *
 * Raster capture of the main 3D view is a real off-screen render on the
 * render thread (OpenGLWidgetQML::requestCapture(): multisampled FBO of the
 * target size, so 300/600 DPI means more pixels, not an upscaled frame).
 * Any other item (the anisotropy surface, or the 3D view before its renderer
 * exists) falls back to QQuickItem::grabToImage(), which reads the item's
 * on-screen texture and only rescales it.
 *
 * Data export (CSV, VRML) takes voxels directly from the renderer.
 *
 * Calls from QML:
 *     exportController.saveAsImage(glWidget)
 *     exportController.exportToCSV()
 */
class ExportController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool legendVertical READ legendVertical WRITE setLegendVertical NOTIFY legendVerticalChanged)
    Q_PROPERTY(bool autoCrop       READ autoCrop       WRITE setAutoCrop       NOTIFY autoCropChanged)
    Q_PROPERTY(bool highDpi        READ highDpi        WRITE setHighDpi        NOTIFY highDpiChanged)
    Q_PROPERTY(int  dpi            READ dpi            WRITE setDpi            NOTIFY dpiChanged)
    /// Resolution preset of the screenshot popup: 0 the screen's own pixels,
    /// 1 150 DPI, 2 300 DPI, 3 600 DPI (sets highDpi and dpi).
    Q_PROPERTY(int  resolution     READ resolution     WRITE setResolution     NOTIFY dpiChanged)
    /// Width of the exported figure in cm at the DPI (0 = the view's size).
    /// With a width the model is scaled to it and the legend and axis labels
    /// keep their size in points (compact: 7-8 pt text, as journals ask).
    Q_PROPERTY(double widthCm      READ widthCm        WRITE setWidthCm        NOTIFY widthCmChanged)
    /// PDF: the model as vector polygons (one per visible voxel face, shaded
    /// like the screen) instead of the rendered image. Legend and axes are
    /// vectors either way.
    Q_PROPERTY(bool pdfVectorModel READ pdfVectorModel WRITE setPdfVectorModel NOTIFY pdfVectorModelChanged)
    /// The same as an index for the popup's segmented row: 0 image, 1 vector.
    Q_PROPERTY(int  pdfModel       READ pdfModel       WRITE setPdfModel       NOTIFY pdfVectorModelChanged)
    /// Background of Save / Copy: 0 Auto (Save: theme, Copy: white), 1 Theme,
    /// 2 White, 3 Transparent. See Background.
    Q_PROPERTY(int  background     READ background     WRITE setBackground     NOTIFY backgroundChanged)
    /// Legend card and axis labels: 0 Auto (dark on the theme background,
    /// light on white or transparent), 1 Light, 2 Dark. See OverlayStyle.
    Q_PROPERTY(int  overlayStyle   READ overlayStyle   WRITE setOverlayStyle   NOTIFY overlayStyleChanged)

public:
    enum class Background   { Auto = 0, Theme = 1, White = 2, Transparent = 3 };
    enum class OverlayStyle { Auto = 0, Light = 1, Dark = 2 };

    /// What processScreenshot() makes of a raw scene image. `background` is a
    /// resolved one (never Auto), `darkOverlay` the resolved overlay style.
    struct ScreenshotOptions
    {
        Background background     = Background::Theme;
        bool       darkOverlay    = true;
        bool       autoCrop       = true;
        bool       verticalLegend = false;
        int        dpi            = 300;
        bool       fieldActive    = false;
        /// The raw image is the on-screen frame from grabToImage(): it still
        /// has the corner triad and the theme backdrop, which are replaced by
        /// colour matching. False for the off-screen render, which is already
        /// on the requested background, triad-free.
        bool       fromGrab       = false;
        /// Width of the whole figure in pixels (0 = what the crop gives):
        /// the model is resampled to it, the overlays are not.
        int        targetWidthPx  = 0;
        /// Size of the legend, gizmo and their text relative to the defaults
        /// (title 10.5 pt, values 9 pt); 0.8 in a figure of a given width.
        qreal      overlayScale   = 1.0;
    };

    /// Font family of every exported label: Inter, registered from the
    /// resources by registerFonts() (main.cpp), so a figure looks the same on
    /// every platform; "Segoe UI" / the default sans before that.
    static QString exportFontFamily();
    static void registerFonts();

    explicit ExportController(QObject* parent = nullptr);

    Q_INVOKABLE void saveAsImage(QQuickItem* item);
    Q_INVOKABLE void saveAsImage(QQuickItem* item, bool verticalLegend, bool fieldActive = false);
    Q_INVOKABLE void copyToClipboard(QQuickItem* item);
    Q_INVOKABLE void copyToClipboard(QQuickItem* item, bool verticalLegend, bool fieldActive = false);
    /// Save without a dialog (scripts, the MATVIZ_EXPORT check).
    Q_INVOKABLE void saveImageTo(QQuickItem* item, const QString& fileName,
                                 bool verticalLegend, bool fieldActive = false);
    /// MATVIZ_EXPORT=<prefix>: write <prefix>_theme.png (what Save writes),
    /// <prefix>_clipboard.png (what Copy puts on the clipboard) and
    /// <prefix>_raw_transparent.png (the bare capture on a transparent
    /// background), then call `finished`.
    void debugExport(QQuickItem* item, const QString& prefix, std::function<void()> finished);
    /// Save dialog for a PDF: one page of the figure's size, legend and axes
    /// as vectors, the model as an image or as polygons (pdfVectorModel).
    Q_INVOKABLE void saveAsPdf(QQuickItem* item, bool verticalLegend, bool fieldActive = false);
    Q_INVOKABLE void saveAsSVG(QQuickItem* item);
    Q_INVOKABLE void saveAsVectorSVG(QQuickItem* item);  // true-vector, not raster
    /// The export pipeline (capture, crop, legend, gizmo) written as a PNG
    /// embedded in an SVG whose width and height are in millimetres at the DPI.
    void saveAsSVGTo(QQuickItem* item, const QString& fileName, bool verticalLegend);
    /// The export pipeline written as a one-page PDF: legend and axis triad
    /// as vectors (text as text), the model as the image or, with
    /// `vectorModel`, as polygons.
    void savePdfTo(QQuickItem* item, const QString& fileName, bool verticalLegend, bool fieldActive,
                   bool vectorModel);
    /// A PNG of `img` in an SVG of its physical size (mm at `dpi`).
    static bool writeEmbeddedSvg(const QImage& img, int dpi, const QString& fileName);
    /// Any other item (a chart, a texture page): grabbed at the export DPI and
    /// saved with the DPI in the file. For the windows that used a fixed 2x.
    Q_INVOKABLE void saveItemImage(QQuickItem* item, const QString& fileName);

    Q_INVOKABLE void setLegendVertical(bool v) { if (m_legendVertical != v) { m_legendVertical = v; emit legendVerticalChanged(); } }
    bool legendVertical() const { return m_legendVertical; }

    Q_INVOKABLE void setAutoCrop(bool v) { if (m_autoCrop != v) { m_autoCrop = v; emit autoCropChanged(); } }
    bool autoCrop() const { return m_autoCrop; }

    Q_INVOKABLE void setHighDpi(bool v) { if (m_highDpi != v) { m_highDpi = v; emit highDpiChanged(); } }
    bool highDpi() const { return m_highDpi; }

    Q_INVOKABLE void setDpi(int v) { if (m_dpi != v) { m_dpi = v; emit dpiChanged(); } }
    int dpi() const { return m_dpi; }

    int resolution() const;
    Q_INVOKABLE void setResolution(int v);

    bool pdfVectorModel() const { return m_pdfVectorModel; }
    int pdfModel() const { return m_pdfVectorModel ? 1 : 0; }
    Q_INVOKABLE void setPdfModel(int v) { setPdfVectorModel(v == 1); }
    Q_INVOKABLE void setPdfVectorModel(bool v) { if (m_pdfVectorModel != v) { m_pdfVectorModel = v; emit pdfVectorModelChanged(); } }

    double widthCm() const { return m_widthCm; }
    Q_INVOKABLE void setWidthCm(double v) { v = std::clamp(v, 0.0, 100.0); if (m_widthCm != v) { m_widthCm = v; emit widthCmChanged(); } }

    Q_INVOKABLE void setBackground(int v) { v = std::clamp(v, 0, 3); if (m_background != v) { m_background = v; emit backgroundChanged(); } }
    int background() const { return m_background; }

    Q_INVOKABLE void setOverlayStyle(int v) { v = std::clamp(v, 0, 2); if (m_overlayStyle != v) { m_overlayStyle = v; emit overlayStyleChanged(); } }
    int overlayStyle() const { return m_overlayStyle; }

    /// Put an image on the clipboard as PNG (with its alpha) and as a plain
    /// image: some applications on Linux only read image/png, others only the
    /// platform bitmap, which carries no alpha.
    static void putImageOnClipboard(const QImage& img);


    /// What --nogui --screenshot asks for.
    struct HeadlessShot
    {
        QString file;                  ///< .png or .pdf
        int     dpi = 300;
        int     background = int(Background::White);
        double  widthCm = 0.0;
        QSize   viewSize = QSize(1200, 900);
        int     outlines = 0;          ///< 0 off, 1 voxel edges, 2 boundaries
        bool    pdfVector = false;
    };
    /// Render the current structure (Parameters::voxels) without a window
    /// and write it through the same pipeline as Save. False with `error`
    /// set when no GL context can be made or the file cannot be written.
    static bool headlessScreenshot(const HeadlessShot& shot, QString* error = nullptr);

    static bool writeVoxelsCSV(const QString& path, int32_t*** voxels,
                               int numCubes, QString* error = nullptr);

    /**
     * @brief Headless geometry-only HDF5 export: the voxel grid plus the
     *        geometry metadata under the next /<last_set>/ group, the same
     *        layout the stress solvers write, minus the fields. Used by
     *        --nogui --output when no stress calculation is requested, so a
     *        dataset run that only needs the structure (e.g. for an external
     *        nonlinear solver) still gets a file.
     */
    static bool writeGeometryHDF5(const QString& path, int32_t*** voxels, int size, QString* error = nullptr);

    static void overlayColorBar(QImage& img, bool vertical = false, QQuickItem* item = nullptr);
    /// Crop, lay out legend and gizmo, compose. Returns ARGB32_Premultiplied
    /// for a transparent background and RGB32 otherwise, with the DPI set.
    static QImage processScreenshot(const QImage& rawImg, const ScreenshotOptions& options,
                                    QQuickItem* item = nullptr);

    /// A screenshot before it is painted: the model image and where it, the
    /// legend card and the axis triad go. processScreenshot() paints it into
    /// an image, writePdf() onto a PDF page (legend and triad as vectors).
    struct Composition
    {
        QSize   size;                       ///< the figure, pixels at `dpi`
        QColor  background;                 ///< alpha 0 when transparent
        bool    transparent = false;
        int     dpi = 96;
        QImage  model;                      ///< cropped (and resampled) scene
        QPoint  modelPos;
        /// Scene pixel p lands at (p - cropRect.topLeft()) * resample + modelPos.
        QSize   sceneSize;
        QRect   cropRect;
        qreal   resample = 1.0;
        qreal   lineScale = 1.0;            ///< figure pixels per logical view pixel
        bool    hasLegend = false;
        double  vmin = 0.0, vmax = 0.0;
        QString legendName;
        QVector<QColor> legendColors;
        QRectF  legendRect;
        bool    verticalLegend = false;
        bool    translucentLegend = false;
        QPointF gizmoCenter;
        qreal   overlayScale = 1.0;         ///< DPI / 96 x the options' overlayScale
        bool    darkOverlay = true;
        QPointer<QQuickItem> item;
    };
    static Composition layoutScreenshot(const QImage& rawImg, const ScreenshotOptions& options,
                                        QQuickItem* item = nullptr);
    /// Legend card and axis triad, as vector drawing on any QPainter.
    static void paintOverlays(QPainter& p, const Composition& c);
    /// One page of the figure's physical size (pixels at its DPI); the model
    /// as the image, or as `faces` (scene pixels, see Composition) when given.
    static bool writePdf(const Composition& c, const QString& fileName,
                         const std::vector<struct PdfFace>* faces = nullptr);

    Q_INVOKABLE void exportToCSV();
    Q_INVOKABLE void exportToVRML();
    Q_INVOKABLE void exportToHDF5();

    Q_INVOKABLE void openHDF5();

signals:
    void exportFinished(const QString& filePath);
    void exportFailed(const QString& message);
    void legendVerticalChanged();
    void autoCropChanged();
    void highDpiChanged();
    void dpiChanged();
    void widthCmChanged();
    void pdfVectorModelChanged();
    void backgroundChanged();
    void overlayStyleChanged();

private:
    /// Raw scene image at the export resolution. `offscreen` says whether it
    /// came from the off-screen render (no corner triad in it, background as
    /// asked) or from grabToImage() (on-screen look, rescaled).
    using SceneCallback = std::function<void(QImage img, bool offscreen)>;
    /// `background` must be resolved (not Auto).
    void captureScene(QQuickItem* item, Background background, SceneCallback done);
    /// The options of the current settings; `forClipboard` picks Auto's meaning.
    ScreenshotOptions currentOptions(QQuickItem* item, bool forClipboard, bool verticalLegend,
                                     bool fieldActive) const;
    /// The DPI an export is made at: `dpi` with High DPI on, else the screen's.
    int effectiveDpi(QQuickItem* item) const;
    void grabScene(QQuickItem* item, SceneCallback done);
    QSize exportPixelSize(QQuickItem* item) const;

    /// matviz3d-ddMMyyyy-HHmmss.<ext>, in the folder of the last screenshot saved.
    static QString defaultScreenshotPath(const QString& ext);
    static void rememberScreenshotDir(const QString& fileName);
    bool fetchVoxels(int32_t***& voxelsOut, int& numCubesOut);
    bool m_legendVertical = false;
    bool m_autoCrop       = true;
    bool m_highDpi        = true;
    int  m_dpi            = 300;
    double m_widthCm      = 0.0;
    bool   m_pdfVectorModel = false;
    int  m_background     = int(Background::Auto);
    int  m_overlayStyle   = int(OverlayStyle::Auto);
};

#endif // EXPORTCONTROLLER_H

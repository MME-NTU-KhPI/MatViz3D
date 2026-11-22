#ifndef EXPORTCONTROLLER_H
#define EXPORTCONTROLLER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <functional>
#include <algorithm>

class QQuickItem;

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
    };

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
    Q_INVOKABLE void saveAsSVG(QQuickItem* item);
    Q_INVOKABLE void saveAsVectorSVG(QQuickItem* item);  // true-vector, not raster

    Q_INVOKABLE void setLegendVertical(bool v) { if (m_legendVertical != v) { m_legendVertical = v; emit legendVerticalChanged(); } }
    bool legendVertical() const { return m_legendVertical; }

    Q_INVOKABLE void setAutoCrop(bool v) { if (m_autoCrop != v) { m_autoCrop = v; emit autoCropChanged(); } }
    bool autoCrop() const { return m_autoCrop; }

    Q_INVOKABLE void setHighDpi(bool v) { if (m_highDpi != v) { m_highDpi = v; emit highDpiChanged(); } }
    bool highDpi() const { return m_highDpi; }

    Q_INVOKABLE void setDpi(int v) { if (m_dpi != v) { m_dpi = v; emit dpiChanged(); } }
    int dpi() const { return m_dpi; }

    Q_INVOKABLE void setBackground(int v) { v = std::clamp(v, 0, 3); if (m_background != v) { m_background = v; emit backgroundChanged(); } }
    int background() const { return m_background; }

    Q_INVOKABLE void setOverlayStyle(int v) { v = std::clamp(v, 0, 2); if (m_overlayStyle != v) { m_overlayStyle = v; emit overlayStyleChanged(); } }
    int overlayStyle() const { return m_overlayStyle; }

    /// Put an image on the clipboard as PNG (with its alpha) and as a plain
    /// image: some applications on Linux only read image/png, others only the
    /// platform bitmap, which carries no alpha.
    static void putImageOnClipboard(const QImage& img);


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
    ScreenshotOptions currentOptions(bool forClipboard, bool verticalLegend, bool fieldActive) const;
    void grabScene(QQuickItem* item, SceneCallback done);
    QSize exportPixelSize(QQuickItem* item) const;

    bool fetchVoxels(int32_t***& voxelsOut, int& numCubesOut);
    bool m_legendVertical = false;
    bool m_autoCrop       = true;
    bool m_highDpi        = true;
    int  m_dpi            = 300;
    int  m_background     = int(Background::Auto);
    int  m_overlayStyle   = int(OverlayStyle::Auto);
};

#endif // EXPORTCONTROLLER_H

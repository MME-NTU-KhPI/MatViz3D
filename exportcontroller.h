#ifndef EXPORTCONTROLLER_H
#define EXPORTCONTROLLER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <functional>

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

public:
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
    static QImage processScreenshot(const QImage& rawImg, bool replaceWhiteBg, bool autoCrop,
                                    bool verticalLegend, int targetDpi = 300, QQuickItem* item = nullptr,
                                    bool fieldActive = false, bool sceneHasCornerAxes = true);

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

private:
    enum class SceneBackground { Theme, White, Transparent };
    /// Raw scene image at the export resolution. `offscreen` says whether it
    /// came from the off-screen render (no corner triad in it, background as
    /// asked) or from grabToImage() (on-screen look, rescaled).
    using SceneCallback = std::function<void(QImage img, bool offscreen)>;
    void captureScene(QQuickItem* item, SceneBackground background, SceneCallback done);
    void grabScene(QQuickItem* item, SceneCallback done);
    QSize exportPixelSize(QQuickItem* item) const;

    bool fetchVoxels(int32_t***& voxelsOut, int& numCubesOut);
    bool m_legendVertical = false;
    bool m_autoCrop       = true;
    bool m_highDpi        = true;
    int  m_dpi            = 300;
};

#endif // EXPORTCONTROLLER_H

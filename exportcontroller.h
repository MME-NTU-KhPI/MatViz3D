#ifndef EXPORTCONTROLLER_H
#define EXPORTCONTROLLER_H

#include <QObject>
#include <QString>

class QQuickItem;

/**
 * @brief Scene export: PNG, clipboard, SVG, CSV, VRML.
 *
 * Raster capture is performed via QQuickItem::grabToImage()—the only
 * correct method in Qt Quick: the renderer runs on a separate thread,
 * and directly reading the FBO from the GUI thread yields an empty frame.
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

    static void overlayColorBar(QImage& img, bool vertical = false, QQuickItem* item = nullptr);
    static QImage processScreenshot(const QImage& rawImg, bool replaceWhiteBg, bool autoCrop,
                                    bool verticalLegend, int targetDpi = 300, QQuickItem* item = nullptr,
                                    bool fieldActive = false);

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
    bool fetchVoxels(int32_t***& voxelsOut, int& numCubesOut);
    bool m_legendVertical = false;
    bool m_autoCrop       = true;
    bool m_highDpi        = true;
    int  m_dpi            = 300;
};

#endif // EXPORTCONTROLLER_H

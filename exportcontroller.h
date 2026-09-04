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

public:
    explicit ExportController(QObject* parent = nullptr);

    Q_INVOKABLE void saveAsImage(QQuickItem* item);
    Q_INVOKABLE void copyToClipboard(QQuickItem* item);
    Q_INVOKABLE void saveAsSVG(QQuickItem* item);
    Q_INVOKABLE void saveAsVectorSVG(QQuickItem* item);  // true-vector, not raster

    Q_INVOKABLE void exportToCSV();
    Q_INVOKABLE void exportToVRML();
    Q_INVOKABLE void exportToHDF5();

    Q_INVOKABLE void openHDF5();

signals:
    void exportFinished(const QString& filePath);
    void exportFailed(const QString& message);

private:
    bool fetchVoxels(int32_t***& voxelsOut, int& numCubesOut);
};

#endif // EXPORTCONTROLLER_H

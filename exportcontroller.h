#ifndef EXPORTCONTROLLER_H
#define EXPORTCONTROLLER_H

#include <QObject>
#include <QString>
#include "hdf5wrapper.h"

class QQuickItem;

/**
 * @brief Экспорт сцены: PNG, буфер обмена, SVG, CSV, VRML.
 *
 * Растровый захват идёт через QQuickItem::grabToImage() — единственный
 * корректный способ в Qt Quick: рендерер живёт на отдельном потоке,
 * прямое чтение FBO из GUI-потока даёт пустой кадр.
 *
 * Экспорт данных (CSV, VRML) берёт воксели у рендерера напрямую.
 *
 * Вызовы из QML:
 *     exportController.saveAsImage(glWidget)
 *     exportController.exportToCSV()
 */
class ExportController : public QObject
{
    Q_OBJECT

public:
    explicit ExportController(QObject* parent = nullptr);

    // ── Растровый экспорт вида (нужен элемент сцены) ──
    Q_INVOKABLE void saveAsImage(QQuickItem* item);
    Q_INVOKABLE void copyToClipboard(QQuickItem* item);
    Q_INVOKABLE void saveAsSVG(QQuickItem* item);

    // ── Экспорт данных (воксели берутся у рендерера) ──
    Q_INVOKABLE void exportToCSV();
    Q_INVOKABLE void exportToVRML();
    Q_INVOKABLE void exportToHDF5();

    Q_INVOKABLE void openHDF5();

signals:
    void exportFinished(const QString& filePath);
    void exportFailed(const QString& message);

private:
    /// Общая проверка: есть ли сгенерированная структура
    bool fetchVoxels(int32_t***& voxelsOut, int& numCubesOut);
};

#endif // EXPORTCONTROLLER_H

#include "exportcontroller.h"
#include "openglwidgetqml.h"
#include "parameters.h"
#include "hdf5wrapper.h"
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
#include <cmath>

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
//  PNG — original scene colors
// ═══════════════════════════════════════════════════════════════════
void ExportController::saveAsImage(QQuickItem* item)
{
    if (!item) {
        emit exportFailed(tr("Scene item is not available"));
        return;
    }

    // Ask for the path *before* the capture—otherwise, the dialogue window will obscure the scene.
    QString fileName = QFileDialog::getSaveFileName(
        nullptr, tr("Save Image"), "",
        tr("PNG Images (*.png);;All Files (*.*)"));
    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".png", Qt::CaseInsensitive))
        fileName += ".png";

    // Asynchronous capture of a scene element
    auto grab = item->grabToImage();
    if (!grab) {
        emit exportFailed(tr("grabToImage() failed — item has no window"));
        return;
    }

    // `grab` is captured by value—it keeps the object alive until the `ready` signal.
    connect(grab.data(), &QQuickItemGrabResult::ready, this,
            [this, grab, fileName]()
            {
                const QImage img = grab->image();
                if (img.isNull()) {
                    emit exportFailed(tr("Captured image is empty"));
                    return;
                }
                if (!img.save(fileName)) {
                    emit exportFailed(tr("Failed to save image to ") + fileName);
                    return;
                }
                qDebug() << "Image saved:" << fileName << img.size();
                emit exportFinished(fileName);
            });
}

// ═══════════════════════════════════════════════════════════════════
//  Clipboard — background replaced with white
// ═══════════════════════════════════════════════════════════════════
void ExportController::copyToClipboard(QQuickItem* item)
{
    if (!item) {
        emit exportFailed(tr("Scene item is not available"));
        return;
    }

    auto grab = item->grabToImage();
    if (!grab) {
        emit exportFailed(tr("grabToImage() failed — item has no window"));
        return;
    }

    connect(grab.data(), &QQuickItemGrabResult::ready, this,
            [this, grab]()
            {
                const QImage img = makeWhiteBackground(grab->image());
                if (img.isNull()) {
                    emit exportFailed(tr("Captured image is empty"));
                    return;
                }
                QGuiApplication::clipboard()->setImage(img);
                qDebug() << "Screenshot copied to clipboard" << img.size();
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
                const QImage img = makeWhiteBackground(grab->image());
                if (img.isNull()) {
                    emit exportFailed(tr("Captured image is empty"));
                    return;
                }

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
    HDF5Wrapper hdf5Wrapper("hdf5_save.hdf");

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
        hdf5Wrapper.write(prefix, "voxels", Parameters::voxels, Parameters::instance()->getSize());
        hdf5Wrapper.write(prefix, "cubeSize", Parameters::instance()->getSize());
        hdf5Wrapper.write(prefix, "numPoints", Parameters::instance()->getPoints());
    }
}

void ExportController::openHDF5()
{
    // QString fileName = QFileDialog::getOpenFileName(this , "Choose MatViz3d HDF5 file" , "" ,"MV3D HDF5 (*.hdf5)");
    // if (fileName.length() == 0)
    // {
    //     qDebug() << "OpenHDF: no file selected";
    //     return;
    // }
    // LoadStepManager& lsm = LoadStepManager::getInstance();
    // if (lsm.LoadFromHDF5(fileName))
    // {
    //     ui->backgrAnim_2->show();

    //     ui->geom_ID->blockSignals(true);
    //     ui->geom_sub_ID->blockSignals(true);

    //     ui->geom_ID->clear();
    //     ui->geom_ID->addItems(lsm.getGeomSetList());

    //     ui->geom_sub_ID->clear();
    //     ui->geom_sub_ID->addItems(lsm.getGeomSetSubList());

    //     ui->geom_ID->blockSignals(false);
    //     ui->geom_sub_ID->blockSignals(false);

    //     auto cmap = ui->myGLWidget->getColorMap(9);
    //     int comp = ui->geom_ID->currentIndex();

    //     float maxv = lsm.getMaxVal(comp);
    //     float minv = lsm.getMinVal(comp);

    //     if (!this->scene)
    //     {
    //         qDebug() << "LegendView is not initialized!";
    //         return;
    //     }

    //     qDebug() << "Min/Max values:" << minv << maxv;
    //     qDebug() << "ColorMap size:" << cmap.size();

    //     if (minv == maxv) {
    //         minv -= 0.01f;
    //         maxv += 0.01f;
    //     }

    //     this->scene->setMinMax(minv, maxv);
    //     this->scene->setCmap(cmap);
    //     this->scene->draw();

    //     ui->LegendView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    //     ui->LegendView->viewport()->update();
    //     ui->LegendView->show();
    //     ui->myGLWidget->setNumColors(lsm.getNumPoints());
    //     ui->myGLWidget->setVoxels(lsm.getVoxelPtr(), lsm.getCubeSize());
    //     ui->myGLWidget->update();
    // }
}

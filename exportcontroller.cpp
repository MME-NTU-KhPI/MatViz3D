#include "exportcontroller.h"
#include "openglwidgetqml.h"
#include "parameters.h"
#include "hdf5wrapper.h"
#include "loadstepmanager.h"
#include "parent_algorithm.h"
#include "stressanalysiscontroller.h"
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

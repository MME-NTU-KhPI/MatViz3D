#include "mainwindowwrapper.h"
#include "parameters.h"
#include "openglwidgetqml.h"
#include <QDebug>

MainWindowWrapper::MainWindowWrapper(QObject *parent) : QObject(parent) {}

void MainWindowWrapper::startButton()
{
    const int depth = 3, rows = 3, cols = 3;

    // Dynamic allocation of 3D array
    int32_t*** array = new int32_t**[depth];
    for (int d = 0; d < depth; ++d) {
        array[d] = new int32_t*[rows];
        for (int r = 0; r < rows; ++r) {
            array[d][r] = new int32_t[cols];
        }
    }

    // Seed for random number generation
    std::srand(std::time(nullptr));

    // Fill the array with random values
    for (int d = 0; d < depth; ++d) {
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                array[d][r][c] = std::rand() % 4 + 1; // Random value between 0 and 99
            }
        }
    }

    // Get Parameters instance
    Parameters* params = Parameters::instance();

    qDebug() << "size " << params->getSize() << " points " << params->getPoints() << " Algorithm " << params->getAlgorithm();

    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        // ogl->setNumColors(4);
        // ogl->setVoxels(array, 3);

        ogl->setNumColors(params->getPoints());
        ogl->setVoxels(array, params->getSize());

        ogl->setIsometricView();
    }
}

void MainWindowWrapper::isometricViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setIsometricView();
    }
}

void MainWindowWrapper::dimetricViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setDimetricView();
    }
}

void MainWindowWrapper::frontViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setFrontView();
    }
}

void MainWindowWrapper::backViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setBackView();
    }
}

void MainWindowWrapper::topViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setTopView();
    }
}

void MainWindowWrapper::bottomViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setBottomView();
    }
}

void MainWindowWrapper::leftViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setLeftView();
    }
}

void MainWindowWrapper::rightViewButton()
{
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setRightView();
    }
}

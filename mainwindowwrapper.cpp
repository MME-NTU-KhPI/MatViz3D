#include "mainwindowwrapper.h"
#include "openglwidgetqml.h"
#include <QDebug>

MainWindowWrapper::MainWindowWrapper(QObject *parent) : QObject(parent) {}

void MainWindowWrapper::showMessage()
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

    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    if (ogl)
    {
        ogl->setNumColors(4);
        ogl->setVoxels(array, 3);
        ogl->setIsometricView();
    }
}

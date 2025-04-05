#include "mainwindowalgorithmhandler.h"
#include "algorithmfactory.h"
#include "openglwidgetqml.h"
#include "parameters.h"

#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <cmath>

MainWindowAlgorithmHandler::MainWindowAlgorithmHandler(QObject *parent) : QObject(parent) {}

bool MainWindowAlgorithmHandler::validateParameters()
{
    Parameters& params = *Parameters::instance();

    if (params.getSize() <= 0) {
        QMessageBox::warning(nullptr, "Warning!", "Entered cube size is less than or equal to zero!");
        return false;
    }

    if (params.getPoints() <= 0) {
        QMessageBox::warning(nullptr, "Warning!", "Invalid initial points value entered!\nThis will result in incorrect program operation!");
        return false;
    }

    if (params.getPoints() > std::pow(params.getSize(), 3)) {
        QMessageBox::warning(nullptr, "Warning!",
                             "The entered value of the initial points exceeds the number of points in the cube! "
                             "This may lead to incorrect operation of the programme. "
                             "Please make sure that the number of starting points does not exceed the volume of the cube!");
        return false;
    }

    return true;
}

void MainWindowAlgorithmHandler::runAlgorithm(const QString& algorithmName, bool isAnimation)
{
    clock_t start_time = clock();

    if (!validateParameters()) return;

    registerAlgorithms();

    Parameters& params = *Parameters::instance();
    auto algorithm = AlgorithmFactory::instance().createAlgorithm(algorithmName, params);
    if (!algorithm) {
        QMessageBox::warning(nullptr, "Error", "Unknown algorithm selected.");
        return;
    }

    setAlgorithmFlags(*algorithm);

    // if (isAnimation) startGifRecording();
    executeAlgorithm(*algorithm, algorithmName);
    // if (isAnimation) stopGifRecording();

    logExecutionTime(start_time);
    emit algorithmFinished();
}

void MainWindowAlgorithmHandler::logExecutionTime(clock_t start_time)
{
    clock_t end_time = clock();
    double elapsed_time = double(end_time - start_time) / CLOCKS_PER_SEC;
    qDebug() << "Total execution time: " << elapsed_time << " seconds";
}

void MainWindowAlgorithmHandler::executeAlgorithm(Parent_Algorithm& algorithm, const QString& algorithmName)
{
    Parameters& params = *Parameters::instance();
    OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
    Parameters::voxels = algorithm.Allocate_Memory();
    // algorithm.Initialization(isWaveGeneration);
    algorithm.Initialization(false);
    algorithm.setRemainingPoints(algorithm.getNumColors() - static_cast<int>(Parameters::wave_coefficient * algorithm.getNumColors()));

    auto updateScene = [&]() {
        ogl->setVoxels(algorithm.getVoxels(), algorithm.getNumCubes());
        ogl->DelayFrameUpdate();
        ogl->update();
        QApplication::processEvents();
    };

    updateScene();

    auto start = std::chrono::high_resolution_clock::now();

    while (!algorithm.getDone()) {
        algorithm.Next_Iteration(updateScene);
    }

    auto end = std::chrono::high_resolution_clock::now();
    qDebug() << "Algorithm execution time: " << std::chrono::duration<double>(end - start).count() << " seconds";

    updateScene();
    algorithm.CleanUp();
    qDebug() << algorithmName;
}

void MainWindowAlgorithmHandler::setAlgorithmFlags(Parent_Algorithm& algorithm)
{
    algorithm.setAnimation(false);
    algorithm.setWaveGeneration(false);

    // algorithm.setAnimation(isAnimation);
    // algorithm.setWaveGeneration(isWaveGeneration);
    algorithm.setPeriodicStructure(false);
}

// void MainWindowAlgorithmHandler::startGifRecording()
// {
//     if (isRecording) return;

//     isRecording = true;

//     if (gif) delete gif;
//     gif = new QGifImage(QSize(ui->myGLWidget->width(), ui->myGLWidget->height()));

//     gif->setDefaultDelay(100); // 10 FPS (100 мс)

//     connect(ui->myGLWidget, &QOpenGLWidget::frameSwapped, this, &MainWindow::captureFrame);

//     qDebug() << "GIF recording started!";
// }

// void MainWindowAlgorithmHandler::stopGifRecording()
// {
//     isRecording = false;

//     disconnect(ui->myGLWidget, &QOpenGLWidget::frameSwapped, this, &MainWindow::captureFrame);

//     if (gif) {
//         gif->save("animation.gif");
//         qDebug() << "Animation has been saved!";
//         delete gif;
//         gif = nullptr;
//     }
// }

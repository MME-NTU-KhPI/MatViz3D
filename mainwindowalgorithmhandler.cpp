#include "mainwindowalgorithmhandler.h"
#include "algorithmfactory.h"
#include "openglwidgetqml.h"
#include "parameters.h"
#include "texturelibrary.h"
#include <array>
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

    executeAlgorithm(*algorithm, algorithmName);

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

    // Assign orientations to each grain and send to renderer.
    // Mirrors ansysWrapper::createFEfromArray: same library, same seed, same
    // sampling order, so orientation index == voxel value == CS id minus 11.
    {
        const int nSeeds = params.getPoints();

        TextureLibrary lib(Parameters::instance()->getSeed());
        if (!Parameters::textureComponents.empty())
            lib.setComponents(Parameters::textureComponents);
        else
            lib.setMode(TextureLibrary::Mode::Random);

        std::vector<std::array<float,3>> orientations;
        orientations.reserve(static_cast<size_t>(nSeeds) + 1);

        for (int i = 0; i <= nSeeds; ++i) {   // index 0 == background CS, as in createLocalCS
            double eu[3] = {0.0, 0.0, 0.0};
            lib.sampleNext(eu, /*in_deg=*/true);
            orientations.push_back({
                static_cast<float>(eu[0]),
                static_cast<float>(eu[1]),
                static_cast<float>(eu[2])
            });
        }
        ogl->setGrainOrientations(orientations);
    }

    auto start = std::chrono::high_resolution_clock::now();

    if( params.getIsAnimation() )
    {
        while(!algorithm.getDone())
        {
            algorithm.Next_Iteration();
            updateScene();
        }
    }
    else
    {
        algorithm.Generate_To_End();
    }

    auto end = std::chrono::high_resolution_clock::now();
    qDebug() << "Algorithm execution time: " << std::chrono::duration<double>(end - start).count() << " seconds";

    updateScene();
    algorithm.CleanUp();
    qDebug() << algorithmName;
}

void MainWindowAlgorithmHandler::setAlgorithmFlags(Parent_Algorithm& algorithm)
{
    algorithm.setAnimation(Parameters::instance()->getIsAnimation());
    algorithm.setWaveGeneration(false);
    algorithm.setPeriodicStructure(false);
}

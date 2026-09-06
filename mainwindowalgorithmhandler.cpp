#include "mainwindowalgorithmhandler.h"
#include "algorithmfactory.h"
#include "openglwidgetqml.h"
#include "parameters.h"
#include "texturelibrary.h"
#include "stressresult.h"
#include "stressanalysis.h"
#include "stressanalysis_fft.h"
#include "hdf5wrapper.h"
#include <array>
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QElapsedTimer>
#include <cmath>

MainWindowAlgorithmHandler::MainWindowAlgorithmHandler(QObject *parent) : QObject(parent) {}

bool MainWindowAlgorithmHandler::validateParameters()
{
    Parameters& params = *Parameters::instance();

    if (params.getSize() <= 0) {
        if (OpenGLWidgetQML::getInstance())
            QMessageBox::warning(nullptr, "Warning!", "Entered cube size is less than or equal to zero!");
        else
            qCritical() << "Cube size must be greater than zero";
        return false;
    }

    if (params.getPoints() <= 0) {
        if (OpenGLWidgetQML::getInstance())
            QMessageBox::warning(nullptr, "Warning!", "Invalid initial points value entered!\nThis will result in incorrect program operation!");
        else
            qCritical() << "Number of initial points must be greater than zero";
        return false;
    }

    if (params.getPoints() > std::pow(params.getSize(), 3)) {
        if (OpenGLWidgetQML::getInstance())
            QMessageBox::warning(nullptr, "Warning!",
                                 "The entered value of the initial points exceeds the number of points in the cube! "
                                 "This may lead to incorrect operation of the programme. "
                                 "Please make sure that the number of starting points does not exceed the volume of the cube!");
        else
            qCritical() << "Initial points exceed the cube volume";
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
        if (OpenGLWidgetQML::getInstance())
            QMessageBox::warning(nullptr, "Error", "Unknown algorithm selected.");
        else
            qCritical() << "Unknown algorithm:" << algorithmName;
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

    // Drop any phase table a previous run published. Only a multi-phase
    // algorithm fills this in, and a stale one left over from, say, a Composite
    // run would otherwise be applied to the next structure's grains -- whose
    // ids mean something completely different.
    Parameters::phaseAssignment.clear();

    algorithm.Initialization(params.getIsWaveGeneration());

    const int totalColors = algorithm.getNumColors();
    if (totalColors > 0) {
        params.setPoints(totalColors);
        if (ogl) {
            ogl->setNumColors(totalColors);
        }
    }

    auto updateScene = [&]() {
        if (!ogl) return;
        ogl->setVoxels(algorithm.getVoxels(), algorithm.getNumCubes());
        ogl->DelayFrameUpdate();
        ogl->update();
        QApplication::processEvents();
    };


    // Assign orientations to each grain and send to renderer.
    // Multi-phase structures (Composite) publish their own per-grain orientations;
    // single-phase polycrystals sample from the TextureLibrary.
    if (ogl)
    {
        if (!Parameters::phaseAssignment.grainOrientation.empty())
        {
            const auto& po = Parameters::phaseAssignment.grainOrientation;
            std::vector<std::array<float,3>> orientations;
            orientations.reserve(po.size());
            const double r2d = 180.0 / M_PI;
            // po has 1-based indexing: index 1 is matrix, index 2..N are fibers.
            // ogl->setGrainOrientations expects index 0 = grain 1, etc.
            for (size_t g = 1; g < po.size(); ++g) {
                orientations.push_back({
                    static_cast<float>(po[g][0] * r2d),
                    static_cast<float>(po[g][1] * r2d),
                    static_cast<float>(po[g][2] * r2d)
                });
            }
            ogl->setGrainOrientations(orientations);
        }
        else
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
                // Bunge ZXZ (phi1,Phi,phi2), matching buildOrientationGlyphs()'s
                // bungeZXZ() reconstruction -- NOT the ANSYS Z-X-Y angles that
                // sampleNext() returns (those two conventions only agree at the
                // identity, which is why this only showed up on non-Cube textures).
                lib.sampleNextBunge(eu, /*in_deg=*/true);
                orientations.push_back({
                    static_cast<float>(eu[0]),
                    static_cast<float>(eu[1]),
                    static_cast<float>(eu[2])
                });
            }
            ogl->setGrainOrientations(orientations);
        }
    }

    updateScene(); // for work with autostart console parameter. idk how to fix that in other way

    auto start = std::chrono::high_resolution_clock::now();

    if( params.getIsAnimation() )
    {
        updateScene();
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
    const double elapsedSec = std::chrono::duration<double>(end - start).count();
    qInfo().noquote() << QString("[%1] Structure generation completed in %2 s").arg(algorithmName).arg(elapsedSec, 0, 'f', 4);

    algorithm.saveSeeds();

    updateScene();
    algorithm.CleanUp();
}

void MainWindowAlgorithmHandler::setAlgorithmFlags(Parent_Algorithm& algorithm)
{
    Parameters& params = *Parameters::instance();
    algorithm.setAnimation(params.getIsAnimation());
    algorithm.setWaveGeneration(params.getIsWaveGeneration());
    algorithm.setPeriodicStructure(params.getIsPeriodic());
}

void MainWindowAlgorithmHandler::runStressCalculation()
{
    if (!Parameters::voxels) {
        qCritical() << "No structure generated; nothing to solve";
        return;
    }

    Parameters* p = Parameters::instance();
    const short   numCubes  = static_cast<short>(p->getSize());
    const short   numPoints = static_cast<short>(p->getPoints());
    const QString solver    = p->getStressSolver();
    const bool    fft       = solver.compare("fft", Qt::CaseInsensitive) == 0;
    const double* eps       = p->getStressEps();

    if (p->getStressMode().compare("single", Qt::CaseInsensitive) == 0) {
        SingleShotResult r = fft
                                 ? StressAnalysisFFT().solveSingleLoadCase(numCubes, numPoints, Parameters::voxels, eps)
                                 : StressAnalysis().solveSingleLoadCase(numCubes, numPoints, Parameters::voxels, eps);
        if (!r.ok) {
            qCritical() << "Solve failed:" << r.errorMessage;
            return;
        }
        qInfo() << "macro stress (Pa): sx" << r.macro_stress[0] << " sy" << r.macro_stress[1]
                << " sz" << r.macro_stress[2] << " sxy" << r.macro_stress[3]
                << " syz" << r.macro_stress[4] << " sxz" << r.macro_stress[5];
        qInfo() << "von Mises:" << r.von_mises;
        if (fft) qInfo() << "iterations:" << r.iterations << " error:" << r.error;
        return;
    }

    if (p->getStressMode().compare("stiffness", Qt::CaseInsensitive) == 0) {
        // Quick-test S/C/P: 6 canonical unit-strain solves, no Hill calibration,
        // no 300-sample main run -- seconds instead of minutes for FFT, one
        // ANSYS batch instead of ~450 for ANSYS. Written into the same HDF5
        // schema (S_matrix/C_matrix/P_matrix/Effective_Moduli under an
        // auto-incrementing /<last_set>/ group) as dataset mode, so any
        // existing reader (e.g. h5py) doesn't need special-casing.
        StiffnessMatrixResult r = fft
            ? StressAnalysisFFT().computeStiffnessMatrix(numCubes, numPoints, Parameters::voxels)
            : StressAnalysis().computeStiffnessMatrix(numCubes, numPoints, Parameters::voxels);
        if (!r.ok) {
            qCritical() << "Stiffness matrix computation failed:" << r.errorMessage;
            return;
        }

        const QString filename = Parameters::filename.length() ? Parameters::filename : "current_ls.hdf5";
        const QString group = saveStiffnessMatrixToHDF5(filename, r, solver, Parameters::seed);
        if (group.isEmpty()) {
            qCritical() << "Failed to write the stiffness matrix to" << filename;
            return;
        }

        // (saveStiffnessMatrixToHDF5 already logs the file and group.)
        qInfo() << QString("Effective moduli (1/Sii) [Pa]: Ex=%1 Ey=%2 Ez=%3 Gxy=%4 Gyz=%5 Gxz=%6")
                        .arg(r.moduli[0], 0, 'e', 3).arg(r.moduli[1], 0, 'e', 3).arg(r.moduli[2], 0, 'e', 3)
                        .arg(r.moduli[3], 0, 'e', 3).arg(r.moduli[4], 0, 'e', 3).arg(r.moduli[5], 0, 'e', 3);
        return;
    }

    // dataset mode -- writes HDF5 itself
    qInfo() << "Results ->" << (Parameters::filename.length() ? Parameters::filename
                                                              : QStringLiteral("current_ls.hdf5"));
    if (fft) {
        StressAnalysisFFT sa;
        if (p->getNumRndLoads() > 0) sa.num_samples = p->getNumRndLoads();
        sa.estimateStressWithFFT(numCubes, numPoints, Parameters::voxels);
    } else {
        StressAnalysis sa;
        if (p->getNumRndLoads() > 0) sa.num_samples = p->getNumRndLoads();
        sa.estimateStressWithANSYS(numCubes, numPoints, Parameters::voxels);
    }
}

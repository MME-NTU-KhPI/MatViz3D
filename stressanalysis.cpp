#include "stressanalysis.h"

#include "ansyswrapper.h"
#include <Windows.h>

#include <QtWidgets>
#include <QMessageBox>
#include <QPushButton>
#include <QFileDialog>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>

void StressAnalysis::estimateStressWithANSYS(short int numCubes, int16_t ***voxels)
{
    const auto N = numCubes;
    ansysWrapper wr(true);
    wr.setNP(1);
    //wr.setMaterial(2.1e11, 0.3, 0);

    //wr.setMaterial(2.1e11, 0.3, 0);
    double c11 = 168.40e9, c12=121.40e9, c44=75.40e9;
    wr.setAnisoMaterial(c11, c12, c12, c11, c12, c11, c44, c44, c44) ;

    wr.setElemByNum(186);

    wr.createFEfromArray(voxels, N, N);

    wr.applyComplexLoads(0, 0, 0, N, N, N,
                         0.001, 0, 0,
                         0, 0, 0);
    wr.solveLS(1, 1);
    wr.saveAll();
    wr.run();
    wr.load_loadstep(1);
}

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
    wr.setMaterial(2.1e11, 0.3, 0);
    wr.setElemByNum(186);


   /* int16_t*** a = new int16_t**[N];

    for (int i = 0; i < N; i++) {

        // Allocate memory blocks for
        // rows of each 2D array
        a[i] = new int16_t*[N];

        for (int j = 0; j < N; j++) {

            // Allocate memory blocks for
            // columns of each 2D array
            a[i][j] = new int16_t[N];
        }
    }

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                a[i][j][k] = rand()%N;*/

    wr.createFEfromArray(voxels, N, N);
    // wr.applyTensBC(0, 0, 0, N, N, N, 0.1, 0, 0);
    // wr.applyTensBC(0, 0, 0, N, N, N, 0, 0.1, 0);
    // wr.applyTensBC(0, 0, 0, N, N, N, 0, 0, 0.1);
    // wr.applyTensBC(0, 0, 0, N, N, N, 0, -0.1, 0.1);


    //wr.applyTensBC(0, 0, 0, N, N, N, 0.1, 0, 0);
    //wr.applyTensBC(0, 0, 0, N, N, N, 0, 0.1, 0);
    //wr.applyTensBC(0, 0, 0, N, N, N, 0, 0, 0.1);
    // wr.applyTensBC(0, 0, 0, N, N, N, 0, -0.1, 0.1);
    // wr.applyPureShearBC(0, 0, 0, N, N, N, "XY", 0.1);
    // wr.applyPureShearBC(0, 0, 0, N, N, N, "XZ", 0.1);
    // wr.applyPureShearBC(0, 0, 0, N, N, N, "YZ", 0.1);


    wr.applyComplexLoads(0, 0, 0, N, N, N, 
                         0.001, 0.002, 0.003,
                         0.004, 0.005, 0.006);
    wr.solveLS(1, 1);
    wr.saveAll();
    wr.run();
}

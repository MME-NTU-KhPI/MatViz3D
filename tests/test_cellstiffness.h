#ifndef TEST_CELLSTIFFNESS_H
#define TEST_CELLSTIFFNESS_H

#include <QObject>

/// cellstiffness.h: the solve behind "asked / obtained" on the Neural tab. Small cells (8 to 12 voxels along
/// an edge), about 10 s in all (the cell with no solid path across it takes thousands of iterations).
class TestCellStiffness : public QObject
{
    Q_OBJECT

private slots:
    void testSolidCellGivesBackTheLaw();
    void testPorousCellEqualsADirectSolve();
    void testOccupancyKeepsTheAxes();
    void testSofterVoidWhenTheVoidCarriesTheLoad();
    void testStopping();
    void testCompare();
    void testGpuSolveEqualsTheCpuSolve();
};

#endif // TEST_CELLSTIFFNESS_H

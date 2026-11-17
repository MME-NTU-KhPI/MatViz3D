#ifndef TEST_CELLPREDICTOR_H
#define TEST_CELLPREDICTOR_H

#include <QObject>
#include "cellpredictor.h"

/// The property predictor of the Neural tab (cellpredictor.h) on the weights
/// file models/cell_predictor.bin (MATVIZ_CELL_PREDICTOR names another one).
/// The file is 12 MB and not in git: without it the tests that need it are
/// skipped, and only the refusal of broken files is checked.
class TestCellPredictor : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testLoadReadsTheHeader();
    void testEncoderMatchesPyTorch();
    void testPropertiesMatchPyTorch();
    void testNoisedCodeMatchesPyTorch();
    void testShiftedCellGivesTheSameProperties();
    void testWrongSizesGiveNothing();
    void testBrokenFilesAreRefused();

private:
    CellPredictor m_net;
    QString m_file;
};

#endif // TEST_CELLPREDICTOR_H

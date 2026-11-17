#ifndef TEST_CELLDIFFUSION_H
#define TEST_CELLDIFFUSION_H

#include <QObject>
#include "celldiffusion.h"

/// The diffusion network of the Neural tab (celldiffusion.h) on the weights
/// file models/cell_diffusion.bin (MATVIZ_CELL_DIFFUSION names another one).
/// The file is about 100 MB and not in git: without it the tests that need it are
/// skipped, and only the refusal of broken files is checked.
class TestCellDiffusion : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testOpenReadsTheHeader();
    void testRequestIsBuiltAsInTraining();
    void testRequestNotesAndRanges();
    void testMatchesPyTorchOnTheReferenceInput();
    void testShortSamplingMatchesPyTorch();
    void testStartNoiseIsReproducible();
    void testShortSamplingStopsAndRepeats();
    void testBrokenFilesAreRefused();

private:
    CellDiffusion m_net;
    QString m_file;
};

#endif // TEST_CELLDIFFUSION_H

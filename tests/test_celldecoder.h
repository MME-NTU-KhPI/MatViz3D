#ifndef TEST_CELLDECODER_H
#define TEST_CELLDECODER_H

#include <QObject>

/// The decoder of the cell autoencoder (celldecoder.h) with the weights the
/// program ships (models/cell_decoder.bin, embedded through models.qrc).
class TestCellDecoder : public QObject
{
    Q_OBJECT

private slots:
    void testLoadReadsTheHeader();
    void testMatchesPyTorchOnTheReferenceCode();
    void testShiftedCodeGivesTheShiftedCell();
    void testRandomCodeIsReproducibleWithTheStoredStatistics();
    void testBrokenWeightsAreRefused();
    void testKeepLargestBodyIsPeriodic();
};

#endif // TEST_CELLDECODER_H

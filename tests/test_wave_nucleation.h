#ifndef TEST_WAVE_NUCLEATION_H
#define TEST_WAVE_NUCLEATION_H

#include <QObject>

class TestWaveNucleation : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testWaveOffAllSeedsAtOnce();
    void testWaveBatchFormulaPolycrystall();
    void testSurfaceModeSkipsWave();
    void testProbabilityWaveBatchesProgressive();
};

#endif // TEST_WAVE_NUCLEATION_H

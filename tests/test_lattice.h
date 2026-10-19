#ifndef TEST_LATTICE_H
#define TEST_LATTICE_H

#include <QObject>

class TestLattice : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testFamilyParsing();
    void testDensityIsHitForEveryFamily();
    void testFieldIsPeriodicInTheCube();
    void testLayerRevealMatchesOneShot();
    void testGradingThickensTowardsTop();
    void testSeedReproducesSpinodoid();
    void testSpinodoidConesMakeLamellae();
    void testFourierFileRoundTrip();
    void testFourierRandomIsReproducibleAndPublished();
    void testSmoothedNoiseIsReproducibleAndSmoothingConnects();
    void testMaterialOverrideAndTpmsWaves();
    void testPlateLatticesSymmetryAndUnion();
    void testSitesCarvingAndSmoothing();
    void testCliOptions();
};

#endif // TEST_LATTICE_H

#ifndef TEST_TEXTUREMATH_H
#define TEST_TEXTUREMATH_H

#include <QObject>

class TestTextureMath : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testCrystalToSampleIdentity();
    void testCrystalToSampleOrthogonality();
    void testStereographicKnownPoles();
    void testFamilyDirections();
    void testPhiWeightIntegral();
    void testMarchingSquaresContour();
};

#endif // TEST_TEXTUREMATH_H

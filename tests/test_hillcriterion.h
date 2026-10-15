#ifndef TEST_HILLCRITERION_H
#define TEST_HILLCRITERION_H

#include <QObject>
#include <array>
#include <vector>

class TestHillCriterion : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testDeviatoricConversions6Dto5D();
    void testResolvedShearStress();
    void testFitEllipsoidKnownInput();
    void testFitEllipsoidFewerThan15PointsRejection();

    void testVonMisesUniaxialAndShear();
    void testFitFull6DRecoversKnownEllipsoid();
    void testFitFull6DFallbackDeviatoric();
    void testFitFull6DFallbackNearlyPressureInsensitive();
    void testGenerateLoadsDefaultEqualsExplicitSchmid5D();

private:
    // Points on the 5D deviatoric sphere of radius R (random directions)
    static std::vector<std::array<double, 6>> deviatoricSpherePoints(int n, double R, unsigned seed);
};

#endif // TEST_HILLCRITERION_H

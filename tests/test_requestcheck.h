#ifndef TEST_REQUESTCHECK_H
#define TEST_REQUESTCHECK_H

#include <QObject>

/// requestcheck.h: the conflicts between the target properties of the Neural tab (no data, microseconds).
class TestRequestCheck : public QObject
{
    Q_OBJECT

private slots:
    void testFeasibleRequestsPass();
    void testVoigtBound();
    void testModuliTogether();
    void testSameInAllDirections();
    void testVoigtFollowsTheLayers();
    void testHashinShtrikmanForAnIsotropicCell();
    void testPoissonRatiosMustGiveAPositiveCompliance();
    void testIsotropicCellTiesGToEAndNu();
    void testRatiosAgainstTheAxes();
    void testCubicCellFixesZenerAndEmaxEmin();
    void testBulkModulusFollowsEAndNu();
};

#endif // TEST_REQUESTCHECK_H

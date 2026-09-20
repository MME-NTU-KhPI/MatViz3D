#ifndef TEST_HILLCRITERION_H
#define TEST_HILLCRITERION_H

#include <QObject>

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
};

#endif // TEST_HILLCRITERION_H

#ifndef TEST_PROPERTYSEARCH_H
#define TEST_PROPERTYSEARCH_H

#include <QObject>

class TestPropertySearch : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void testLawStep();
    void testPredictionScalesExactly();
    void testPredictionAgainstASolve();
    void testReadCellConvertsTheShearBasis();
    void testSearchRanksAndFilters();
    void testAnisotropyMeasures();
    void testMoreMeasures();
    void testSearchByAnisotropy();
    void testControllerOnADatasetFolder();
};

#endif // TEST_PROPERTYSEARCH_H

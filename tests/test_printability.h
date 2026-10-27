#ifndef TEST_PRINTABILITY_H
#define TEST_PRINTABILITY_H

#include <QObject>

class TestPrintability : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void testIslandsAreCounted();
    void testSeparateBodiesFail();
    void testBodyMustJoinInThreeDirections();
    void testPeriodicWrapJoinsAcrossFaces();
    void testThinWallsAreDetected();
    void testOverhangPicksTheBuildDirection();
    void testClosedPoresAreReported();
    void testGyroidPassesAndReportIsPublished();
    void testUniformWallMakesRandomSheetsPrintable();
    void testCliOptions();
};

#endif // TEST_PRINTABILITY_H

#ifndef TEST_POLYCRYSTALL_GROWTH_H
#define TEST_POLYCRYSTALL_GROWTH_H

#include <QObject>

class TestPolycrystallGrowth : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testNeighborhoodOffsetsSingleStep();
    void testDeterministicFullGrowth();
    void testPeriodicBoundaryGrowth();
};

#endif // TEST_POLYCRYSTALL_GROWTH_H

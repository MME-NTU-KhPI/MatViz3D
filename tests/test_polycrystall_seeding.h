#ifndef TEST_POLYCRYSTALL_SEEDING_H
#define TEST_POLYCRYSTALL_SEEDING_H

#include <QObject>

class TestPolycrystallSeeding : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testVolumeSeedingDistribution();
    void testVolumeSeedingCapacity();
    void testSurfaceSeedingSixFaces();
    void testSurfaceSeedingPlaneCapacity();
};

#endif // TEST_POLYCRYSTALL_SEEDING_H

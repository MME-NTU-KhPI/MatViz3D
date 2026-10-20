#ifndef TEST_SEED_DISTRIBUTION_H
#define TEST_SEED_DISTRIBUTION_H

#include <QObject>

class TestSeedDistribution : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testDefaults();
    void testRandomSeedsUnchanged_data();
    void testRandomSeedsUnchanged();
    void testSeedVoxelsUniqueAndDeterministic();
    void testSobolAlgorithms_data();
    void testSobolAlgorithms();
    void testSobolLowersGrainSizeScatter();
};

#endif // TEST_SEED_DISTRIBUTION_H

#ifndef TEST_DLCA_H
#define TEST_DLCA_H

#include <QObject>

class TestDlca : public QObject
{
    Q_OBJECT

private slots:
    void testCollision_data();
    void testCollision();
    void testFaceContactAcrossBoundaryJoins();

    void testWrapRank_data();
    void testWrapRank();

    void testGeneratedClusterReportsRank();
    void testSingleParticleWarns();
    void testDenseClusterDoesNotWarn();

    void testParticlesKeepDistinctVoxels_data();
    void testParticlesKeepDistinctVoxels();

    void testRankAttributeInGeometry();
    void testNoRankAttributeWhenNotComputed();
};

#endif // TEST_DLCA_H

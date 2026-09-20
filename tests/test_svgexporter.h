#ifndef TEST_SVGEXPORTER_H
#define TEST_SVGEXPORTER_H

#include <QObject>

class TestSvgExporter : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testProjectKnownMVPIdentity();
    void testProjectDropsNonPositiveW();
    void testProjectPerspectiveCamera();
    void testProjectOverloadsConsistency();
    void testShadeFaceAndFacesFromQuad();
};

#endif // TEST_SVGEXPORTER_H

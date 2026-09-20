#include "test_svgexporter.h"
#include <QTest>
#include <cmath>
#include "svgexporter.hpp"

void TestSvgExporter::init()
{
}

void TestSvgExporter::cleanup()
{
}

void TestSvgExporter::testProjectKnownMVPIdentity()
{
    QMatrix4x4 mvp;
    mvp.setToIdentity();

    const int width = 1000;
    const int height = 800;
    const float eps = 1e-4f;

    // 1. Origin (0, 0, 0) -> Screen center (500, 400)
    {
        QVector3D p(0.0f, 0.0f, 0.0f);
        QPointF out;
        float zOut = 0.0f;
        bool ok = svgx::project(mvp, p, width, height, out, zOut);

        QVERIFY(ok);
        QVERIFY(std::abs(out.x() - 500.0) < eps);
        QVERIFY(std::abs(out.y() - 400.0) < eps);
        QVERIFY(std::abs(zOut - 0.0f) < eps);
    }

    // 2. Top-right NDC (+1, +1, 0.5) -> (1000, 800)
    {
        QVector3D p(1.0f, 1.0f, 0.5f);
        QPointF out;
        float zOut = 0.0f;
        bool ok = svgx::project(mvp, p, width, height, out, zOut);

        QVERIFY(ok);
        QVERIFY(std::abs(out.x() - 1000.0) < eps);
        QVERIFY(std::abs(out.y() - 800.0) < eps);
        QVERIFY(std::abs(zOut - 0.5f) < eps);
    }

    // 3. Bottom-left NDC (-1, -1, -0.5) -> (0, 0)
    {
        QVector3D p(-1.0f, -1.0f, -0.5f);
        QPointF out;
        float zOut = 0.0f;
        bool ok = svgx::project(mvp, p, width, height, out, zOut);

        QVERIFY(ok);
        QVERIFY(std::abs(out.x() - 0.0) < eps);
        QVERIFY(std::abs(out.y() - 0.0) < eps);
        QVERIFY(std::abs(zOut - (-0.5f)) < eps);
    }
}

void TestSvgExporter::testProjectDropsNonPositiveW()
{
    // Construct an MVP matrix where clip.w() <= 0
    QMatrix4x4 mvp;
    mvp.setToIdentity();

    // Set the 4th row so that clip.w = -1.0 * p.z
    // [ 1 0 0  0 ]
    // [ 0 1 0  0 ]
    // [ 0 0 1  0 ]
    // [ 0 0 -1 0 ]
    mvp.setRow(3, QVector4D(0.0f, 0.0f, -1.0f, 0.0f));

    QPointF out;
    float zOut = 0.0f;

    // For p.z = 1.0, clip.w = -1.0 <= 0 -> MUST BE DROPPED
    {
        QVector3D p(0.0f, 0.0f, 1.0f);
        bool ok = svgx::project(mvp, p, 800, 600, out, zOut);
        QCOMPARE(ok, false);
    }

    // For p.z = 0.0, clip.w = 0.0 <= 0 -> MUST BE DROPPED
    {
        QVector3D p(0.0f, 0.0f, 0.0f);
        bool ok = svgx::project(mvp, p, 800, 600, out, zOut);
        QCOMPARE(ok, false);
    }

    // For p.z = -1.0, clip.w = +1.0 > 0 -> ACCEPTED
    {
        QVector3D p(0.0f, 0.0f, -1.0f);
        bool ok = svgx::project(mvp, p, 800, 600, out, zOut);
        QCOMPARE(ok, true);
    }
}

void TestSvgExporter::testProjectPerspectiveCamera()
{
    // Perspective projection looking down -Z
    QMatrix4x4 proj;
    proj.perspective(60.0f, 1.0f, 0.1f, 100.0f);

    QMatrix4x4 view;
    // Eye at (0, 0, 5), looking at (0, 0, 0), up is (0, 1, 0)
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f),
                QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f));

    QMatrix4x4 mvp = proj * view;

    QPointF out;
    float zOut = 0.0f;

    // Point in front of camera at (0, 0, 0): should project to center (400, 400)
    bool okInFront = svgx::project(mvp, QVector3D(0.0f, 0.0f, 0.0f), 800, 800, out, zOut);
    QVERIFY(okInFront);
    QVERIFY(std::abs(out.x() - 400.0) < 1.0);
    QVERIFY(std::abs(out.y() - 400.0) < 1.0);

    // Point behind camera at (0, 0, 10.0): should be dropped (w <= 0)
    bool okBehind = svgx::project(mvp, QVector3D(0.0f, 0.0f, 10.0f), 800, 800, out, zOut);
    QCOMPARE(okBehind, false);
}

void TestSvgExporter::testProjectOverloadsConsistency()
{
    QMatrix4x4 mvp;
    mvp.setToIdentity();

    GlVertex g;
    g.x = 0.2f; g.y = -0.4f; g.z = 0.1f;
    g.r = 255;  g.g = 0;     g.b = 0; g.a = 255;
    g.nx = 0;   g.ny = 127;  g.nz = 0;

    QVector3D v(0.2f, -0.4f, 0.1f);

    QPointF outG, outV;
    float zG = 0.0f, zV = 0.0f;

    bool okG = svgx::project(mvp, g, 1024, 768, outG, zG);
    bool okV = svgx::project(mvp, v, 1024, 768, outV, zV);

    QCOMPARE(okG, okV);
    QVERIFY(std::abs(outG.x() - outV.x()) < 1e-4);
    QVERIFY(std::abs(outG.y() - outV.y()) < 1e-4);
    QVERIFY(std::abs(zG - zV) < 1e-4);
}

void TestSvgExporter::testShadeFaceAndFacesFromQuad()
{
    GlVertex v;
    v.r = 200; v.g = 100; v.b = 50; v.a = 255;
    v.nx = 0;  v.ny = 127; v.nz = 0; // facing Y

    QRgb color = svgx::shadeFace(v);
    QVERIFY(qAlpha(color) == 255);
    QVERIFY(qRed(color) > 0 && qRed(color) <= 255);
    QVERIFY(qGreen(color) > 0 && qGreen(color) <= 255);
    QVERIFY(qBlue(color) > 0 && qBlue(color) <= 255);

    // Quad buffer
    std::vector<GlVertex> buf(8, v);
    auto faces = svgx::facesFromQuadBuffer(buf);
    QCOMPARE(faces.size(), static_cast<size_t>(2));
    QCOMPARE(faces[0].v.size(), static_cast<size_t>(4));
    QCOMPARE(faces[1].v.size(), static_cast<size_t>(4));
}

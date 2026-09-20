#include "test_texturemath.h"
#include <QTest>
#include <cmath>
#include "texturemath.hpp"

void TestTextureMath::init()
{
}

void TestTextureMath::cleanup()
{
}

void TestTextureMath::testCrystalToSampleIdentity()
{
    texmath::Bunge b0 = {0.0, 0.0, 0.0};
    texmath::Mat3 a = texmath::crystalToSample(b0);

    const double eps = 1e-7;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double expected = (i == j) ? 1.0 : 0.0;
            QVERIFY2(std::abs(a[i][j] - expected) < eps,
                     qPrintable(QString("Identity mismatch at [%1][%2]: got %3, expected %4")
                                    .arg(i).arg(j).arg(a[i][j]).arg(expected)));
        }
    }
}

void TestTextureMath::testCrystalToSampleOrthogonality()
{
    texmath::Bunge b = {35.0, 45.0, 60.0};
    texmath::Mat3 a = texmath::crystalToSample(b);

    const double eps = 1e-7;

    // Check R * R^T = I
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double dot = 0.0;
            for (int k = 0; k < 3; ++k) {
                dot += a[i][k] * a[j][k];
            }
            double expected = (i == j) ? 1.0 : 0.0;
            QVERIFY2(std::abs(dot - expected) < eps,
                     qPrintable(QString("Orthogonality failed at [%1][%2]: got %3, expected %4")
                                    .arg(i).arg(j).arg(dot).arg(expected)));
        }
    }

    // Check determinant = +1
    double det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
               - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
               + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);

    QVERIFY2(std::abs(det - 1.0) < eps,
             qPrintable(QString("Rotation matrix determinant != 1: got %1").arg(det)));
}

void TestTextureMath::testStereographicKnownPoles()
{
    const float eps = 1e-5f;

    // 1. North pole (0, 0, 1) -> center of disc (0, 0)
    {
        auto p = texmath::stereographic(0.0, 0.0, 1.0);
        QVERIFY(std::abs(p[0]) < eps);
        QVERIFY(std::abs(p[1]) < eps);
    }

    // 2. Equator (1, 0, 0): screen_x = dy=0, screen_y = -dx = -1
    {
        auto p = texmath::stereographic(1.0, 0.0, 0.0);
        QVERIFY(std::abs(p[0] - 0.0f) < eps);
        QVERIFY(std::abs(p[1] - (-1.0f)) < eps);
    }

    // 3. Equator (0, 1, 0): screen_x = dy=1, screen_y = -dx = 0
    {
        auto p = texmath::stereographic(0.0, 1.0, 0.0);
        QVERIFY(std::abs(p[0] - 1.0f) < eps);
        QVERIFY(std::abs(p[1] - 0.0f) < eps);
    }

    // 4. Lower hemisphere (0, 0, -1) folded to (0, 0, 1) -> (0, 0)
    {
        auto p = texmath::stereographic(0.0, 0.0, -1.0);
        QVERIFY(std::abs(p[0]) < eps);
        QVERIFY(std::abs(p[1]) < eps);
    }

    // Radius <= 1.0 for arbitrary unit direction
    double v[3] = { 0.57735, 0.57735, 0.57735 };
    auto p = texmath::stereographic(v[0], v[1], v[2]);
    float radius = std::sqrt(p[0] * p[0] + p[1] * p[1]);
    QVERIFY2(radius <= 1.0f + eps, "Stereographic projection point fell outside unit disc");
}

void TestTextureMath::testFamilyDirections()
{
    const auto& d100 = texmath::familyDirections(texmath::PoleFamily::F100);
    const auto& d110 = texmath::familyDirections(texmath::PoleFamily::F110);
    const auto& d111 = texmath::familyDirections(texmath::PoleFamily::F111);

    QCOMPARE(d100.size(), static_cast<size_t>(3));
    QCOMPARE(d110.size(), static_cast<size_t>(6));
    QCOMPARE(d111.size(), static_cast<size_t>(4));

    for (const auto& d : d100) {
        double len = std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
        QVERIFY(std::abs(len - 1.0) < 1e-9);
    }
}

void TestTextureMath::testPhiWeightIntegral()
{
    const int nbins = 18;
    const double bin = 90.0 / nbins;

    double totalWeight = 0.0;
    for (int j = 0; j < nbins; ++j) {
        totalWeight += texmath::phiWeight(j, bin);
    }

    // Analytically: sum(cos(j*bin) - cos((j+1)*bin)) = cos(0) - cos(pi/2) = 1.0
    QVERIFY2(std::abs(totalWeight - 1.0) < 1e-9,
             qPrintable(QString("Sum of phiWeights != 1.0: got %1").arg(totalWeight)));
}

void TestTextureMath::testMarchingSquaresContour()
{
    const int n = 4;
    std::vector<double> uniformSec(n * n, 0.5);

    // Uniform grid below level 1.0 has 0 contour segments
    auto segsEmpty = texmath::contour(uniformSec, n, 1.0);
    QCOMPARE(static_cast<int>(segsEmpty.size()), 0);

    // Gradient section crossing level 0.5
    std::vector<double> gradSec(n * n, 0.0);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            gradSec[i * n + j] = static_cast<double>(i) / (n - 1);
        }
    }
    auto segsGrad = texmath::contour(gradSec, n, 0.5);
    QVERIFY(segsGrad.size() > 0);

    for (const auto& s : segsGrad) {
        QVERIFY(s.x1 >= 0.0f && s.x1 <= 1.0f);
        QVERIFY(s.y1 >= 0.0f && s.y1 <= 1.0f);
        QVERIFY(s.x2 >= 0.0f && s.x2 <= 1.0f);
        QVERIFY(s.y2 >= 0.0f && s.y2 <= 1.0f);
    }
}

#include "test_requestcheck.h"
#include <QElapsedTimer>
#include <QTest>
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include "requestcheck.h"

namespace {

using Targets = std::map<std::string, double>;

// The same value for the three axes of a property, as "same in all directions" sends it.
void three(Targets& t, const std::vector<std::string>& keys, double v)
{
    for (const std::string& k : keys) t[k] = v;
}
const std::vector<std::string> kE = { "Ex", "Ey", "Ez" }, kG = { "Gxy", "Gyz", "Gxz" }, kNu = { "nuXy", "nuXz", "nuYz" };

// The conflicts whose text contains `part`.
int countWith(const std::vector<reqcheck::Issue>& issues, const std::string& part)
{
    return int(std::count_if(issues.begin(), issues.end(), [&](const reqcheck::Issue& i) { return i.text.find(part) != std::string::npos; }));
}
bool names(const reqcheck::Issue& issue, const std::string& key)
{
    return std::find(issue.keys.begin(), issue.keys.end(), key) != issue.keys.end();
}
QByteArray describe(const std::vector<reqcheck::Issue>& issues)
{
    QByteArray text;
    for (const reqcheck::Issue& i : issues) text += QByteArray("\n  ") + i.text.c_str();
    return text;
}

} // namespace

void TestRequestCheck::testFeasibleRequestsPass()
{
    const reqcheck::Material m{ 2.0, 0.38, 1.0 };
    QVERIFY(reqcheck::check({}, m).empty());
    // The tab's defaults: E 300 MPa on the three axes, density 0.30.
    Targets t;
    three(t, kE, 300.0);
    t["density"] = 0.30;
    QVERIFY2(reqcheck::check(t, m).empty(), describe(reqcheck::check(t, m)).constData());
    // Stiff, at a density that allows it, with the Poisson's ratio stiff cells have.
    three(t, kE, 650.0);
    three(t, kG, 146.0);
    three(t, kNu, 0.23);
    t["density"] = 0.50;
    QVERIFY2(reqcheck::check(t, m).empty(), describe(reqcheck::check(t, m)).constData());
}

void TestRequestCheck::testVoigtBound()
{
    const reqcheck::Material m{ 2.0, 0.38, 1.0 };
    QCOMPARE(reqcheck::voigtPerDensity("Ex", m), 2000.0);
    QVERIFY(std::abs(reqcheck::voigtPerDensity("Gxy", m) - 2000.0 / 2.76) < 1e-9);
    QVERIFY(std::abs(reqcheck::voigtPerDensity("K", m) - 2000.0 / (3.0 * (1.0 - 0.76))) < 1e-6);
    QCOMPARE(reqcheck::voigtPerDensity("nuXy", m), 0.0);

    // The request that came out softer than asked in the program (2026-11-15): E 653 MPa on three axes at density 0.30.
    Targets t;
    three(t, kE, 653.0);
    three(t, kG, 146.0);
    three(t, kNu, 0.40);
    t["density"] = 0.30;
    const std::vector<reqcheck::Issue> issues = reqcheck::check(t, m);
    QVERIFY2(issues.size() == 1, describe(issues).constData());
    QVERIFY(names(issues[0], "Ex") && names(issues[0], "Ey") && names(issues[0], "Ez") && names(issues[0], "density"));
    QVERIFY(!names(issues[0], "Gxy"));                                    // G 146 is below 0.30 × 725 = 217
    QVERIFY2(countWith(issues, "limit 600 MPa") == 1, describe(issues).constData());
    QVERIFY2(countWith(issues, "density ≥ 0.33") == 1, describe(issues).constData());   // 653 / 2000
    // At density 0.50 each modulus is within reach, but ν 0.40 with E 653 MPa means a bulk modulus
    // E / (3 (1 - 2 ν)) = 1088 MPa that no porous cell has there (the trace bounds; density ≥ 0.71 by the cubic scan).
    t["density"] = 0.50;
    const std::vector<reqcheck::Issue> together = reqcheck::check(t, m);
    QVERIFY2(together.size() == 1 && names(together[0], "nuXy") && countWith(together, "do not fit in one cell") == 1, describe(together).constData());
    QVERIFY2(countWith(together, "Together they need density ≥ 0.71") == 1, describe(together).constData());
    // Without the Poisson's ratios they fit.
    for (const std::string& k : kNu) t.erase(k);
    QVERIFY2(reqcheck::check(t, m).empty(), describe(reqcheck::check(t, m)).constData());

    // Without a density: only what no density reaches.
    Targets u;
    u["Ez"] = 2400.0;
    QVERIFY(countWith(reqcheck::check(u, m), "Stiffer than the solid itself") == 1);
    u["Ez"] = 1500.0;
    QVERIFY(reqcheck::check(u, m).empty());
}

void TestRequestCheck::testModuliTogether()
{
    const reqcheck::Material m{ 2.0, 0.38, 1.0 };
    // The request of 2026-11-15 evening: each modulus under its Voigt limit at density 0.34 (E 680, G 246 MPa), yet no
    // cell has them together. The numbers are those of the cubic scan in cellgen (E 610 -> G <= 69; G alone from 0.350;
    // both from 0.470; at 0.438 E 610 -> G <= 202 and G 239 -> E <= 478).
    Targets t;
    three(t, kE, 610.0);
    three(t, kG, 239.0);
    t["density"] = 0.34;
    QElapsedTimer timer;
    timer.start();
    std::vector<reqcheck::Issue> issues = reqcheck::check(t, m);
    const qint64 ms = timer.elapsed();
    qInfo("joint bound with a conflict: %lld ms", ms);
    QVERIFY2(issues.size() == 1, describe(issues).constData());
    for (const char* k : { "Ex", "Ey", "Ez", "Gxy", "Gyz", "Gxz", "density" }) QVERIFY(names(issues[0], k));
    QVERIFY2(countWith(issues, "do not fit in one cell") == 1, describe(issues).constData());
    QVERIFY2(countWith(issues, "G xy, G yz, G xz can reach at most 69") == 1, describe(issues).constData());
    QVERIFY2(countWith(issues, "239 MPa alone need density ≥ 0.35") == 1, describe(issues).constData());
    QVERIFY2(countWith(issues, "Together they need density ≥ 0.47") == 1, describe(issues).constData());
    QVERIFY2(ms < 500, "the check runs on every change of the form");

    // At the density the network gave the cell: still apart.
    t["density"] = 0.438;
    issues = reqcheck::check(t, m);
    QVERIFY2(issues.size() == 1, describe(issues).constData());
    QVERIFY2(countWith(issues, "can reach at most 202 MPa") == 1, describe(issues).constData());
    QVERIFY2(countWith(issues, "can reach at most 478 MPa") == 1, describe(issues).constData());

    // The cell it made (solved by FFT) fits at its density.
    Targets got = { { "Ex", 509.0 }, { "Ey", 308.0 }, { "Ez", 521.0 }, { "Gxy", 152.0 }, { "Gyz", 150.0 }, { "Gxz", 184.0 }, { "density", 0.438 } };
    timer.restart();
    QVERIFY2(reqcheck::check(got, m).empty(), describe(reqcheck::check(got, m)).constData());
    qInfo("joint bound without a conflict: %lld ms", timer.elapsed());

    // E 610 alone fits at 0.34 (with the shear free the bounds leave E along the axes almost at the Voigt level);
    // G 239 alone does not.
    Targets e, g;
    three(e, kE, 610.0);
    e["density"] = 0.34;
    QVERIFY2(reqcheck::check(e, m).empty(), describe(reqcheck::check(e, m)).constData());
    three(g, kG, 239.0);
    g["density"] = 0.34;
    issues = reqcheck::check(g, m);
    QVERIFY2(issues.size() == 1 && countWith(issues, "Together they need density ≥ 0.35") == 1, describe(issues).constData());

    // A solid weaker across the layers keeps the bound of the isotropic solid with E along them.
    const reqcheck::Material weakZ{ 2.0, 0.38, 0.9 };
    Targets w;
    three(w, kE, 610.0);
    three(w, kG, 200.0);   // under 0.34 x 0.9 x 725 = 222 across the layers
    w["density"] = 0.34;
    issues = reqcheck::check(w, weakZ);
    QVERIFY2(issues.size() == 1 && countWith(issues, "G xy, G yz, G xz can reach at most 69") == 1, describe(issues).constData());
    // A solid stiffer across them is not covered.
    const reqcheck::Material stiffZ{ 2.0, 0.38, 1.2 };
    QCOMPARE(countWith(reqcheck::check(t, stiffZ), "do not fit in one cell"), 0);
}

void TestRequestCheck::testSameInAllDirections()
{
    const reqcheck::Material m{ 2.0, 0.38, 1.0 };
    reqcheck::Form same;
    same.sameInAllDirections = true;
    // The form of 2026-11-15 night: E 197, G 95.3, ν 0.31 for all directions, E z / E x 5.00.
    Targets t;
    three(t, kE, 197.0);
    three(t, kG, 95.3);
    three(t, kNu, 0.31);
    t["EzEx"] = 5.0;
    std::vector<reqcheck::Issue> issues = reqcheck::check(t, m, same);
    QVERIFY2(issues.size() == 1, describe(issues).constData());   // not repeated as "E z / E x disagrees with E z / E x"
    QVERIFY(names(issues[0], "EzEx") && names(issues[0], "Ex") && names(issues[0], "Ez"));
    QVERIFY2(countWith(issues, "\"Same in all directions\" gives E one value") == 1, describe(issues).constData());
    QVERIFY2(countWith(issues, "E z / E x 5.00") == 1, describe(issues).constData());
    // The same numbers typed per axis are a contradiction of another kind: the ratio against the axes.
    issues = reqcheck::check(t, m);
    QVERIFY2(issues.size() == 1 && countWith(issues, "disagrees with the asked") == 1, describe(issues).constData());
    // A ratio of 1 agrees.
    t["EzEx"] = 1.0;
    QVERIFY2(reqcheck::check(t, m, same).empty(), describe(reqcheck::check(t, m, same)).constData());

    // E max / min above 1: E changes with direction. Typed per axis, equal axes with E max / min > 1 are a cubic cell.
    t.erase("EzEx");
    t["EmaxEmin"] = 1.5;
    t.erase("Gxy"); t.erase("Gyz"); t.erase("Gxz");
    t.erase("nuXy"); t.erase("nuXz"); t.erase("nuYz");
    issues = reqcheck::check(t, m, same);
    QVERIFY2(issues.size() == 1 && names(issues[0], "EmaxEmin"), describe(issues).constData());
    QVERIFY2(reqcheck::check(t, m).empty(), describe(reqcheck::check(t, m)).constData());

    // A shear ratio is a conflict only when G was given for all directions.
    Targets g;
    three(g, kE, 197.0);
    g["GxzGxy"] = 2.0;
    QVERIFY2(reqcheck::check(g, m, same).empty(), describe(reqcheck::check(g, m, same)).constData());
    three(g, kG, 80.0);
    issues = reqcheck::check(g, m, same);
    QVERIFY2(issues.size() == 1 && names(issues[0], "GxzGxy") && countWith(issues, "gives G one value") == 1, describe(issues).constData());
}

void TestRequestCheck::testVoigtFollowsTheLayers()
{
    // A solid half as stiff across the printed layers (along Z).
    const reqcheck::Material m{ 2.0, 0.38, 0.5 };
    QCOMPARE(reqcheck::voigtPerDensity("Ez", m), 1000.0);
    QCOMPARE(reqcheck::voigtPerDensity("Ex", m), 2000.0);
    QVERIFY(std::abs(reqcheck::voigtPerDensity("Gxz", m) - 0.5 * 2000.0 / 2.76) < 1e-9);
    QVERIFY(std::abs(reqcheck::voigtPerDensity("Emean", m) - std::cbrt(2000.0 * 2000.0 * 1000.0)) < 1e-9);
    Targets t;
    t["Ex"] = 600.0;
    t["density"] = 0.50;
    QVERIFY(reqcheck::check(t, m).empty());                              // 600 < 0.5 × 2000
    t["Ez"] = 600.0;
    const std::vector<reqcheck::Issue> issues = reqcheck::check(t, m);   // 600 > 0.5 × 1000
    QVERIFY2(issues.size() == 1 && names(issues[0], "Ez") && !names(issues[0], "Ex"), describe(issues).constData());
    QVERIFY(countWith(issues, "limit 500 MPa") == 1);
}

void TestRequestCheck::testHashinShtrikmanForAnIsotropicCell()
{
    double E = 0, G = 0, K = 0;
    reqcheck::hashinShtrikman(2000.0, 0.38, 1.0, &E, &G, &K);             // a solid block: the solid itself
    QVERIFY(std::abs(E - 2000.0) < 1e-6 && std::abs(G - 2000.0 / 2.76) < 1e-6 && std::abs(K - 2000.0 / 0.72) < 1e-6);
    reqcheck::hashinShtrikman(2000.0, 0.38, 0.30, &E, &G, &K);
    QVERIFY2(std::abs(E - 354.75) < 0.1, qPrintable(QString::number(E)));   // computed by hand
    QVERIFY(std::abs(G - 137.90) < 0.05 && std::abs(K - 276.62) < 0.05);

    const reqcheck::Material m{ 2.0, 0.38, 1.0 };
    Targets t;
    three(t, kE, 400.0);
    t["density"] = 0.30;
    QVERIFY(reqcheck::check(t, m).empty());                              // 400 < 600: some shape reaches it
    t["AU"] = 0.0;                                                       // the same in every direction: at most 355
    std::vector<reqcheck::Issue> issues = reqcheck::check(t, m);
    QVERIFY2(issues.size() == 1 && countWith(issues, "Hashin") == 1 && names(issues[0], "AU") && names(issues[0], "Ex"), describe(issues).constData());
    // E max / min = 1 alone allows a cell whose shear is not the same in every direction: no isotropic bound then.
    t.erase("AU");
    t["EmaxEmin"] = 1.0;
    QVERIFY2(reqcheck::check(t, m).empty(), describe(reqcheck::check(t, m)).constData());
    t["GmaxGmin"] = 1.0;
    QVERIFY(countWith(reqcheck::check(t, m), "Hashin") == 1);
    three(t, kE, 300.0);
    QVERIFY(reqcheck::check(t, m).empty());
    // Beyond both bounds: the stricter isotropic one is reported, not the Voigt one again.
    three(t, kE, 653.0);
    issues = reqcheck::check(t, m);
    QVERIFY2(issues.size() == 1 && countWith(issues, "Hashin") == 1, describe(issues).constData());
}

void TestRequestCheck::testPoissonRatiosMustGiveAPositiveCompliance()
{
    const reqcheck::Material m{ 2.0, 0.38, 1.0 };
    Targets t;
    three(t, kE, 300.0);
    three(t, kNu, 0.49);
    QVERIFY(reqcheck::check(t, m).empty());
    three(t, kNu, 0.50);                                                 // 1 − 3ν² − 2ν³ = 0
    QVERIFY(countWith(reqcheck::check(t, m), "below 0.5") == 1);
    three(t, kNu, 0.55);
    QVERIFY(countWith(reqcheck::check(t, m), "No elastic material") == 1);
    // One pair: |ν xy| < √(E x / E y).
    Targets u;
    u["Ex"] = 100.0;
    u["Ey"] = 400.0;
    u["nuXy"] = 0.45;
    QVERIFY(reqcheck::check(u, m).empty());
    u["nuXy"] = 0.60;                                                    // the limit is √(1/4) = 0.5
    QVERIFY(countWith(reqcheck::check(u, m), "No elastic material") == 1);
}

void TestRequestCheck::testIsotropicCellTiesGToEAndNu()
{
    const reqcheck::Material m{ 2.0, 0.38, 1.0 };
    Targets t;
    t["AU"] = 0.0;
    three(t, kE, 300.0);
    three(t, kNu, 0.30);
    three(t, kG, 115.0);                                                 // 300 / 2.6 = 115.4
    QVERIFY2(reqcheck::check(t, m).empty(), describe(reqcheck::check(t, m)).constData());
    three(t, kG, 50.0);
    QVERIFY(countWith(reqcheck::check(t, m), "G = E / (2 (1 + ν)) = 115") == 1);
    three(t, kG, 115.0);
    t["K"] = 400.0;                                                      // 300 / (3 × 0.4) = 250
    QVERIFY(countWith(reqcheck::check(t, m), "K = E / (3 (1 − 2 ν)) = 250") == 1);
    t["K"] = 250.0;
    t["EzEx"] = 2.0;                                                     // isotropic, yet twice as stiff along Z
    QVERIFY(countWith(reqcheck::check(t, m), "same in every direction") >= 1);
}

void TestRequestCheck::testRatiosAgainstTheAxes()
{
    const reqcheck::Material m{ 2.0, 0.38, 1.0 };
    Targets t;
    t["EzEx"] = 2.0;
    t["EmaxEmin"] = 1.5;                                                 // the largest ratio over all directions is at least 2
    QVERIFY(countWith(reqcheck::check(t, m), "cannot be below E z / E x 2.00") == 1);
    t["EmaxEmin"] = 2.5;
    QVERIFY(reqcheck::check(t, m).empty());

    Targets u;
    u["Ex"] = 300.0;
    u["Ez"] = 600.0;
    u["EmaxEmin"] = 1.5;
    QVERIFY(countWith(reqcheck::check(u, m), "cannot be below") == 1);
    u["EzEx"] = 2.0;                                                     // agrees with 600 / 300
    u["EmaxEmin"] = 2.0;
    QVERIFY2(reqcheck::check(u, m).empty(), describe(reqcheck::check(u, m)).constData());
    u["EzEx"] = 1.0;
    QVERIFY(countWith(reqcheck::check(u, m), "disagrees with the asked E z") == 1);

    Targets c;                                                           // E z / E y = (E z / E x) / (E y / E x)
    c["EzEx"] = 2.0;
    c["EyEx"] = 1.0;
    c["EzEy"] = 1.0;
    QVERIFY(countWith(reqcheck::check(c, m), "disagrees with E z / E x") == 1);
    c["EzEy"] = 2.0;
    QVERIFY(reqcheck::check(c, m).empty());

    Targets g;                                                           // the geometric mean of the axes
    g["Ex"] = 100.0;
    g["Ey"] = 300.0;
    g["Ez"] = 900.0;
    g["Emean"] = 300.0;
    QVERIFY(reqcheck::check(g, m).empty());
    g["Emean"] = 500.0;
    QVERIFY(countWith(reqcheck::check(g, m), "geometric mean") == 1);

    Targets n;                                                           // ν min is the smallest over all directions
    n["nuXy"] = 0.20;
    n["nuMin"] = 0.30;
    QVERIFY(countWith(reqcheck::check(n, m), "cannot be above ν xy 0.20") == 1);
    n["nuMin"] = 0.10;
    QVERIFY(reqcheck::check(n, m).empty());
}

void TestRequestCheck::testCubicCellFixesZenerAndEmaxEmin()
{
    const reqcheck::Material m{ 2.0, 0.38, 1.0 };
    Targets t;
    three(t, kE, 300.0);
    three(t, kG, 100.0);
    three(t, kNu, 0.30);
    // Zener = 2 G (1 + ν) / E = 0.867; E along [111]: 1 / (S11 − 2/3 (S11 − S12 − S44 / 2)) = 264.7; G max / min = 1 / 0.867.
    t["zener"] = 0.87;
    t["EmaxEmin"] = 1.13;
    t["GmaxGmin"] = 1.15;
    QVERIFY2(reqcheck::check(t, m).empty(), describe(reqcheck::check(t, m)).constData());
    t["zener"] = 2.0;
    QVERIFY(countWith(reqcheck::check(t, m), "Zener ratio is 2 G (1 + ν) / E = 0.87") == 1);
    t["zener"] = 0.87;
    t["EmaxEmin"] = 1.5;
    QVERIFY(countWith(reqcheck::check(t, m), "E max / min = 1.13") == 1);
    t["EmaxEmin"] = 1.13;
    t["GmaxGmin"] = 2.0;
    QVERIFY(countWith(reqcheck::check(t, m), "G max / min = 1.15") == 1);
}

void TestRequestCheck::testBulkModulusFollowsEAndNu()
{
    const reqcheck::Material m{ 2.0, 0.38, 1.0 };
    Targets t;
    three(t, kE, 300.0);
    three(t, kNu, 0.30);
    t["K"] = 255.0;                                                      // 1 / (3/300 − 6 × 0.3/300) = 250
    QVERIFY(reqcheck::check(t, m).empty());
    t["K"] = 400.0;
    QVERIFY(countWith(reqcheck::check(t, m), "K = 1 / (sum of the normal compliances) = 250") == 1);
}

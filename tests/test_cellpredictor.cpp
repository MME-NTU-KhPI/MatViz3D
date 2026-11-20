#include "test_cellpredictor.h"
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#define NEED_WEIGHTS() \
    do { if (!m_net.loaded()) QSKIP("models/cell_predictor.bin is not here (tools/neural/export_cell_predictor.py writes it)"); } while (false)

namespace {

// The cell tools/neural/export_cell_predictor.py --reference feeds to PyTorch: a sheet gyroid with two periods along x.
// shift moves it cyclically along the three axes.
std::vector<uint8_t> referenceCell(int n, int sx = 0, int sy = 0, int sz = 0)
{
    const double w = 2.0 * M_PI / n;
    std::vector<uint8_t> cell(size_t(n) * n * n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            for (int k = 0; k < n; ++k) {
                const double x = i + 0.5, y = j + 0.5, z = k + 0.5;
                const double g = std::sin(2 * w * x) * std::cos(w * y) + std::sin(w * y) * std::cos(w * z) + std::sin(w * z) * std::cos(2 * w * x);
                cell[(size_t((i + sx) % n) * n + size_t((j + sy) % n)) * n + size_t((k + sz) % n)] = std::fabs(g) < 0.75 ? 1 : 0;
            }
    return cell;
}

// The code of the diffusion test: a smooth periodic pattern.
std::vector<float> patternCode(const CellPredictor& p)
{
    const int zc = p.codeChannels(), n = p.codeSize();
    const double w = 2.0 * M_PI / n;
    std::vector<float> x(p.codeLength());
    for (int c = 0; c < zc; ++c)
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                for (int k = 0; k < n; ++k)
                    x[((size_t(c) * n + i) * n + j) * n + k] = float(
                        1.3 * std::sin(w * ((c + 1) * i + 2 * j + 3 * k) + 0.5 * c) * std::cos(w * (i - (c + 2) * j + k)) + 0.1 * c);
    return x;
}

// What PyTorch returns for the weights of run pred1, step 14 000 (export_cell_predictor.py --reference).
const float kCellScores[24] = { -0.052080f, 0.756728f, 0.869411f, 0.580728f, 1.106047f, 0.651532f, -0.576037f, -0.539939f,
                                0.130762f, 0.589285f, 0.837909f, 1.346690f, 1.348131f, 0.173735f, 0.475205f, 0.148076f,
                                0.152884f, 0.952743f, 1.213303f, -0.093299f, 0.040817f, 0.504616f, 0.192370f, 0.906558f };
const float kPatternScores[24] = { -1.627690f, -4.867162f, -3.561207f, -2.894949f, -3.799808f, -2.317131f, 1.841682f, 0.046935f,
                                   -1.087496f, -3.692518f, -3.167688f, -2.745718f, -5.459402f, 3.440823f, 0.307187f, 1.997019f,
                                   1.104616f, -1.489377f, -3.654605f, 3.058352f, 3.466674f, -3.851171f, -2.995006f, -2.303265f };

} // namespace

void TestCellPredictor::initTestCase()
{
    m_file = qEnvironmentVariable("MATVIZ_CELL_PREDICTOR");
#ifdef MATVIZ_SOURCE_DIR
    if (m_file.isEmpty()) m_file = QStringLiteral(MATVIZ_SOURCE_DIR "/models/cell_predictor.bin");
#endif
    if (m_file.isEmpty() || !QFileInfo::exists(m_file)) return;
    std::string error;
    QVERIFY2(m_net.load(m_file.toStdString(), &error), error.c_str());
}

void TestCellPredictor::testLoadReadsTheHeader()
{
    NEED_WEIGHTS();
    QCOMPARE(m_net.cellSize(), 64);
    QCOMPARE(m_net.codeChannels(), 4);
    QCOMPARE(m_net.codeSize(), 16);
    QCOMPARE(m_net.codeLength(), size_t(4 * 16 * 16 * 16));
    QCOMPARE(m_net.targets().size(), size_t(24));
    QCOMPARE(m_net.targets().front().key, std::string("Ex"));
    QCOMPARE(m_net.targets().front().kind, 3);          // a modulus: relative to the material
    QCOMPARE(m_net.targets().back().key, std::string("density"));
    QCOMPARE(m_net.targets().back().kind, 0);
    QVERIFY(m_net.weightCount() > 1000000);
    QVERIFY(QString::fromStdString(m_net.name()).startsWith(QStringLiteral("pred")));
    QVERIFY(!m_net.note().empty());
}

void TestCellPredictor::testEncoderMatchesPyTorch()
{
    NEED_WEIGHTS();
    const std::vector<uint8_t> cell = referenceCell(m_net.cellSize());
    size_t solid = 0;
    for (uint8_t v : cell) solid += v;
    QCOMPARE(solid, size_t(129024));   // the same cell as in PyTorch
    const std::vector<float> code = m_net.encode(cell);
    QCOMPARE(code.size(), m_net.codeLength());
    double sum = 0.0, sq = 0.0;
    for (float v : code) { sum += v; sq += double(v) * v; }
    QVERIFY(std::fabs(sum / double(code.size()) - 0.027508) < 1e-4);
    QVERIFY(std::fabs(std::sqrt(sq / double(code.size())) - 1.096348) < 1e-4);
    const struct { int c, i, j, k; float value; } probes[] = {
        { 0, 0, 0, 0, 0.999322f }, { 1, 1, 2, 3, 0.817759f }, { 2, 15, 0, 7, -1.152861f }, { 3, 8, 8, 8, 1.710610f },
        { 0, 5, 11, 2, 0.204642f }, { 3, 15, 15, 15, -1.211197f }, { 1, 0, 9, 14, -0.994237f }, { 2, 7, 3, 12, -0.707081f } };
    const int n = m_net.codeSize();
    for (const auto& p : probes) {
        const float mine = code[((size_t(p.c) * n + p.i) * n + p.j) * n + p.k];
        QVERIFY2(std::fabs(mine - p.value) < 5e-4f, qPrintable(QString("code[%1][%2][%3][%4] = %5, PyTorch %6").arg(p.c).arg(p.i).arg(p.j).arg(p.k).arg(mine).arg(p.value)));
    }
}

void TestCellPredictor::testPropertiesMatchPyTorch()
{
    NEED_WEIGHTS();
    const std::vector<uint8_t> cell = referenceCell(m_net.cellSize());
    const std::vector<float> z = m_net.scaled(m_net.encode(cell), 0, 0.38, 1.0);
    QCOMPARE(z.size(), size_t(24));
    for (size_t j = 0; j < z.size(); ++j)
        QVERIFY2(std::fabs(z[j] - kCellScores[j]) < 2e-3f, qPrintable(QString("%1: %2, PyTorch %3").arg(QString::fromStdString(m_net.targets()[j].key)).arg(z[j]).arg(kCellScores[j])));

    // In the units of the dataset for a material of 2 GPa: moduli in Pa, ratios and the density as they are.
    const std::map<std::string, double> p = m_net.predict(cell, 2.0, 0.38, 1.0);
    QCOMPARE(p.size(), size_t(24));
    const std::map<std::string, double> expected = { { "Ex", 1.94832e8 }, { "Ey", 4.55687e8 }, { "Ez", 4.6219e8 }, { "Gxy", 1.32327e8 },
                                                     { "Emean", 3.53146e8 }, { "K", 2.17968e8 }, { "EzEx", 2.38052 }, { "zener", 1.07313 },
                                                     { "nuXy", 0.186724 }, { "AU", 1.20376 }, { "density", 0.485501 } };
    for (const auto& item : expected) {
        QVERIFY2(p.count(item.first) == 1, item.first.c_str());
        QVERIFY2(std::fabs(p.at(item.first) / item.second - 1.0) < 3e-3, qPrintable(QString("%1: %2, PyTorch %3").arg(QString::fromStdString(item.first)).arg(p.at(item.first)).arg(item.second)));
    }
    // A modulus scales with the material's E, a ratio does not.
    const std::map<std::string, double> stiff = m_net.predict(cell, 4.0, 0.38, 1.0);
    QVERIFY(std::fabs(stiff.at("Ex") / p.at("Ex") - 2.0) < 1e-9);
    QVERIFY(std::fabs(stiff.at("EzEx") / p.at("EzEx") - 1.0) < 1e-9);
    // The cell's own density is 129024 / 64^3 = 0.4922; the network reads it to about 0.01.
    QVERIFY(std::fabs(p.at("density") - 129024.0 / (64.0 * 64.0 * 64.0)) < 0.02);
}

void TestCellPredictor::testNoisedCodeMatchesPyTorch()
{
    NEED_WEIGHTS();
    const std::vector<float> z = m_net.scaled(patternCode(m_net), 500, 0.33, 0.8);
    QCOMPARE(z.size(), size_t(24));
    for (size_t j = 0; j < z.size(); ++j)
        QVERIFY2(std::fabs(z[j] - kPatternScores[j]) < 2e-3f, qPrintable(QString("%1: %2, PyTorch %3").arg(QString::fromStdString(m_net.targets()[j].key)).arg(z[j]).arg(kPatternScores[j])));
    // The level and the material are inputs: another level or another material is another answer.
    const std::vector<float> clean = m_net.scaled(patternCode(m_net), 0, 0.33, 0.8), other = m_net.scaled(patternCode(m_net), 500, 0.42, 0.8);
    double dLevel = 0.0, dMaterial = 0.0;
    for (size_t j = 0; j < z.size(); ++j) {
        dLevel = std::max(dLevel, double(std::fabs(clean[j] - z[j])));
        dMaterial = std::max(dMaterial, double(std::fabs(other[j] - z[j])));
    }
    QVERIFY(dLevel > 1e-2);
    QVERIFY(dMaterial > 1e-2);
}

void TestCellPredictor::testShiftedCellGivesTheSameProperties()
{
    NEED_WEIGHTS();
    // 16 voxels = 4 positions of the code = 1 position of the predictor's last grid: every layer wraps around,
    // so the cell moved by a multiple of 16 voxels is the same cell to the network.
    const int n = m_net.cellSize();
    const std::vector<float> a = m_net.scaled(m_net.encode(referenceCell(n)), 0, 0.38, 1.0);
    const std::vector<float> b = m_net.scaled(m_net.encode(referenceCell(n, 16, 32, 48)), 0, 0.38, 1.0);
    QCOMPARE(a.size(), b.size());
    for (size_t j = 0; j < a.size(); ++j) QVERIFY2(std::fabs(a[j] - b[j]) < 1e-3f, qPrintable(QString("target %1: %2 against %3").arg(j).arg(a[j]).arg(b[j])));
}

void TestCellPredictor::testWrongSizesGiveNothing()
{
    NEED_WEIGHTS();
    QVERIFY(m_net.encode(std::vector<uint8_t>(32 * 32 * 32, 1)).empty());
    QVERIFY(m_net.scaled(std::vector<float>(100, 0.0f), 0, 0.38, 1.0).empty());
    QVERIFY(m_net.predict(std::vector<uint8_t>(10, 1), 2.0, 0.38, 1.0).empty());
}

void TestCellPredictor::testBrokenFilesAreRefused()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    CellPredictor net;
    std::string error;
    QVERIFY(!net.load(dir.filePath("missing.bin").toStdString(), &error));
    QVERIFY(!error.empty());

    const QString other = dir.filePath("other.bin");
    {
        QFile f(other);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray("MVCELLF1").append(QByteArray(200, '\1')));   // the magic of the diffusion file
    }
    QVERIFY(!net.load(other.toStdString(), &error));
    QVERIFY(!net.loaded());

    if (m_file.isEmpty() || !QFileInfo::exists(m_file)) return;
    // The real file cut short: the header is fine, the weights do not fit it.
    QFile whole(m_file);
    QVERIFY(whole.open(QIODevice::ReadOnly));
    const QByteArray data = whole.readAll();
    const QString cut = dir.filePath("cut.bin");
    {
        QFile f(cut);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data.left(data.size() - 4096));
    }
    QVERIFY(!net.load(cut.toStdString(), &error));
    QVERIFY(!net.loaded());
    // And with a tail that does not belong.
    const QString longer = dir.filePath("longer.bin");
    {
        QFile f(longer);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data + QByteArray(8, '\0'));
    }
    QVERIFY(!net.load(longer.toStdString(), &error));
}

#include "test_celldecoder.h"
#include <QFile>
#include <QTest>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include "celldecoder.h"

namespace {

QByteArray shippedWeights()
{
    QFile f(QStringLiteral(":/models/cell_decoder.bin"));
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

bool loadShipped(CellDecoder& d, std::string* error = nullptr)
{
    const QByteArray w = shippedWeights();
    return d.load(reinterpret_cast<const unsigned char*>(w.constData()), size_t(w.size()), error);
}

// The code tools/neural/export_cell_decoder.py --reference feeds to PyTorch:
// a smooth periodic pattern around the stored mean of every channel.
std::vector<float> referenceCode(const CellDecoder& d)
{
    const int zc = d.codeChannels(), n = d.codeSize();
    const double w = 2.0 * M_PI / n;
    std::vector<float> z(size_t(zc) * n * n * n);
    for (int c = 0; c < zc; ++c)
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                for (int k = 0; k < n; ++k)
                    z[((size_t(c) * n + i) * n + j) * n + k] = float(
                        double(d.codeMean()[c]) + double(d.codeSpread()[c])
                        * std::sin(w * ((c + 1) * i + 2 * j + 3 * k) + 0.5 * c) * std::cos(w * (i - (c + 2) * j + k)));
    return z;
}

} // namespace

void TestCellDecoder::testLoadReadsTheHeader()
{
    CellDecoder d;
    QVERIFY(!d.loaded());
    std::string error;
    QVERIFY2(loadShipped(d, &error), error.c_str());
    QVERIFY(d.loaded());
    QCOMPARE(d.codeChannels(), 4);
    QCOMPARE(d.codeSize(), 16);
    QCOMPARE(d.cellSize(), 64);
    QCOMPARE(d.threshold(), 0.4375f);
    QCOMPARE(d.weightCount(), size_t(567912));
    QCOMPARE(QString::fromStdString(d.name()), QStringLiteral("ae1 step 16965"));
    QCOMPARE(d.codeMean().size(), size_t(4));
    QCOMPARE(d.codeSpread().size(), size_t(4));
    for (float s : d.codeSpread()) QVERIFY(s > 0.1f && s < 5.0f);
}

void TestCellDecoder::testMatchesPyTorchOnTheReferenceCode()
{
    CellDecoder d;
    QVERIFY(loadShipped(d));
    const std::vector<float> logits = d.decodeLogits(referenceCode(d));
    const int n = d.cellSize();
    QCOMPARE(logits.size(), size_t(n) * n * n);

    // PyTorch on the same code (export_cell_decoder.py --reference): voxel i, j, k and its logit.
    struct Probe { int i, j, k; float logit; };
    const Probe probes[] = { {0, 0, 0, 6.04368f}, {1, 2, 3, -5.44553f}, {17, 40, 5, -7.45791f}, {31, 31, 31, 5.02556f},
                             {32, 0, 63, -7.09946f}, {63, 63, 63, 5.02556f}, {10, 50, 30, 1.02976f}, {45, 7, 22, 1.19063f} };
    for (const Probe& p : probes) {
        const float got = logits[(size_t(p.i) * n + p.j) * n + p.k];
        QVERIFY2(std::abs(got - p.logit) < 2e-3f,
                 qPrintable(QString("voxel %1 %2 %3: %4, PyTorch %5").arg(p.i).arg(p.j).arg(p.k).arg(got).arg(p.logit)));
    }

    // 96 820 solid voxels in PyTorch; a voxel sitting on the threshold may fall either way in float.
    const std::vector<uint8_t> solid = d.decode(referenceCode(d));
    long count = 0;
    for (uint8_t v : solid) count += v;
    QVERIFY2(std::labs(count - 96820) <= 3, qPrintable(QString("solid voxels %1, PyTorch 96820").arg(count)));
    // decode() is the threshold on the share of solid, sigmoid(logit) >= 0.4375.
    const float cut = std::log(d.threshold() / (1.0f - d.threshold()));
    for (size_t v = 0; v < solid.size(); v += 97)
        QCOMPARE(int(solid[v]), int(logits[v] >= cut));
}

void TestCellDecoder::testShiftedCodeGivesTheShiftedCell()
{
    // Every convolution pads circularly: the code moved by one position is the cell moved by 4 voxels.
    CellDecoder d;
    QVERIFY(loadShipped(d));
    const int zc = d.codeChannels(), m = d.codeSize(), n = d.cellSize(), step = n / m;
    const std::vector<float> code = d.randomCode(11, 1.0);
    const int si = 1, sj = 2, sk = 5;
    std::vector<float> moved(code.size());
    for (int c = 0; c < zc; ++c)
        for (int i = 0; i < m; ++i)
            for (int j = 0; j < m; ++j)
                for (int k = 0; k < m; ++k)
                    moved[((size_t(c) * m + (i + si) % m) * m + (j + sj) % m) * m + (k + sk) % m] = code[((size_t(c) * m + i) * m + j) * m + k];
    const std::vector<float> a = d.decodeLogits(code), b = d.decodeLogits(moved);
    float worst = 0.0f;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            for (int k = 0; k < n; ++k) {
                const size_t to = (size_t((i + si * step) % n) * n + (j + sj * step) % n) * n + (k + sk * step) % n;
                worst = std::max(worst, std::abs(b[to] - a[(size_t(i) * n + j) * n + k]));
            }
    QVERIFY2(worst < 1e-3f, qPrintable(QString("largest difference of a logit %1").arg(worst)));
}

void TestCellDecoder::testRandomCodeIsReproducibleWithTheStoredStatistics()
{
    CellDecoder d;
    QVERIFY(loadShipped(d));
    const int zc = d.codeChannels(), m = d.codeSize();
    const size_t per = size_t(m) * m * m;
    QVERIFY(d.randomCode(7) == d.randomCode(7));
    QVERIFY(d.randomCode(7) != d.randomCode(8));
    QVERIFY(d.randomCode(7, 1.5) == d.randomCode(7, 1.5));
    for (double smooth : { 0.0, 1.0, 2.5 }) {
        const std::vector<float> z = d.randomCode(7, smooth);
        QCOMPARE(z.size(), per * zc);
        for (int c = 0; c < zc; ++c) {
            double sum = 0.0, sq = 0.0;
            for (size_t v = 0; v < per; ++v) { const double x = z[c * per + v]; sum += x; sq += x * x; }
            const double mean = sum / per, spread = std::sqrt(std::max(0.0, sq / per - mean * mean));
            QVERIFY2(std::abs(mean - d.codeMean()[c]) < 1e-3, qPrintable(QString("channel %1, blur %2: mean %3").arg(c).arg(smooth).arg(mean)));
            QVERIFY2(std::abs(spread - d.codeSpread()[c]) < 1e-3 * d.codeSpread()[c],
                     qPrintable(QString("channel %1, blur %2: spread %3").arg(c).arg(smooth).arg(spread)));
        }
    }
    // The blur ties neighbouring code positions together.
    auto neighbourCorrelation = [&](const std::vector<float>& z) {
        double sum = 0.0;
        for (int c = 0; c < zc; ++c) {
            double s = 0.0;
            for (int i = 0; i < m; ++i)
                for (int j = 0; j < m; ++j)
                    for (int k = 0; k < m; ++k) {
                        const double x = (z[((size_t(c) * m + i) * m + j) * m + k] - d.codeMean()[c]) / d.codeSpread()[c];
                        const double y = (z[((size_t(c) * m + i) * m + j) * m + (k + 1) % m] - d.codeMean()[c]) / d.codeSpread()[c];
                        s += x * y;
                    }
            sum += s / per;
        }
        return sum / zc;
    };
    QVERIFY(std::abs(neighbourCorrelation(d.randomCode(3, 0.0))) < 0.1);
    QVERIFY(neighbourCorrelation(d.randomCode(3, 1.5)) > 0.6);
    // A decoded random code is a cell of 64^3 zeros and ones with solid and void both present.
    const std::vector<uint8_t> solid = d.decode(d.randomCode(3, 0.0));
    long count = 0;
    for (uint8_t v : solid) { QVERIFY(v <= 1); count += v; }
    const double density = double(count) / double(solid.size());
    QVERIFY2(density > 0.1 && density < 0.9, qPrintable(QString("density %1").arg(density)));
}

void TestCellDecoder::testBrokenWeightsAreRefused()
{
    const QByteArray good = shippedWeights();
    QVERIFY(good.size() > 1000);
    auto load = [](const QByteArray& w, std::string* error) {
        CellDecoder d;
        const bool ok = d.load(reinterpret_cast<const unsigned char*>(w.constData()), size_t(w.size()), error);
        return ok || d.loaded();
    };
    std::string error;
    QVERIFY(!load(QByteArray(), &error));
    QVERIFY(!error.empty());
    QByteArray otherFile = good;
    otherFile[0] = 'X';
    error.clear();
    QVERIFY(!load(otherFile, &error));
    QCOMPARE(QString::fromStdString(error), QStringLiteral("not a cell decoder file"));
    error.clear();
    QVERIFY(!load(good.left(good.size() - 4), &error));     // the last weight is missing
    QVERIFY(!error.empty());
    error.clear();
    QVERIFY(!load(good.left(good.size() / 2), &error));
    QVERIFY(!error.empty());
    error.clear();
    QVERIFY(!load(good + QByteArray(8, '\0'), &error));     // something after the weights
    QVERIFY(!error.empty());
}

// A slab cut by the cell's face is one body (the cell is periodic), specks next to it are cleared.
void TestCellDecoder::testKeepLargestBodyIsPeriodic()
{
    const int n = 16;
    auto at = [n](int i, int j, int k) { return (size_t(i) * n + j) * n + k; };
    std::vector<uint8_t> cell(size_t(n) * n * n, 0);
    for (int i : { 0, 1, n - 2, n - 1 })                      // a slab of 4 layers across the face x = 0
        for (int j = 0; j < n; ++j)
            for (int k = 0; k < n; ++k) cell[at(i, j, k)] = 1;
    cell[at(8, 8, 8)] = 1;                                    // a speck of one voxel
    for (int k : { 0, n - 1 }) cell[at(8, 3, k)] = 1;         // a speck of two voxels across the face z = 0
    cell[at(7, 7, 7)] = 1;                                    // touches the first speck by a corner only: its own body
    const std::vector<uint8_t> before = cell;

    int pieces = -1;
    const size_t removed = CellDecoder::keepLargestBody(cell, n, &pieces);
    QCOMPARE(removed, size_t(4));
    QCOMPARE(pieces, 3);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            for (int k = 0; k < n; ++k)
                QCOMPARE(int(cell[at(i, j, k)]), int(i < 2 || i >= n - 2));

    // One body: nothing to clear. A wrong size: left alone.
    QCOMPARE(CellDecoder::keepLargestBody(cell, n, &pieces), size_t(0));
    QCOMPARE(pieces, 0);
    std::vector<uint8_t> other = before;
    QCOMPARE(CellDecoder::keepLargestBody(other, n + 1, &pieces), size_t(0));
    QVERIFY(other == before);
}

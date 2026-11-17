#include "test_celldiffusion.h"
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#define NEED_WEIGHTS() \
    do { if (!m_net.opened()) QSKIP("models/cell_diffusion.bin is not here (tools/neural/export_cell_diffusion.py writes it)"); } while (false)

namespace {

// The input tools/neural/export_cell_diffusion.py --reference feeds to PyTorch: a smooth periodic pattern.
std::vector<float> referenceInput(const CellDiffusion& d)
{
    const int zc = d.codeChannels(), n = d.codeSize();
    const double w = 2.0 * M_PI / n;
    std::vector<float> x(d.codeLength());
    for (int c = 0; c < zc; ++c)
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                for (int k = 0; k < n; ++k)
                    x[((size_t(c) * n + i) * n + j) * n + k] = float(
                        1.3 * std::sin(w * ((c + 1) * i + 2 * j + 3 * k) + 0.5 * c) * std::cos(w * (i - (c + 2) * j + k)) + 0.1 * c);
    return x;
}

// The request of the reference: a cell of 300 MPa along every axis, density 0.30, in a material of 2 GPa.
std::vector<float> referenceRequest(const CellDiffusion& d, CellDiffusion::RequestNotes* notes = nullptr)
{
    return d.request({ { "Ex", 3.0e8 }, { "Ey", 3.0e8 }, { "Ez", 3.0e8 }, { "density", 0.30 }, { "EmaxEmin", 1.5 }, { "nuMin", 0.1 } },
                     2.0, 0.38, 1.0, notes);
}

// "dif4" of "dif4 step 20000": the training run of the weights.
std::string runOf(const CellDiffusion& d)
{
    return d.name().substr(0, d.name().find(' '));
}

struct Probe { int c, i, j, k; float value; };

// The largest |got - PyTorch| over the probes of a code-shaped array.
double worstProbe(const CellDiffusion& d, const std::vector<float>& got, const std::vector<Probe>& probes)
{
    const int n = d.codeSize();
    double worst = 0.0;
    for (const Probe& p : probes)
        worst = std::max(worst, double(std::abs(got[((size_t(p.c) * n + p.i) * n + p.j) * n + p.k] - p.value)));
    return worst;
}

int indexOf(const CellDiffusion& d, const std::string& key)
{
    for (size_t j = 0; j < d.targets().size(); ++j)
        if (d.targets()[j].key == key) return int(j);
    return -1;
}

bool has(const std::vector<std::string>& list, const std::string& key)
{
    return std::find(list.begin(), list.end(), key) != list.end();
}

} // namespace

void TestCellDiffusion::initTestCase()
{
    m_file = qEnvironmentVariable("MATVIZ_CELL_DIFFUSION");
#ifdef MATVIZ_SOURCE_DIR
    if (m_file.isEmpty()) m_file = QStringLiteral(MATVIZ_SOURCE_DIR "/models/cell_diffusion.bin");
#endif
    if (m_file.isEmpty() || !QFileInfo::exists(m_file)) return;
    std::string error;
    QVERIFY2(m_net.open(m_file.toStdString(), &error), error.c_str());
    QVERIFY2(m_net.load(&error), error.c_str());
}

void TestCellDiffusion::testOpenReadsTheHeader()
{
    NEED_WEIGHTS();
    QVERIFY(m_net.loaded());
    QCOMPARE(m_net.codeChannels(), 4);
    QCOMPARE(m_net.codeSize(), 16);
    QCOMPARE(m_net.codeLength(), size_t(4 * 16 * 16 * 16));
    QCOMPARE(m_net.targets().size(), size_t(24));
    QCOMPARE(m_net.requestSize(), 50);   // 24 targets, 24 flags "given", 2 numbers of the material
    QCOMPARE(m_net.levels(), 1000);
    // The first recipe (dif2) starts at level 980 and has no layers for the tokens of the request; the second
    // (dif3, dif4) starts from pure noise and is asked twice per step when the file carries a guidance.
    QCOMPARE(m_net.startLevel(), m_net.predictsV() ? 999 : 980);
    QCOMPARE(m_net.steps(), 50);
    QCOMPARE(m_net.weightCount(), m_net.usesTokens() ? size_t(25465160) : size_t(23484868));
    QCOMPARE(m_net.passesPerStep(), m_net.guidance() != 1.0f ? 2 : 1);
    QVERIFY(m_net.guidance() >= 1.0f);
    QVERIFY(m_net.clamp() > 1.0f);
    QVERIFY(!m_net.name().empty());
    QCOMPARE(m_net.codeMean().size(), size_t(4));
    QCOMPARE(m_net.codeSpread().size(), size_t(4));
    for (float s : m_net.codeSpread()) QVERIFY(s > 0.0f);
    // The targets the Neural tab sends, by the names of its form.
    for (const char* key : { "Ex", "Ey", "Ez", "Gxy", "Gyz", "Gxz", "nuXy", "nuXz", "nuYz", "Emean", "Gmean", "EzEx", "EyEx", "EzEy",
                             "GxzGxy", "GyzGxy", "GyzGxz", "EmaxEmin", "GmaxGmin", "AU", "zener", "K", "nuMin", "density" })
        QVERIFY2(indexOf(m_net, key) >= 0, key);
    // Moduli are scaled by the material (kind 3), the density is taken as it is.
    QCOMPARE(m_net.targets()[size_t(indexOf(m_net, "Ex"))].kind, 3);
    QCOMPARE(m_net.targets()[size_t(indexOf(m_net, "K"))].kind, 3);
    QCOMPARE(m_net.targets()[size_t(indexOf(m_net, "density"))].kind, 0);
}

void TestCellDiffusion::testRequestIsBuiltAsInTraining()
{
    NEED_WEIGHTS();
    CellDiffusion::RequestNotes notes;
    const std::vector<float> req = referenceRequest(m_net, &notes);
    const size_t nt = m_net.targets().size();
    QCOMPARE(req.size(), size_t(m_net.requestSize()));
    QCOMPARE(notes.used.size(), size_t(6));
    QVERIFY(notes.clipped.empty());
    QVERIFY(notes.unknown.empty());
    // A flag "given" for every target passed, and nothing for the rest.
    const std::vector<std::string> given = { "Ex", "Ey", "Ez", "density", "EmaxEmin", "nuMin" };
    for (size_t j = 0; j < nt; ++j) {
        const bool asked = has(given, m_net.targets()[j].key);
        QCOMPARE(req[nt + j], asked ? 1.0f : 0.0f);
        if (!asked) QCOMPARE(req[j], 0.0f);
    }
    // A modulus is passed relative to the material: the same cell in a material twice as stiff is asked for twice the modulus.
    const std::vector<float> stiff = m_net.request({ { "Ex", 6.0e8 } }, 4.0, 0.38, 1.0);
    const size_t ex = size_t(indexOf(m_net, "Ex"));
    QVERIFY(std::abs(stiff[ex] - req[ex]) < 1e-5f);
    // No target at all: the material alone.
    const std::vector<float> bare = m_net.request({}, 2.0, 0.38, 1.0);
    for (size_t j = 0; j < 2 * nt; ++j) QCOMPARE(bare[j], 0.0f);
    QCOMPARE(bare[2 * nt], req[2 * nt]);
    QCOMPARE(bare[2 * nt + 1], req[2 * nt + 1]);

    // The numbers cellgen's Conditions.request gives for this request (export_cell_diffusion.py --reference);
    // they belong to the table of the dataset the runs dif2 - dif6 were trained on.
    const std::string run = runOf(m_net);
    if (run != "dif2" && run != "dif3" && run != "dif4" && run != "dif5" && run != "dif6")
        QSKIP("the reference numbers are those of the runs dif2 - dif6");
    const std::map<int, double> expected = { { 0, 0.358807 }, { 1, 0.358807 }, { 2, 0.450899 }, { 13, -0.643803 }, { 22, 0.219591 },
                                             { 23, -1.015125 }, { 48, 0.129461 }, { 49, 1.220474 } };
    for (const auto& e : expected)
        QVERIFY2(std::abs(double(req[size_t(e.first)]) - e.second) < 2e-6,
                 qPrintable(QString("request[%1] = %2, expected %3").arg(e.first).arg(req[size_t(e.first)], 0, 'f', 6).arg(e.second, 0, 'f', 6)));
}

void TestCellDiffusion::testRequestNotesAndRanges()
{
    NEED_WEIGHTS();
    // What cannot be passed: a name the network does not know, a modulus that is not positive.
    CellDiffusion::RequestNotes notes;
    std::vector<float> req = m_net.request({ { "Ex", -5.0 }, { "no_such_target", 1.0 }, { "density", 0.30 } }, 2.0, 0.38, 1.0, &notes);
    const size_t nt = m_net.targets().size();
    QCOMPARE(notes.used.size(), size_t(1));
    QCOMPARE(notes.unknown.size(), size_t(2));
    QVERIFY(has(notes.unknown, "Ex"));
    QVERIFY(has(notes.unknown, "no_such_target"));
    QCOMPARE(req[nt + size_t(indexOf(m_net, "Ex"))], 0.0f);

    // A target outside what the training cells span is passed at the edge of the span.
    double lo = 0.0, hi = 0.0;
    QVERIFY(m_net.targetRange("density", 2.0, &lo, &hi));
    QVERIFY(lo > 0.0 && lo < hi && hi < 1.0);
    notes = {};
    req = m_net.request({ { "density", 5.0 } }, 2.0, 0.38, 1.0, &notes);
    QVERIFY(has(notes.used, "density"));
    QVERIFY(has(notes.clipped, "density"));
    const size_t rho = size_t(indexOf(m_net, "density"));
    QVERIFY(std::abs(req[rho] - m_net.request({ { "density", hi } }, 2.0, 0.38, 1.0)[rho]) < 1e-5f);
    notes = {};
    m_net.request({ { "density", 0.5 * (lo + hi) } }, 2.0, 0.38, 1.0, &notes);
    QVERIFY(notes.clipped.empty());

    // The range of a modulus is in Pa and follows the material.
    double lo2 = 0.0, hi2 = 0.0;
    QVERIFY(m_net.targetRange("Ex", 2.0, &lo, &hi));
    QVERIFY(m_net.targetRange("Ex", 4.0, &lo2, &hi2));
    QVERIFY(lo > 0.0 && lo < hi);
    QVERIFY(hi < 2.0e9);                       // a porous cell is softer than its solid
    QVERIFY(std::abs(hi2 / hi - 2.0) < 1e-6);
    QVERIFY(std::abs(lo2 / lo - 2.0) < 1e-6);
    QVERIFY(!m_net.targetRange("no_such_target", 2.0, &lo, &hi));
}

void TestCellDiffusion::testMatchesPyTorchOnTheReferenceInput()
{
    NEED_WEIGHTS();
    const std::vector<float> x = referenceInput(m_net), req = referenceRequest(m_net);
    const std::vector<float> e = m_net.predict(x, 500, req);
    QCOMPARE(e.size(), m_net.codeLength());   // the noise or v: the size of the code in both recipes
    for (float v : e) QVERIFY(std::isfinite(v));
    // The same pass again gives the same numbers, and the request matters.
    QVERIFY(m_net.predict(x, 500, req) == e);
    QVERIFY(m_net.predict(x, 500, m_net.request({ { "density", 0.6 } }, 2.0, 0.38, 1.0)) != e);
    // Wrong sizes and noise levels that do not exist are refused.
    QVERIFY(m_net.predict(std::vector<float>(x.size() - 1), 500, req).empty());
    QVERIFY(m_net.predict(x, 500, std::vector<float>(req.size() + 1)).empty());
    QVERIFY(m_net.predict(x, -1, req).empty());
    QVERIFY(m_net.predict(x, m_net.levels(), req).empty());
    QVERIFY(!m_net.predict(x, m_net.levels() - 1, req).empty());   // pure noise is a level

    // What PyTorch returns for the same input (export_cell_diffusion.py --reference): the noise of run dif2, step 30 000
    // (the first recipe), v of runs dif4, step 20 000 and dif6, step 5 000 (the second: tokens of the request, scaled
    // blocks, two heads).
    double wantMean = 0.0, wantRms = 0.0;
    std::vector<Probe> probes;
    if (m_net.name() == "dif2 step 30000") {
        wantMean = 0.233647;
        wantRms = 0.701981;
        probes = { { 0, 0, 0, 0, 0.726715f }, { 1, 1, 2, 3, -0.145744f }, { 2, 15, 0, 7, -1.359847f }, { 3, 8, 8, 8, 1.946858f },
                   { 0, 5, 11, 2, 1.017959f }, { 3, 15, 15, 15, 0.031219f }, { 1, 0, 9, 14, 0.425822f }, { 2, 7, 3, 12, -1.331640f } };
    } else if (m_net.name() == "dif4 step 20000") {
        wantMean = 0.196302;
        wantRms = 0.746012;
        probes = { { 0, 0, 0, 0, 0.687779f }, { 1, 1, 2, 3, -0.917318f }, { 2, 15, 0, 7, -1.512231f }, { 3, 8, 8, 8, 0.622397f },
                   { 0, 5, 11, 2, 0.360908f }, { 3, 15, 15, 15, 0.316170f }, { 1, 0, 9, 14, 1.487716f }, { 2, 7, 3, 12, -1.014801f } };
    } else if (m_net.name() == "dif6 step 5000") {
        wantMean = 0.160204;
        wantRms = 0.945706;
        probes = { { 0, 0, 0, 0, 0.256296f }, { 1, 1, 2, 3, -0.901766f }, { 2, 15, 0, 7, -1.331236f }, { 3, 8, 8, 8, 1.199171f },
                   { 0, 5, 11, 2, -0.421643f }, { 3, 15, 15, 15, -0.098989f }, { 1, 0, 9, 14, -0.172676f }, { 2, 7, 3, 12, -1.253155f } };
    } else {
        QSKIP("the reference numbers are those of dif2 step 30000, dif4 step 20000 and dif6 step 5000");
    }
    double sum = 0.0, sq = 0.0;
    for (float v : e) { sum += v; sq += double(v) * v; }
    const double mean = sum / double(e.size()), rms = std::sqrt(sq / double(e.size()));
    QVERIFY2(std::abs(mean - wantMean) < 1e-4, qPrintable(QString("mean %1").arg(mean, 0, 'f', 6)));
    QVERIFY2(std::abs(rms - wantRms) < 1e-4, qPrintable(QString("rms %1").arg(rms, 0, 'f', 6)));
    const double worst = worstProbe(m_net, e, probes);
    QVERIFY2(worst < 1e-3, qPrintable(QString("largest difference to PyTorch at the probes %1").arg(worst, 0, 'g', 3)));
}

void TestCellDiffusion::testShortSamplingMatchesPyTorch()
{
    NEED_WEIGHTS();
    // The sampler of the second recipe against cellgen diffusion.sample: 3 steps (levels 999, 500, 0) that start from
    // the reference input in place of noise, every step asked with the request and with its targets hidden (guidance 3).
    double wantMean = 0.0, wantRms = 0.0;
    std::vector<Probe> probes;
    if (m_net.name() == "dif4 step 20000") {
        wantMean = -0.027890;
        wantRms = 0.569126;
        probes = { { 0, 0, 0, 0, -0.361767f }, { 1, 1, 2, 3, 1.248525f }, { 2, 15, 0, 7, 0.700138f }, { 3, 8, 8, 8, 0.370675f },
                   { 0, 5, 11, 2, 0.049828f }, { 3, 15, 15, 15, -0.877734f }, { 1, 0, 9, 14, -0.875392f }, { 2, 7, 3, 12, 0.260390f } };
    } else if (m_net.name() == "dif6 step 5000") {
        wantMean = 0.032778;
        wantRms = 0.961168;
        probes = { { 0, 0, 0, 0, -0.053372f }, { 1, 1, 2, 3, 1.096515f }, { 2, 15, 0, 7, 0.715726f }, { 3, 8, 8, 8, 0.336829f },
                   { 0, 5, 11, 2, -0.066620f }, { 3, 15, 15, 15, 0.320166f }, { 1, 0, 9, 14, -0.185813f }, { 2, 7, 3, 12, -0.175714f } };
    } else {
        QSKIP("the reference numbers are those of dif4 step 20000 and dif6 step 5000");
    }
    QCOMPARE(m_net.guidance(), 3.0f);
    const std::vector<float> code = m_net.sample(referenceRequest(m_net), referenceInput(m_net), {}, 3);
    QCOMPARE(code.size(), m_net.codeLength());
    double sum = 0.0, sq = 0.0;
    for (float v : code) { sum += v; sq += double(v) * v; }
    const double mean = sum / double(code.size()), rms = std::sqrt(sq / double(code.size()));
    QVERIFY2(std::abs(mean - wantMean) < 2e-4, qPrintable(QString("mean %1").arg(mean, 0, 'f', 6)));
    QVERIFY2(std::abs(rms - wantRms) < 2e-4, qPrintable(QString("rms %1").arg(rms, 0, 'f', 6)));
    const double worst = worstProbe(m_net, code, probes);
    QVERIFY2(worst < 2e-3, qPrintable(QString("largest difference to PyTorch at the probes %1").arg(worst, 0, 'g', 3)));
}

void TestCellDiffusion::testStartNoiseIsReproducible()
{
    NEED_WEIGHTS();
    const std::vector<float> a = m_net.startNoise(7), b = m_net.startNoise(7), c = m_net.startNoise(8);
    QCOMPARE(a.size(), m_net.codeLength());
    QVERIFY(a == b);
    QVERIFY(a != c);
    double sq = 0.0;
    for (float v : a) { QVERIFY(std::isfinite(v)); sq += double(v) * v; }
    const double rms = std::sqrt(sq / double(a.size()));
    QVERIFY2(rms > 0.5 && rms < 2.0, qPrintable(QString("rms %1").arg(rms)));   // unit variance, plus the shared number per channel
}

void TestCellDiffusion::testShortSamplingStopsAndRepeats()
{
    NEED_WEIGHTS();
    const std::vector<float> req = referenceRequest(m_net), noise = m_net.startNoise(7);
    // Three steps instead of the file's 50: the sampler's path, not the quality of the code.
    std::vector<int> seen;
    int total = 0;
    const std::vector<float> code = m_net.sample(req, noise, [&](int step, int steps) { seen.push_back(step); total = steps; return true; }, 3);
    QCOMPARE(code.size(), m_net.codeLength());
    QCOMPARE(seen, (std::vector<int>{ 0, 1, 2 }));
    QCOMPARE(total, 3);
    for (float v : code) QVERIFY(std::isfinite(v) && std::abs(v) <= m_net.clamp());
    QVERIFY(m_net.sample(req, noise, {}, 3) == code);                // the same request and noise: the same code
    QVERIFY(m_net.sample(req, m_net.startNoise(8), {}, 3) != code);  // another noise: another code
    // The progress callback stops the sampling, and nothing comes back.
    int calls = 0;
    QVERIFY(m_net.sample(req, noise, [&](int, int) { return ++calls < 2; }, 3).empty());
    QCOMPARE(calls, 2);
    // Wrong sizes are refused.
    QVERIFY(m_net.sample(req, std::vector<float>(noise.size() - 1), {}, 3).empty());
    QVERIFY(m_net.sample(std::vector<float>(req.size() - 1), noise, {}, 3).empty());
}

void TestCellDiffusion::testBrokenFilesAreRefused()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto write = [&](const char* name, const QByteArray& bytes) {
        const QString path = dir.filePath(QLatin1String(name));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size()) return std::string();
        return path.toStdString();
    };
    auto refused = [](const std::string& path) {
        CellDiffusion d;
        std::string error;
        if (d.open(path, &error) || d.opened() || error.empty()) return false;
        // A file that was not opened gives no request, no noise and no code.
        return !d.load() && d.request({ { "density", 0.3 } }, 2.0, 0.38, 1.0).empty() && d.startNoise(1).empty()
               && d.sample({}, {}).empty();
    };
    QVERIFY(refused(dir.filePath(QStringLiteral("absent.bin")).toStdString()));
    QVERIFY(refused(write("empty.bin", QByteArray())));
    QVERIFY(refused(write("garbage.bin", QByteArray(4096, 'x'))));
    QVERIFY(refused(write("magic.bin", QByteArray("MVCELLF1"))));
    QVERIFY(refused(write("zeros.bin", QByteArray("MVCELLF1") + QByteArray(4096, '\0'))));
    // The weights of the decoder are not a diffusion file.
    QFile decoder(QStringLiteral(":/models/cell_decoder.bin"));
    QVERIFY(decoder.open(QIODevice::ReadOnly));
    QVERIFY(refused(write("decoder.bin", decoder.readAll())));

    // The real file cut short: in the header, right after it, and one byte before its end; one byte too many.
    NEED_WEIGHTS();
    QFile real(m_file);
    QVERIFY(real.open(QIODevice::ReadOnly));
    const QByteArray bytes = real.readAll();
    QVERIFY(bytes.size() > 100000);
    QVERIFY(refused(write("cut_header.bin", bytes.left(100))));
    QVERIFY(refused(write("cut_weights.bin", bytes.left(100000))));
    QVERIFY(refused(write("cut_last.bin", bytes.left(bytes.size() - 1))));
    QVERIFY(refused(write("one_more.bin", bytes + QByteArray(1, '\0'))));
}

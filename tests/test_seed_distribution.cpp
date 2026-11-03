#include "test_seed_distribution.h"
#include <QTest>
#include <QDebug>
#include <cmath>
#include <cstdint>
#include <memory>
#include <set>
#include <tuple>
#include "parameters.h"
#include "parent_algorithm.h"
#include "algorithmfactory.h"
#include "lowdiscrepancy.hpp"

namespace {

// Builds the named algorithm on a fresh grid and runs its Initialization(),
// which is where the crystallization seeds are placed.
std::shared_ptr<Parent_Algorithm> initialize(const QString& name, int size, int points)
{
    Parameters& params = *Parameters::instance();
    params.setSize(size);
    params.setPoints(points);
    params.setAlgorithm(name);
    auto algo = AlgorithmFactory::instance().createAlgorithm(name, params);
    if (!algo) return nullptr;
    algo->Allocate_Memory();
    algo->Initialization(params.getIsWaveGeneration());
    return algo;
}

void release(const std::shared_ptr<Parent_Algorithm>& algo)
{
    algo->CleanUp();
    Parent_Algorithm::Delete3D(algo->getVoxels());
}

// FNV-1a over the ordered seed list.
uint64_t fingerprint(const std::vector<Parent_Algorithm::Coordinate>& seeds)
{
    uint64_t h = 1469598103934665603ULL;
    auto mix = [&h](int32_t v) {
        for (int b = 0; b < 4; ++b) {
            h ^= static_cast<uint8_t>((static_cast<uint32_t>(v) >> (8 * b)) & 0xFFu);
            h *= 1099511628211ULL;
        }
    };
    for (const auto& c : seeds) { mix(c.x); mix(c.y); mix(c.z); }
    return h;
}

std::vector<std::array<int, 3>> toArrays(const std::vector<Parent_Algorithm::Coordinate>& seeds)
{
    std::vector<std::array<int, 3>> out;
    for (const auto& c : seeds) out.push_back({c.x, c.y, c.z});
    return out;
}

// Coefficient of variation of the grain volumes (voxel count per grain id).
double grainVolumeCV(int32_t*** voxels, int n, int numGrains)
{
    std::vector<double> vol(static_cast<size_t>(numGrains) + 1, 0.0);
    for (int x = 0; x < n; ++x)
        for (int y = 0; y < n; ++y)
            for (int z = 0; z < n; ++z) {
                const int id = voxels[x][y][z];
                if (id > 0 && id <= numGrains) vol[id] += 1.0;
            }
    double mean = 0.0;
    for (int g = 1; g <= numGrains; ++g) mean += vol[g];
    mean /= numGrains;
    double var = 0.0;
    for (int g = 1; g <= numGrains; ++g) var += (vol[g] - mean) * (vol[g] - mean);
    var /= numGrains;
    return std::sqrt(var) / mean;
}

} // namespace

void TestSeedDistribution::init()
{
    Parameters::resetDefaults();
}

void TestSeedDistribution::cleanup()
{
    Parameters::resetDefaults();
}

void TestSeedDistribution::testDefaults()
{
    QCOMPARE(Parameters::seed_method, Parameters::Random);
    QCOMPARE(Parameters::seed_sigma, 0.3);
}

void TestSeedDistribution::testRandomSeedsUnchanged_data()
{
    QTest::addColumn<QString>("algorithm");
    QTest::addColumn<bool>("thinLayer");
    QTest::addColumn<bool>("wave");
    QTest::addColumn<quint64>("expected");

    // Fingerprints recorded on qml-development (7c19501) before seed_method
    // existed: seed 12345, 20^3 grid, 50 points.
    QTest::newRow("Voronoi")           << "Voronoi"      << false << false << quint64(4050609364494010151ULL);
    QTest::newRow("Polycrystall")      << "Polycrystall" << false << false << quint64(4050609364494010151ULL);
    QTest::newRow("Polycrystall thin") << "Polycrystall" << true  << false << quint64(4857445111754592697ULL);
    QTest::newRow("Probability")       << "Probability"  << false << false << quint64(4050609364494010151ULL);
    QTest::newRow("Probability wave")  << "Probability"  << false << true  << quint64(6462749697163791670ULL);
    QTest::newRow("DLCA")              << "DLCA"         << false << false << quint64(4050609364494010151ULL);
}

void TestSeedDistribution::testRandomSeedsUnchanged()
{
    QFETCH(QString, algorithm);
    QFETCH(bool, thinLayer);
    QFETCH(bool, wave);
    QFETCH(quint64, expected);

    Parameters::seed = 12345;
    Parameters::instance()->setIsThinLayer(thinLayer);
    if (wave) {
        Parameters::is_wave_generation = true;
        Parameters::initial_nuclei_count = 7;
    }

    auto algo = initialize(algorithm, 20, 50);
    QVERIFY(algo);
    QCOMPARE(quint64(fingerprint(algo->seedPoints)), expected);
    release(algo);
}

void TestSeedDistribution::testSeedVoxelsUniqueAndDeterministic()
{
    const int n = 500;
    const int size = 16;
    for (double sigma : {0.0, 0.3, 1.0}) {
        const auto a = qmc::seedVoxels(n, size, 7, sigma);
        QCOMPARE(static_cast<int>(a.size()), n);
        std::set<std::tuple<int, int, int>> uniq;
        for (const auto& v : a) {
            for (int d = 0; d < 3; ++d) {
                QVERIFY(v[d] >= 0);
                QVERIFY(v[d] < size);
            }
            uniq.insert({v[0], v[1], v[2]});
        }
        QCOMPARE(static_cast<int>(uniq.size()), n);

        QVERIFY(qmc::seedVoxels(n, size, 7, sigma) == a);
        QVERIFY(qmc::seedVoxels(n, size, 8, sigma) != a);
    }
}

void TestSeedDistribution::testSobolAlgorithms_data()
{
    QTest::addColumn<QString>("algorithm");
    QTest::newRow("Voronoi")      << "Voronoi";
    QTest::newRow("Polycrystall") << "Polycrystall";
    QTest::newRow("Probability")  << "Probability";
    QTest::newRow("DLCA")         << "DLCA";
}

void TestSeedDistribution::testSobolAlgorithms()
{
    QFETCH(QString, algorithm);
    const int size = 20;
    const int n = 50;
    Parameters::instance()->setSeedMethod(Parameters::Sobol);
    Parameters::instance()->setSeedSigma(0.3);

    Parameters::seed = 12345;
    auto a = initialize(algorithm, size, n);
    QVERIFY(a);
    const auto seedsA = toArrays(a->seedPoints);
    int32_t*** vox = a->getVoxels();

    // The algorithm places exactly qmc::seedVoxels' points, in its (x, y, z)
    // order, and grain i is seeded at point i.
    QVERIFY(seedsA == qmc::seedVoxels(n, size, 12345, 0.3));
    std::set<std::tuple<int, int, int>> uniq;
    for (size_t i = 0; i < seedsA.size(); ++i) {
        const auto& s = seedsA[i];
        for (int d = 0; d < 3; ++d) {
            QVERIFY(s[d] >= 0);
            QVERIFY(s[d] < size);
        }
        QCOMPARE(vox[s[0]][s[1]][s[2]], static_cast<int32_t>(i + 1));
        uniq.insert({s[0], s[1], s[2]});
    }
    QCOMPARE(static_cast<int>(uniq.size()), n);
    release(a);

    Parameters::seed = 12345;
    auto b = initialize(algorithm, size, n);
    QVERIFY(toArrays(b->seedPoints) == seedsA);
    release(b);

    Parameters::seed = 54321;
    auto c = initialize(algorithm, size, n);
    QVERIFY(toArrays(c->seedPoints) != seedsA);
    release(c);
}

void TestSeedDistribution::testSobolLowersGrainSizeScatter()
{
    const int size = 64;
    const int n = 64;
    const int runs = 10;

    auto meanCV = [&](Parameters::SeedMethod method) {
        Parameters::instance()->setSeedMethod(method);
        Parameters::instance()->setSeedSigma(0.0);
        double sum = 0.0;
        for (int s = 1; s <= runs; ++s) {
            Parameters::seed = static_cast<unsigned int>(s);
            auto algo = initialize("Voronoi", size, n);
            algo->Generate_To_End();
            sum += grainVolumeCV(algo->getVoxels(), size, algo->getNumColors());
            release(algo);
        }
        return sum / runs;
    };

    const double cvRandom = meanCV(Parameters::Random);
    const double cvSobol  = meanCV(Parameters::Sobol);
    qInfo().noquote() << QString("Voronoi n=%1 size=%2, mean grain-volume CV over %3 seeds: "
                                 "Random %4, Sobol(sigma=0) %5")
                             .arg(n).arg(size).arg(runs)
                             .arg(cvRandom, 0, 'f', 4).arg(cvSobol, 0, 'f', 4);
    QVERIFY(cvSobol < cvRandom);
}

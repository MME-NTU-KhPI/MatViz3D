#include "cellstiffness.h"

#include <chrono>
#include <cmath>
#include <exception>
#include <limits>

#include "fft_solver_session.hpp"
#include "parameters.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace cellstiff {

namespace {

// Thrown from the solver's iteration callback to leave a solve. The callback is called from the serial part
// of the iteration and the solver keeps its fields in containers, so unwinding through it is clean.
struct Stop {};

struct Pass {
    propsearch::Mat6 C{};
    double share = 1.0;
    int iterations = 0;
    bool converged = true;
};

// The six unit loads with one void stiffness: StressAnalysisFFT::computeStiffnessMatrix with the settings of
// the dataset, and its solid share of the strain energy.
Pass solvePass(const std::vector<int>& grain, int n, const fftsa::Mat6& base, double voidEta, const Settings& s,
               int pass, const Progress& progress, const std::atomic<bool>* cancel)
{
    // One solid in the axes of the cell (the law's weak direction is z, the build direction).
    const std::vector<std::array<double, 3>> orientation(2, std::array<double, 3>{ 0.0, 0.0, 0.0 });   // the void slot and grain 1
    fftsa::FFTSolverSession session(n, n, n, grain, orientation, base, voidEta);
    session.set_tolerance(s.tol);
    session.set_max_iters(s.maxIter);
    session.set_scheme(fftsa::FFTSolverSession::Scheme::Auto);   // conjugate gradients as soon as there is a void
    session.set_discretization(ffth::FFTHomogenizer::Discretization::Rotated);

    // The solid's stiffness in Mandel form, pipeline order (the session keeps the shear as yz, xz, xy).
    const fftsa::Mat6& mandel = session.grain_stiffness_mandel(1);
    static const int perm[6] = { 0, 1, 2, 5, 3, 4 };
    double Cs[6][6];
    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < 6; ++b) Cs[a][b] = mandel[size_t(perm[a])][size_t(perm[b])];

    const double voxels = double(n) * double(n) * double(n);
    const double r2 = std::sqrt(2.0);
    Pass out;
    fftsa::Mat6 C{};
    for (int j = 0; j < 6; ++j) {
        if (cancel && cancel->load()) throw Stop{};
        fftsa::Vec6 e{};
        e[size_t(j)] = (j >= 3) ? 0.5 : 1.0;   // unit engineering strain (the shear is passed as a tensor component)
        std::vector<fftsa::Vec6> strain;       // per solid voxel, engineering
        const auto step = session.solveLoadCaseFull(e, strain, [&](int iteration, double error) {
            if (cancel && cancel->load()) throw Stop{};
            if (progress) progress(pass, j, iteration + 1, error);
        });
        for (int i = 0; i < 6; ++i) C[size_t(i)][size_t(j)] = step.macro_stress[size_t(i)];
        out.iterations += step.iterations;
        if (!(step.error < s.tol)) out.converged = false;

        // Solid share of the strain energy of this load: the sum over the solid of eps : C_s : eps against
        // C[j][j] (Hill: they are equal when the void holds no energy). The same number as the diagonal of
        // FFTSolverSession::sensitivity_tensor contracted with C_s, which the dataset stores.
        double energy = 0.0;
        for (const fftsa::Vec6& g : strain) {
            const double a[6] = { g[0], g[1], g[2], g[3] / r2, g[4] / r2, g[5] / r2 };
            for (int p = 0; p < 6; ++p) {
                double row = 0.0;
                for (int q = 0; q < 6; ++q) row += Cs[p][q] * a[q];
                energy += a[p] * row;
            }
        }
        const double cjj = step.macro_stress[size_t(j)];
        const double share = std::abs(cjj) > 0.0 ? energy / voxels / cjj : 0.0;
        if (j == 0 || share < out.share) out.share = share;
    }
    fftsa::symmetrize_pipeline_C(C);
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) out.C[size_t(i)][size_t(j)] = C[size_t(i)][size_t(j)];
    return out;
}

} // namespace

std::vector<uint8_t> occupancy(int32_t*** voxels, int n)
{
    std::vector<uint8_t> solid(size_t(n) * size_t(n) * size_t(n), 0);
    for (int ix = 0; ix < n; ++ix)
        for (int iy = 0; iy < n; ++iy)
            for (int iz = 0; iz < n; ++iz)
                solid[(size_t(iz) * size_t(n) + size_t(iy)) * size_t(n) + size_t(ix)] = voxels[ix][iy][iz] != 0 ? 1 : 0;
    return solid;
}

Solution solve(const std::vector<uint8_t>& solid, int n, const propsearch::Law& material, const Settings& settings,
               const Progress& progress, const FirstPass& firstPass, const std::atomic<bool>* cancel)
{
    Solution out;
    const auto start = std::chrono::steady_clock::now();
    auto elapsed = [start]() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(); };

    if (n < 2 || solid.size() != size_t(n) * size_t(n) * size_t(n)) {
        out.error = "the cell is not a cube of voxels";
        return out;
    }
    std::vector<int> grain(solid.size(), 0);
    size_t filled = 0;
    for (size_t i = 0; i < solid.size(); ++i)
        if (solid[i]) { grain[i] = 1; ++filled; }
    out.density = double(filled) / double(solid.size());
    if (filled == 0) {
        out.error = "the cell has no material";
        return out;
    }
    double law[6][6];
    if (!Parameters::lawStiffnessPa(material.eGpa, material.nu, material.aniso, law)) {
        out.error = "the material is not a valid elastic law";
        return out;
    }
    fftsa::Mat6 base{};
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j) base[size_t(i)][size_t(j)] = law[i][j];

#ifdef _OPENMP
    if (settings.threads > 0) omp_set_num_threads(settings.threads);
#endif

    try {
        const Pass first = solvePass(grain, n, base, settings.voidEta, settings, 0, progress, cancel);
        out.C = first.C;
        out.share = out.shareFirst = first.share;
        out.iterations = first.iterations;
        out.converged = first.converged;
        out.voidEta = settings.voidEta;
        out.passes = 1;
        out.ok = true;
    } catch (const Stop&) {
        out.cancelled = true;
    } catch (const std::exception& e) {
        out.error = e.what();
    }
    out.seconds = elapsed();
    if (!out.ok) return out;

    // The dataset's rule: the void is propping the cell up in some load, solve again with a softer one.
    const bool again = filled < solid.size() && settings.voidEtaFallback > 0.0 && settings.voidEtaFallback < settings.voidEta
                       && !(out.share >= settings.minSolidShare);
    if (!again) return out;
    if (firstPass) firstPass(out);
    try {
        const Pass second = solvePass(grain, n, base, settings.voidEtaFallback, settings, 1, progress, cancel);
        out.C = second.C;
        out.share = second.share;
        out.iterations += second.iterations;
        out.converged = second.converged;
        out.voidEta = settings.voidEtaFallback;
        out.passes = 2;
    } catch (const Stop&) {
        out.cancelled = true;      // the first solve's stiffness stays
    } catch (const std::exception& e) {
        out.cancelled = true;
        out.error = e.what();
    }
    out.seconds = elapsed();
    return out;
}

std::vector<Row> compare(const std::map<std::string, double>& asked, double askedDensity,
                         const propsearch::Values* obtained, double density)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    auto deviation = [nan](const Row& r) {
        if (!std::isfinite(r.obtained) || !std::isfinite(r.asked)) return nan;
        if (!r.relative) return r.obtained - r.asked;
        return r.asked != 0.0 ? 100.0 * (r.obtained - r.asked) / std::abs(r.asked) : nan;
    };

    std::vector<Row> rows;
    for (int t = 0; t < propsearch::TargetCount; ++t) {
        const auto it = asked.find(propsearch::targetKey(t).toStdString());
        if (it == asked.end()) continue;
        Row r;
        r.key = it->first;
        r.modulus = propsearch::isModulus(t);
        r.relative = r.modulus || propsearch::isRatio(t);
        r.asked = it->second;
        r.obtained = obtained ? (*obtained)[size_t(t)] * (r.modulus ? 1e-6 : 1.0) : nan;
        r.deviation = deviation(r);
        rows.push_back(r);
    }
    if (!std::isnan(askedDensity)) {
        Row r;
        r.key = "density";
        r.asked = askedDensity;
        r.obtained = density;
        r.deviation = deviation(r);
        rows.push_back(r);
    }
    return rows;
}

} // namespace cellstiff

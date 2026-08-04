// ============================================================================
//  solver_compare_main.cpp   —  console test harness: ANSYS vs FFT solver
//
//  Generates ONE polycrystalline microstructure, applies a battery of macro
//  strain (eps) tensors, solves each with BOTH solvers via the existing
//  single-shot entry points, and reports component-by-component agreement.
//
//      StressAnalysisFFT::solveSingleLoadCase(...)     (Moulinec–Suquet FFT)
//      StressAnalysis   ::solveSingleLoadCase(...)     (ANSYS, external solve)
//
//  Both return SingleShotResult.macro_stress[6] in pipeline order
//      [ sx, sy, sz, sxy, syz, sxz ]  (Pa),
//  and take eps[6] in pipeline order [exx,eyy,ezz,exy,eyz,exz] with TENSOR
//  shear.  We hand BOTH the identical eps[] and the identical microstructure,
//  and (because both call the shared buildGrainOrientations(nGrains, seed))
//  the identical per-grain orientations.  So this is a true apples-to-apples
//  check, and any residual gap is a physics/convention difference to explain
//  or a bug to fix.
//
//  Build:   qmake solver_compare.pro && make        (see that file)
//  Run:     ./solver_compare  [options]             (see usage() below)
//
//  NOTE — two *expected* modeling differences that this harness measures and
//  reports rather than hides:
//    (A) Boundary conditions.  The FFT solver is intrinsically PERIODIC.
//        ANSYS::solveSingleLoadCase applies affine displacements on every
//        boundary node (applyComplexLoads = KUBC, kinematic-uniform BC), which
//        is *stiffer* than periodic and does NOT converge to it under mesh
//        refinement.  For a matched comparison, ANSYS should use
//        applyPeriodicBC (already implemented in ansyswrapper) instead — see
//        the report printed at the end.
//    (B) Solid fraction.  FFT averages stress over ALL voxels (voids ~0), i.e.
//        returns RVE-level stress = solid_fraction * <sigma>_solid.  ANSYS's
//        single-shot averages SOLID elements only and does NOT rescale.  With
//        voids present the two differ by exactly the porosity.  This harness
//        therefore builds a FULLY DENSE microstructure (every voxel is a
//        grain, id >= 1) so solid_fraction == 1 and (B) drops out.
// ============================================================================

#include "stressanalysis.h"       // ANSYS single-shot
#include "stressanalysis_fft.h"   // FFT single-shot
#include "parameters.h"           // Parameters::seed / voxels / etc.
#include "fft_solver_session.hpp" // FFTSolverSession (effective-C self-check)
#include "stressresult.h"         // buildGrainOrientations, vonMisesPipeline

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTextStream>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  3D array allocation — identical contiguous layout to
//  Parent_Algorithm::Create3D, so the solvers see exactly the memory contract
//  they expect (voxels[ix][iy][iz], one contiguous block underneath).
// ─────────────────────────────────────────────────────────────────────────────
static int32_t*** create3D(int N)
{
    int32_t*** a = new int32_t**[N];
    a[0]        = new int32_t*[static_cast<size_t>(N) * N];
    a[0][0]     = new int32_t[static_cast<size_t>(N) * N * N];
    for (int i = 0; i < N; ++i) {
        if (i < N - 1) {
            a[0][(i + 1) * N] = &(a[0][0][static_cast<size_t>(i + 1) * N * N]);
            a[i + 1]          = &(a[0][(i + 1) * N]);
        }
        for (int j = 0; j < N; ++j)
            if (j > 0) a[i][j] = a[i][j - 1] + N;
    }
    return a;
}
static void delete3D(int32_t*** a)
{
    delete[] a[0][0];
    delete[] a[0];
    delete[] a;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fully-dense periodic Voronoi polycrystal.  Deterministic in `seed_struct`.
//  Grain ids are compacted to 1..G (no gaps, no empty grains) so that BOTH
//  solvers scan the same nGrains and index buildGrainOrientations() the same.
// ─────────────────────────────────────────────────────────────────────────────
static int fillVoronoi(int32_t*** vox, int N, int nSeeds, unsigned int seed_struct)
{
    std::mt19937 rng(seed_struct);
    std::uniform_int_distribution<int> coord(0, N - 1);

    std::vector<std::array<int, 3>> seeds(nSeeds);
    for (auto& s : seeds) s = { coord(rng), coord(rng), coord(rng) };

    auto pdist2 = [N](int a, int b) {           // periodic 1D squared distance
        int d = std::abs(a - b);
        d = std::min(d, N - d);
        return d * d;
    };

    for (int ix = 0; ix < N; ++ix)
        for (int iy = 0; iy < N; ++iy)
            for (int iz = 0; iz < N; ++iz) {
                int best = 0, bestd = INT32_MAX;
                for (int g = 0; g < nSeeds; ++g) {
                    const int d = pdist2(ix, seeds[g][0])
                                + pdist2(iy, seeds[g][1])
                                + pdist2(iz, seeds[g][2]);
                    if (d < bestd) { bestd = d; best = g; }
                }
                vox[ix][iy][iz] = best + 1;      // provisional id 1..nSeeds
            }

    // Compact ids so they are exactly 1..G with no empties.
    std::vector<int> remap(nSeeds + 1, 0);
    int next = 1;
    for (int ix = 0; ix < N; ++ix)
        for (int iy = 0; iy < N; ++iy)
            for (int iz = 0; iz < N; ++iz) {
                int& id = vox[ix][iy][iz];
                if (remap[id] == 0) remap[id] = next++;
                id = remap[id];
            }
    return next - 1;                              // actual grain count G
}

// ─────────────────────────────────────────────────────────────────────────────
//  crystallization_seeds.csv  — REQUIRED by ansysWrapper::createFEfromArray8Node.
//
//  That routine opens this file (relative to the process working directory) and,
//  if it is missing, warns and returns BEFORE emitting any nodes/elements — the
//  ANSYS deck then has loads but no mesh -> "There are no nodes defined."
//
//  The file must contain a header line (discarded on read) followed by exactly
//  G data rows "x,y,z".  Row count MUST equal the grain count: grain g is bound
//  to coordinate system id (g+11), created one-per-row, so a short/long file
//  mis-binds orientations.  The coordinates themselves are only the local-CS
//  ORIGIN (LOCAL,cs,0,x,y,z,...) and do not affect the rotated stiffness, so we
//  use each grain's voxel centroid — physical and guaranteed in-domain.
// ─────────────────────────────────────────────────────────────────────────────
static bool writeSeedsCSV(int32_t*** vox, int N, int G, const QString& path)
{
    std::vector<long long> sx(G + 1, 0), sy(G + 1, 0), sz(G + 1, 0), cnt(G + 1, 0);
    for (int ix = 0; ix < N; ++ix)
        for (int iy = 0; iy < N; ++iy)
            for (int iz = 0; iz < N; ++iz) {
                const int g = vox[ix][iy][iz];
                if (g < 1 || g > G) continue;
                sx[g] += ix; sy[g] += iy; sz[g] += iz; ++cnt[g];
            }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        std::printf("  [WARN] could not write %s — ANSYS mesh generation will fail.\n",
                    path.toUtf8().constData());
        return false;
    }
    QTextStream out(&f);
    out << "x,y,z\n";                                   // header (reader skips it)
    for (int g = 1; g <= G; ++g) {
        const long long c = cnt[g] > 0 ? cnt[g] : 1;
        out << (int)std::llround((double)sx[g] / c) << ","
            << (int)std::llround((double)sy[g] / c) << ","
            << (int)std::llround((double)sz[g] / c) << "\n";
    }
    f.close();
    std::printf("  wrote %s (%d grain seeds) for ANSYS local-CS generation.\n",
                path.toUtf8().constData(), G);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Load-case battery.  Pipeline order [exx,eyy,ezz,exy,eyz,exz], TENSOR shear.
// ─────────────────────────────────────────────────────────────────────────────
struct LoadCase { const char* name; std::array<double, 6> e; };

static std::vector<LoadCase> makeLoadCases(double a)
{
    return {
        { "uniaxial-x",   { a, 0, 0, 0, 0, 0 } },
        { "uniaxial-y",   { 0, a, 0, 0, 0, 0 } },
        { "uniaxial-z",   { 0, 0, a, 0, 0, 0 } },
        { "shear-xy",     { 0, 0, 0, a, 0, 0 } },
        { "shear-yz",     { 0, 0, 0, 0, a, 0 } },
        { "shear-xz",     { 0, 0, 0, 0, 0, a } },
        { "hydrostatic",  { a, a, a, 0, 0, 0 } },
        { "biaxial-xy",   { a, a, 0, 0, 0, 0 } },
        { "mixed",        { a, -0.5 * a, 0.3 * a, 0.4 * a, -0.2 * a, 0.1 * a } },
    };
}

// ─────────────────────────────────────────────────────────────────────────────
//  Comparison helpers
// ─────────────────────────────────────────────────────────────────────────────
static double l2(const double* v, int n) {
    double s = 0; for (int i = 0; i < n; ++i) s += v[i] * v[i]; return std::sqrt(s);
}
static double relL2(const double* f, const double* r, int n) {
    double dn = 0, rn = 0;
    for (int i = 0; i < n; ++i) { const double d = f[i] - r[i]; dn += d * d; rn += r[i] * r[i]; }
    rn = std::sqrt(rn);
    return rn > 0 ? std::sqrt(dn) / rn : 0.0;
}

static const char* SC[6] = { "sx", "sy", "sz", "sxy", "syz", "sxz" };

// Print one component table for a single load case, comparing two 6-vectors.
static void printStressCompare(const double* fft, const double* ans, double scaleRef)
{
    std::printf("      %-5s %14s %14s %12s %10s\n", "comp", "FFT [MPa]", "ANSYS [MPa]", "abs [MPa]", "rel");
    for (int i = 0; i < 6; ++i) {
        const double d   = fft[i] - ans[i];
        const bool tiny  = std::abs(ans[i]) < 1e-2 * scaleRef;   // below 1% of case scale
        char relbuf[16];
        if (tiny) std::snprintf(relbuf, sizeof relbuf, "   ~0");
        else      std::snprintf(relbuf, sizeof relbuf, "%8.2f%%", 100.0 * d / std::abs(ans[i]));
        std::printf("      %-5s %14.4f %14.4f %12.4f %10s\n",
                    SC[i], fft[i] * 1e-6, ans[i] * 1e-6, d * 1e-6, relbuf);
    }
}

// Map a canonical load-case name to its effective-stiffness column index
// (unit strain e_j), or -1 if the case is not one of the six canonical ones.
// Pipeline eps order is [exx,eyy,ezz,exy,eyz,exz], so the column index equals
// the single non-zero strain slot.
static int canonicalColumn(const std::string& name)
{
    if (name == "uniaxial-x") return 0;
    if (name == "uniaxial-y") return 1;
    if (name == "uniaxial-z") return 2;
    if (name == "shear-xy")   return 3;
    if (name == "shear-yz")   return 4;
    if (name == "shear-xz")   return 5;
    return -1;
}

// Print a 6x6 stiffness (GPa) and return its worst asymmetry |C-C^T|.
static double printC(const char* title, const double C[6][6])
{
    std::printf("%s (GPa):\n", title);
    double sym = 0;
    for (int i = 0; i < 6; ++i) {
        std::printf("    ");
        for (int j = 0; j < 6; ++j) {
            std::printf("%9.3f ", C[i][j] * 1e-9);
            sym = std::max(sym, std::abs(C[i][j] - C[j][i]));
        }
        std::printf("\n");
    }
    return sym;
}

// Assemble effective C from the canonical single-shots already collected:
// column j = (macro stress under unit strain e_j) / strain_amplitude.
// Returns true if all six canonical columns were present.  This is the
// sharpest diagnostic: sign flips, index permutations, and the KUBC-vs-periodic
// stiffness bias each leave a distinct, localized fingerprint here — unlike von
// Mises, which squares away sign errors entirely.
template <class RowVec, class Getter>
static bool assembleC(const RowVec& rows, double amp, Getter get, double C[6][6])
{
    bool have[6] = { false, false, false, false, false, false };
    for (const auto& r : rows) {
        const int col = canonicalColumn(r.name);
        if (col < 0) continue;
        const double* s = nullptr;
        if (!get(r, s)) continue;               // solver didn't produce this case
        for (int i = 0; i < 6; ++i) C[i][col] = s[i] / amp;
        have[col] = true;
    }
    for (bool h : have) if (!h) return false;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
static void usage(const char* argv0)
{
    std::printf(
    "Usage: %s [options]\n"
    "  --n <int>        grid size N (RVE is NxNxN voxels)         [default 16]\n"
    "  --grains <int>   number of Voronoi grains                  [default 24]\n"
    "  --seed <uint>    RNG seed for grains AND orientations      [default 1]\n"
    "  --seed-struct <uint> RNG seed for the Voronoi geometry     [default = --seed]\n"
    "  --strain <float> macro strain amplitude a                  [default 1e-4]\n"
    "  --tol <float>    FFT convergence tolerance                 [default 1e-6]\n"
    "  --maxit <int>    FFT max iterations                        [default 5000]\n"
    "  --solver <s>     fft | ansys | both                        [default both]\n"
    "  --case <name>    run only one load case by name (see list) [default all]\n"
    "  --help\n", argv0);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);   // needed by ansysWrapper (QProcess etc.)

    // ── defaults ────────────────────────────────────────────────────────────
    int          N        = 16;
    int          nGrainsW = 24;
    unsigned int seed     = 1;
    bool         haveSeedStruct = false;
    unsigned int seedStruct = 1;
    double       strain   = 1e-4;
    double       tol      = 1e-6;
    int          maxit    = 5000;
    std::string  solver   = "both";
    std::string  onlyCase;

    // ── parse ───────────────────────────────────────────────────────────────
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        auto next = [&](const char* def) -> std::string {
            return (i + 1 < argc) ? std::string(argv[++i]) : std::string(def);
        };
        if      (k == "--n")           N        = std::stoi(next("16"));
        else if (k == "--grains")      nGrainsW = std::stoi(next("24"));
        else if (k == "--seed")        seed     = (unsigned)std::stoul(next("1"));
        else if (k == "--seed-struct") { seedStruct = (unsigned)std::stoul(next("1")); haveSeedStruct = true; }
        else if (k == "--strain")      strain   = std::stod(next("1e-4"));
        else if (k == "--tol")         tol      = std::stod(next("1e-6"));
        else if (k == "--maxit")       maxit    = std::stoi(next("5000"));
        else if (k == "--solver")      solver   = next("both");
        else if (k == "--case")        onlyCase = next("");
        else if (k == "--help")        { usage(argv[0]); return 0; }
        else { std::printf("unknown option: %s\n", k.c_str()); usage(argv[0]); return 1; }
    }
    if (!haveSeedStruct) seedStruct = seed;

    const bool doFFT   = (solver == "fft"   || solver == "both");
    const bool doANSYS = (solver == "ansys" || solver == "both");

    // ── microstructure (fully dense) ────────────────────────────────────────
    int32_t*** vox = create3D(N);
    const int G = fillVoronoi(vox, N, nGrainsW, seedStruct);

    // ── wire up the globals both solvers read ───────────────────────────────
    Parameters::seed             = seed;
    Parameters::num_threads      = 1;
    Parameters::voxels           = vox;
    Parameters::working_directory = QDir::currentPath() + "/solver_compare_work";
    QDir().mkpath(Parameters::working_directory);
    Parameters::instance()->setSize(N);
    Parameters::instance()->setPoints(G);

    std::printf("\n================ ANSYS vs FFT solver comparison ================\n");
    std::printf("  grid            : %d x %d x %d  (%d voxels)\n", N, N, N, N * N * N);
    std::printf("  grains (G)      : %d  (requested %d)\n", G, nGrainsW);
    std::printf("  seed (orient)   : %u\n", seed);
    std::printf("  seed (geometry) : %u\n", seedStruct);
    std::printf("  strain amplitude: %.3e\n", strain);
    std::printf("  FFT tol / maxit : %.1e / %d\n", tol, maxit);
    std::printf("  material (cubic): C11=168.4  C12=121.4  C44=75.4 GPa "
                "(Zener A = %.3f)\n", 2.0 * 75.4 / (168.4 - 121.4));
    std::printf("  solver(s)       : %s\n", solver.c_str());
    std::printf("================================================================\n");

    // ANSYS mesh generation reads this from the process working directory.
    if (doANSYS) {
        writeSeedsCSV(vox, N, G, QDir::currentPath() + "/crystallization_seeds.csv");
    }

    // ── FFT effective-C self-consistency check (no ANSYS needed) ────────────
    //    Independent of the per-case single-shots: confirms the FFT solver,
    //    its basis conversions, and linear superposition are internally sound,
    //    so that any FFT-vs-ANSYS gap below is attributable to the ANSYS side
    //    or to the BC/porosity modeling — not to a broken FFT path.
    if (doFFT) {
        int nG = 0;
        std::vector<int> field(static_cast<size_t>(N) * N * N);
        for (int ix = 0; ix < N; ++ix)
            for (int iy = 0; iy < N; ++iy)
                for (int iz = 0; iz < N; ++iz)
                    field[(static_cast<size_t>(iz) * N + iy) * N + ix] = vox[ix][iy][iz];
        for (int v : field) nG = std::max(nG, v);
        auto orient = buildGrainOrientations(nG, seed);
        fftsa::FFTSolverSession sess(N, N, N, field, orient);
        sess.set_tolerance(tol);
        sess.set_max_iters(maxit);
        double S[6][6], C[6][6];
        if (sess.computeCompliance(S, C)) {
            double sym = 0;
            for (int i = 0; i < 6; ++i)
                for (int j = 0; j < 6; ++j) sym = std::max(sym, std::abs(C[i][j] - C[j][i]));
            const double K = (C[0][0] + C[1][1] + C[2][2]
                            + 2 * (C[0][1] + C[0][2] + C[1][2])) / 9.0;
            std::printf("\n[FFT self-check] effective stiffness C (pipeline basis, GPa):\n");
            for (int i = 0; i < 6; ++i) {
                std::printf("    ");
                for (int j = 0; j < 6; ++j) std::printf("%9.3f ", C[i][j] * 1e-9);
                std::printf("\n");
            }
            std::printf("    symmetry max|C-C^T| = %.3e Pa   (expect ~0)\n", sym);
            std::printf("    bulk modulus K = %.3f GPa   vs analytic (C11+2C12)/3 = %.3f GPa\n",
                        K * 1e-9, (168.4e9 + 2 * 121.4e9) / 3.0 * 1e-9);
            std::printf("    ^ these two MUST match: hydrostatic response of a cubic\n"
                        "      aggregate is orientation-independent, so K is exact.\n");
        } else {
            std::printf("\n[FFT self-check] computeCompliance FAILED (singular C)\n");
        }
    }

    // ── per-load-case comparison ────────────────────────────────────────────
    auto cases = makeLoadCases(strain);

    struct Row {
        std::string name;
        double sf[6], sa[6];         // FFT / ANSYS macro stress
        double vmf = 0, vma = 0;     // von Mises
        int    fitr = 0; double ferr = 0;
        bool   fok = false, aok = false;
        double relStress = 0, relVM = 0;
    };
    std::vector<Row> rows;

    for (auto& lc : cases) {
        if (!onlyCase.empty() && onlyCase != lc.name) continue;

        Row row; row.name = lc.name;
        double eps[6]; for (int i = 0; i < 6; ++i) eps[i] = lc.e[i];

        std::printf("\n---- load case: %-12s eps = [%.2e %.2e %.2e | %.2e %.2e %.2e] ----\n",
                    lc.name, eps[0], eps[1], eps[2], eps[3], eps[4], eps[5]);

        if (doFFT) {
            StressAnalysisFFT fft;
            fft.fft_tol = tol; fft.fft_max_iter = maxit;   // C11/C12/C44 keep defaults
            SingleShotResult r = fft.solveSingleLoadCase((short)N, (short)G, vox, eps);
            row.fok = r.ok;
            if (r.ok) {
                std::memcpy(row.sf, r.macro_stress, sizeof row.sf);
                row.vmf = r.von_mises; row.fitr = r.iterations; row.ferr = r.error;
                if (r.error > tol)
                    std::printf("      [WARN] FFT did NOT reach tol (err=%.2e > tol=%.1e) "
                                "after %d iters — result under-converged.\n",
                                r.error, tol, r.iterations);
            } else {
                std::printf("      [FFT] FAILED: %s\n", r.errorMessage.toUtf8().constData());
            }
        }

        if (doANSYS) {
            StressAnalysis ans;
            SingleShotResult r = ans.solveSingleLoadCase((short)N, (short)G, vox, eps);
            row.aok = r.ok;
            if (r.ok) {
                std::memcpy(row.sa, r.macro_stress, sizeof row.sa);
                row.vma = r.von_mises;
            } else {
                std::printf("      [ANSYS] FAILED: %s\n", r.errorMessage.toUtf8().constData());
                std::printf("      (ANSYS executable not found / licence? FFT results still valid.)\n");
            }
        }

        if (row.fok && row.aok) {
            const double scaleRef = std::max(l2(row.sa, 6), l2(row.sf, 6));
            printStressCompare(row.sf, row.sa, scaleRef);
            row.relStress = relL2(row.sf, row.sa, 6);
            row.relVM     = (row.vma != 0) ? std::abs(row.vmf - row.vma) / std::abs(row.vma) : 0;
            std::printf("      von Mises: FFT=%.4f MPa  ANSYS=%.4f MPa  relΔ=%.3f%%\n",
                        row.vmf * 1e-6, row.vma * 1e-6, 100.0 * row.relVM);
            std::printf("      >>> relative L2 stress error = %.3f %%   (FFT iters=%d err=%.2e)\n",
                        100.0 * row.relStress, row.fitr, row.ferr);
        } else if (row.fok) {
            std::printf("      FFT macro stress [MPa]: "
                        "sx=%.3f sy=%.3f sz=%.3f sxy=%.3f syz=%.3f sxz=%.3f  (vM=%.3f)\n",
                        row.sf[0]*1e-6, row.sf[1]*1e-6, row.sf[2]*1e-6,
                        row.sf[3]*1e-6, row.sf[4]*1e-6, row.sf[5]*1e-6, row.vmf*1e-6);
            std::printf("      FFT iters=%d err=%.2e\n", row.fitr, row.ferr);
        }
        rows.push_back(row);
    }

    // ── summary ─────────────────────────────────────────────────────────────
    if (doFFT && doANSYS) {
        std::printf("\n================================ SUMMARY ================================\n");
        std::printf("  %-12s %14s %14s %10s %8s %10s\n",
                    "case", "vM FFT[MPa]", "vM ANSYS[MPa]", "relL2", "vM relΔ", "FFT err");
        int nComp = 0; double sumRel = 0, worst = 0; std::string worstCase;
        for (auto& r : rows) {
            if (r.fok && r.aok) {
                std::printf("  %-12s %14.4f %14.4f %9.3f%% %7.3f%% %10.2e\n",
                            r.name.c_str(), r.vmf * 1e-6, r.vma * 1e-6,
                            100.0 * r.relStress, 100.0 * r.relVM, r.ferr);
                sumRel += r.relStress; ++nComp;
                if (r.relStress > worst) { worst = r.relStress; worstCase = r.name; }
            } else if (r.fok) {
                std::printf("  %-12s %14.4f %14s %10s %8s %10.2e\n",
                            r.name.c_str(), r.vmf * 1e-6, "(no ANSYS)", "-", "-", r.ferr);
            }
        }
        if (nComp) {
            std::printf("  ----------------------------------------------------------------------\n");
            std::printf("  mean relative L2 stress error : %.3f %%\n", 100.0 * sumRel / nComp);
            std::printf("  worst case                    : %s (%.3f %%)\n",
                        worstCase.c_str(), 100.0 * worst);
        }
        std::printf("========================================================================\n");

        // ── effective-stiffness fingerprint (the sign/permutation/BC localizer) ──
        // Reuses the six canonical single-shots above — no extra solves.
        if (onlyCase.empty()) {
            double Cf[6][6] = {{0}}, Ca[6][6] = {{0}};
            const bool okF = assembleC(rows, strain,
                [](const Row& r, const double*& s){ if(!r.fok) return false; s=r.sf; return true; }, Cf);
            const bool okA = assembleC(rows, strain,
                [](const Row& r, const double*& s){ if(!r.aok) return false; s=r.sa; return true; }, Ca);

            if (okF && okA) {
                std::printf("\n===================== EFFECTIVE STIFFNESS COMPARE ======================\n");
                std::printf("(pipeline basis: shear columns/rows carry the 2G convention; read SIGNS\n");
                std::printf(" and locations, not von Mises — vM cannot see a flipped shear.)\n\n");
                const double symF = printC("C_FFT", Cf);
                std::printf("\n");
                const double symA = printC("C_ANSYS", Ca);

                // entry-wise diff, worst offender, and diagonal ratio ANSYS/FFT
                double worst = 0; int wi = 0, wj = 0; double scale = 0;
                for (int i = 0; i < 6; ++i)
                    for (int j = 0; j < 6; ++j)
                        scale = std::max(scale, std::abs(Ca[i][j]));
                for (int i = 0; i < 6; ++i)
                    for (int j = 0; j < 6; ++j) {
                        const double d = std::abs(Cf[i][j] - Ca[i][j]);
                        if (d > worst) { worst = d; wi = i; wj = j; }
                    }
                std::printf("\n  symmetry  |C-C^T|max : FFT=%.3e  ANSYS=%.3e Pa\n", symF, symA);
                std::printf("  worst entry diff     : (%d,%d)  FFT=%.3f  ANSYS=%.3f GPa  (Δ=%.3f)\n",
                            wi, wj, Cf[wi][wj]*1e-9, Ca[wi][wj]*1e-9, (Cf[wi][wj]-Ca[wi][wj])*1e-9);
                std::printf("  worst entry rel      : %.2f %% of max|C_ANSYS|\n", 100.0*worst/std::max(scale,1.0));
                std::printf("  diagonal ratio ANSYS/FFT (a uniform >1 = KUBC stiffening, not a bug):\n    ");
                for (int i = 0; i < 6; ++i)
                    std::printf("%s=%.3f  ", SC[i], (std::abs(Cf[i][i])>1?Ca[i][i]/Cf[i][i]:0.0));
                std::printf("\n");
                std::printf("  sign check (entries where FFT and ANSYS disagree in sign, |both|>1%% scale):\n");
                bool anySign = false;
                for (int i = 0; i < 6; ++i)
                    for (int j = 0; j < 6; ++j) {
                        const double f = Cf[i][j], a = Ca[i][j];
                        if (std::abs(f) > 0.01*scale && std::abs(a) > 0.01*scale && (f*a < 0)) {
                            std::printf("    (%d,%d): FFT=%.3f  ANSYS=%.3f GPa  <-- SIGN FLIP\n",
                                        i, j, f*1e-9, a*1e-9);
                            anySign = true;
                        }
                    }
                if (!anySign) std::printf("    none.\n");
                std::printf("========================================================================\n");
            } else if (okF) {
                std::printf("\n[C_FFT assembled — ANSYS columns incomplete, no compare]\n");
                double Conly[6][6]; std::memcpy(Conly, Cf, sizeof Conly);
                printC("C_FFT", Conly);
            }
        }

        std::printf(
        "\nRead the STIFFNESS table above, not the von Mises column, to spot bugs:\n"
        "  • a SIGN FLIP line  -> shear-sign / Bunge->ANSYS-LOCAL bug (bungeZXZtoAnsysZXY).\n"
        "  • a large off-diagonal Δ in the wrong slot -> index permutation.\n"
        "  • a roughly UNIFORM diagonal ratio ANSYS/FFT > 1 -> expected KUBC stiffening.\n"
        "\nIf the mean error is more than a couple of percent, the prime suspect is\n"
        "the BOUNDARY-CONDITION mismatch, not a solver bug:\n"
        "  • FFT  = periodic BC (built in).\n"
        "  • ANSYS::solveSingleLoadCase = applyComplexLoads = affine displacement\n"
        "    on every face (KUBC), which is stiffer than periodic and will read\n"
        "    HIGHER stresses, especially in shear and at small N / high anisotropy.\n"
        "  → For a matched check, switch the ANSYS single-shot to applyPeriodicBC\n"
        "    (already in ansyswrapper.h). Then KUBC↔periodic drops out and only\n"
        "    the FE-vs-voxel discretization gap remains (shrinks as N grows).\n"
        "\nTo isolate a suspected shear-sign / Bunge→ANSYS-LOCAL angle bug, collapse\n"
        "the polycrystal to one effective crystal and re-run:\n"
        "    MATVIZ_FORCE_ORIENT_DEG=30,0,0 ./solver_compare --solver both --case shear-xy\n"
        "With every grain identically oriented, FFT and (periodic-BC) ANSYS must\n"
        "agree to discretization error; a persistent shear-sign flip then points at\n"
        "bungeZXZtoAnsysZXY() / the APDL LOCAL setup rather than the FFT path.\n");
    }

    delete3D(vox);
    Parameters::voxels = nullptr;
    return 0;
}

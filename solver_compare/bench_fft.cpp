// ============================================================================
//  bench_fft.cpp  -  standalone OpenMP speedup benchmark for FFTHomogenizer.
//
//  Header-only, no Qt dependency (fft_homog.hpp doesn't need it), so this
//  builds directly with g++ in seconds -- no qmake/mingw32-make project
//  build required. Useful for quickly checking how OMP_NUM_THREADS affects
//  the FFT solver's per-iteration cost, independent of ANSYS/solver_compare's
//  own harness overhead (self-check, load-case battery, etc.).
//
//  Build (MinGW; M_PI needs _USE_MATH_DEFINES on MinGW's headers):
//      g++ -O2 -fopenmp -std=c++17 -D_USE_MATH_DEFINES -I .. bench_fft.cpp -o bench_fft.exe
//
//  Run (fixed iteration count via tol=0, so timings are apples-to-apples):
//      OMP_NUM_THREADS=1 ./bench_fft.exe 32 30
//      OMP_NUM_THREADS=8 ./bench_fft.exe 32 30
//
//  argv[1] = grid size N (cubic NxNxN RVE), argv[2] = iteration count.
// ============================================================================
#include "../fft_homog.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv) {
    const int N     = argc > 1 ? std::atoi(argv[1]) : 32;
    const int iters = argc > 2 ? std::atoi(argv[2]) : 50;

    ffth::FFTHomogenizer h(N, N, N);

    // Two-phase heterogeneous material (not a trivial uniform field), so the
    // Green-operator / local-stress work is representative of a real RVE.
    std::vector<int> phase(static_cast<size_t>(N) * N * N);
    for (size_t i = 0; i < phase.size(); ++i) phase[i] = (i % 7 == 0) ? 1 : 0;

    std::vector<ffth::Mat6> mats;
    mats.push_back(ffth::isotropic_C(200e9, 0.30));
    mats.push_back(ffth::isotropic_C(70e9, 0.33));
    h.set_materials(mats);
    h.set_phase_field(phase);

    h.set_tolerance(0.0);        // never converge early -> run exactly `iters`
    h.set_max_iterations(iters);

    ffth::Vec6 E{1e-4, 0, 0, 0, 0, 0};

    const auto t0 = std::chrono::steady_clock::now();
    int it = 0;
    h.solve(E, &it);
    const auto t1 = std::chrono::steady_clock::now();

    const double sec = std::chrono::duration<double>(t1 - t0).count();
    std::printf("N=%d  iters=%d  time=%.3fs  (%.2f ms/iter)\n", N, it, sec, 1000.0 * sec / it);
    return 0;
}

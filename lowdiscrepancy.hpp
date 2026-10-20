// lowdiscrepancy.hpp
// Header-only low-discrepancy (quasi-random) point generation. No Qt dependency.
//
//   qmc::Sobol           - Sobol sequence (Joe-Kuo direction numbers, Gray-code
//                          order, optional LMS + digital-shift scrambling,
//                          same scheme as scipy.stats.qmc.Sobol)
//   qmc::seedPoints      - n points in [0,1)^3: scrambled Sobol + periodic
//                          Gaussian jitter (controlled dispersion)
//   qmc::seedVoxels      - the same mapped to a size^3 voxel grid, unique voxels
//
// Dispersion control (sigma, in units of the mean seed spacing d = n^(-1/3)):
//   sigma = 0    -> pure scrambled Sobol (most uniform, lowest grain-size scatter)
//   sigma ~ 0.3  -> intermediate
//   sigma >= ~1  -> practically uniform random (Poisson-like)

#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <set>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace qmc {

namespace detail {

// Joe & Kuo (2008) new-joe-kuo-6.21201, dimensions 2..12: {s, a, m_1..m_s}.
// Dimension 1 is the van der Corput sequence (all m_k = 1).
struct DirInit { int s; uint32_t a; std::array<uint32_t, 5> m; };
inline const std::array<DirInit, 11>& joeKuo()
{
    static const std::array<DirInit, 11> t = {{
        {1,  0, {1}},
        {2,  1, {1, 3}},
        {3,  1, {1, 3, 1}},
        {3,  2, {1, 1, 1}},
        {4,  1, {1, 1, 3, 3}},
        {4,  4, {1, 3, 5, 13}},
        {5,  2, {1, 1, 5, 5, 17}},
        {5,  4, {1, 1, 5, 5, 5}},
        {5,  7, {1, 1, 7, 11, 19}},
        {5, 11, {1, 1, 5, 1, 1}},
        {5, 13, {1, 1, 1, 3, 11}},
    }};
    return t;
}

constexpr int kBits = 32;                       // output resolution 2^-32

}  // namespace detail

class Sobol {
public:
    static constexpr int kMaxDim = 12;

    // dim: 1..12. scramble=false reproduces scipy Sobol(scramble=False) exactly
    // (including the first point 0). scramble=true applies a random linear
    // matrix scramble + digital shift driven by `seed`.
    Sobol(int dim, bool scramble = true, uint64_t seed = 0)
        : dim_(dim), v_(dim), x_(dim, 0u), shift_(dim, 0u)
    {
        if (dim < 1 || dim > kMaxDim)
            throw std::invalid_argument("qmc::Sobol: dim must be 1..12");
        using detail::kBits;

        // direction numbers v[d][k], k = 0..kBits-1, left-aligned in 32 bits
        for (int d = 0; d < dim; ++d) {
            auto& v = v_[d];
            v.assign(kBits, 0u);
            if (d == 0) {
                for (int k = 0; k < kBits; ++k) v[k] = 1u << (kBits - 1 - k);
                continue;
            }
            const auto& in = detail::joeKuo()[d - 1];
            const int s = in.s;
            for (int k = 0; k < s && k < kBits; ++k)
                v[k] = in.m[k] << (kBits - 1 - k);
            for (int k = s; k < kBits; ++k) {
                uint32_t val = v[k - s] ^ (v[k - s] >> s);
                for (int j = 1; j < s; ++j)
                    if ((in.a >> (s - 1 - j)) & 1u) val ^= v[k - j];
                v[k] = val;
            }
        }

        if (scramble) {
            std::mt19937_64 rng(seed);
            // Linear matrix scramble: lower-triangular binary L with unit
            // diagonal; new column k = L * v[k] (bits counted from the MSB).
            for (int d = 0; d < dim; ++d) {
                std::array<uint32_t, kBits> L{};       // L[r] = row r as bit mask
                for (int r = 0; r < kBits; ++r) {
                    uint32_t row = 1u << (kBits - 1 - r);           // diagonal
                    for (int c = 0; c < r; ++c)
                        if (rng() & 1u) row |= 1u << (kBits - 1 - c);
                    L[r] = row;
                }
                for (int k = 0; k < kBits; ++k) {
                    uint32_t out = 0;
                    for (int r = 0; r < kBits; ++r)
                        if (parity(L[r] & v_[d][k])) out |= 1u << (kBits - 1 - r);
                    v_[d][k] = out;
                }
                shift_[d] = static_cast<uint32_t>(rng());           // digital shift
            }
        }
    }

    // Next point in [0,1)^dim (Gray-code order, Antonov-Saleev).
    std::vector<double> next()
    {
        std::vector<double> p(dim_);
        for (int d = 0; d < dim_; ++d)
            p[d] = static_cast<double>(x_[d] ^ shift_[d]) * kScale;
        const int c = lowestZeroBit(index_);
        if (c < detail::kBits)
            for (int d = 0; d < dim_; ++d) x_[d] ^= v_[d][c];
        ++index_;
        return p;
    }

    void skip(uint64_t n) { for (uint64_t i = 0; i < n; ++i) next(); }
    int dim() const { return dim_; }

private:
    static constexpr double kScale = 1.0 / 4294967296.0;   // 2^-32

    static bool parity(uint32_t x)
    {
        x ^= x >> 16; x ^= x >> 8; x ^= x >> 4; x ^= x >> 2; x ^= x >> 1;
        return x & 1u;
    }
    static int lowestZeroBit(uint64_t i)
    {
        int c = 0;
        while (i & 1u) { i >>= 1; ++c; }
        return c;
    }

    int dim_;
    uint64_t index_ = 0;
    std::vector<std::vector<uint32_t>> v_;
    std::vector<uint32_t> x_;
    std::vector<uint32_t> shift_;
};

// n points in the periodic unit cube [0,1)^3.
// sigma: Gaussian jitter in units of the mean spacing n^(-1/3); 0 = pure Sobol.
// The first point is kept: after scrambling it is not 0, and skipping it would
// break the balance property for n = 2^m. Same (n, seed, sigma) -> same points.
inline std::vector<std::array<double, 3>> seedPoints(int n, uint64_t seed, double sigma)
{
    if (n <= 0) return {};
    if (sigma < 0) throw std::invalid_argument("qmc::seedPoints: sigma must be >= 0");
    Sobol sob(3, true, seed);
    std::mt19937_64 rng(seed ^ 0x9E3779B97F4A7C15ULL);   // independent stream for jitter
    std::normal_distribution<double> gauss(0.0, 1.0);
    const double amp = sigma * std::cbrt(1.0 / n);

    std::vector<std::array<double, 3>> pts(n);
    for (int i = 0; i < n; ++i) {
        const auto p = sob.next();
        for (int d = 0; d < 3; ++d) {
            double x = p[d] + (amp > 0 ? amp * gauss(rng) : 0.0);
            x -= std::floor(x);                     // periodic wrap into [0,1)
            pts[i][d] = (x >= 1.0) ? 0.0 : x;       // guard against rounding to 1
        }
    }
    return pts;
}

// n seeds as unique voxel indices on a size^3 grid. Collisions (two seeds in one
// voxel) are replaced by further points of the same sequence, so the result is
// still deterministic. Throws if n > size^3.
inline std::vector<std::array<int, 3>> seedVoxels(int n, int size, uint64_t seed, double sigma)
{
    if (static_cast<long long>(n) > static_cast<long long>(size) * size * size)
        throw std::invalid_argument("qmc::seedVoxels: more seeds than voxels");
    std::vector<std::array<int, 3>> out;
    std::set<std::tuple<int, int, int>> used;
    // generate in growing batches from one sequence; the jitter amplitude is
    // tied to the requested n, not to the batch length
    const double amp = sigma * std::cbrt(1.0 / n);
    Sobol sob(3, true, seed);
    std::mt19937_64 rng(seed ^ 0x9E3779B97F4A7C15ULL);
    std::normal_distribution<double> gauss(0.0, 1.0);
    const long long maxDraws = 64LL * n + 1024;
    for (long long draw = 0; static_cast<int>(out.size()) < n; ++draw) {
        if (draw >= maxDraws)
            throw std::runtime_error("qmc::seedVoxels: could not place all seeds");
        const auto p = sob.next();
        std::array<int, 3> v{};
        for (int d = 0; d < 3; ++d) {
            double x = p[d] + (amp > 0 ? amp * gauss(rng) : 0.0);
            x -= std::floor(x);
            int k = static_cast<int>(x * size);
            v[d] = (k >= size) ? size - 1 : k;
        }
        if (used.insert({v[0], v[1], v[2]}).second) out.push_back(v);
    }
    return out;
}

}  // namespace qmc

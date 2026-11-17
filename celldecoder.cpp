#include "celldecoder.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

namespace {

// Cursor over the exported file.
struct Reader {
    const unsigned char* p;
    size_t left;
    bool ok = true;

    bool take(void* dst, size_t n)
    {
        if (!ok || n > left) { ok = false; return false; }
        std::memcpy(dst, p, n);
        p += n;
        left -= n;
        return true;
    }
    int32_t i32() { int32_t v = 0; take(&v, 4); return v; }
    float f32() { float v = 0; take(&v, 4); return v; }
    std::vector<float> floats(size_t n)
    {
        std::vector<float> v;
        if (!ok || n > left / 4) { ok = false; return v; }
        v.resize(n);
        take(v.data(), 4 * n);
        return v;
    }
    // One tensor: its length, then the values; the length has to be the expected one.
    std::vector<float> tensor(size_t expected)
    {
        const int32_t n = i32();
        if (n < 0 || size_t(n) != expected) { ok = false; return {}; }
        return floats(expected);
    }
};

} // namespace

bool CellDecoder::load(const unsigned char* data, size_t size, std::string* error)
{
    auto fail = [&](const char* what) {
        if (error) *error = what;
        *this = CellDecoder();
        return false;
    };
    *this = CellDecoder();
    if (!data || size < 8 || std::memcmp(data, "MVCELLD1", 8) != 0) return fail("not a cell decoder file");
    Reader r{ data + 8, size - 8 };
    m_zc = r.i32();
    m_codeN = r.i32();
    m_stem = r.i32();
    m_groups = r.i32();
    const int levels = r.i32();
    if (!r.ok || m_zc < 1 || m_zc > 64 || m_codeN < 2 || m_codeN > 64 || m_stem < 1 || m_stem > 4 || m_groups < 1
        || levels < 1 || levels > 6)
        return fail("bad header");
    for (int i = 0; i < levels; ++i) m_channels.push_back(r.i32());
    for (int i = 0; i < levels; ++i) m_blocks.push_back(r.i32());
    for (int i = 0; i < levels; ++i)
        if (!r.ok || m_channels[i] < m_groups || m_channels[i] > 1024 || m_channels[i] % m_groups != 0 || m_blocks[i] < 0 || m_blocks[i] > 16)
            return fail("bad layer sizes");
    m_threshold = r.f32();
    m_mean = r.floats(size_t(m_zc));
    m_std = r.floats(size_t(m_zc));
    r.i32(); // training step, part of the name
    char name[33] = {};
    r.take(name, 32);
    m_name = name;

    auto conv = [&](int cin, int cout) {
        Conv c;
        c.cin = cin;
        c.cout = cout;
        c.w = r.tensor(size_t(cout) * cin * 27);
        c.b = r.tensor(size_t(cout));
        m_weights += c.w.size() + c.b.size();
        m_convs.push_back(std::move(c));
    };
    auto norm = [&](int channels) {
        Norm g;
        g.channels = channels;
        g.w = r.tensor(size_t(channels));
        g.b = r.tensor(size_t(channels));
        m_weights += g.w.size() + g.b.size();
        m_norms.push_back(std::move(g));
    };
    // The same walk as Decoder.__init__ in model.py: from the coarsest level up.
    conv(m_zc, m_channels.back());
    for (int lvl = levels - 1; lvl >= 0; --lvl) {
        const int c = m_channels[lvl];
        for (int b = 0; b < m_blocks[lvl]; ++b) { norm(c); conv(c, c); norm(c); conv(c, c); }
        if (lvl > 0) conv(c, m_channels[lvl - 1]);
    }
    norm(m_channels[0]);
    conv(m_channels[0], m_stem * m_stem * m_stem);
    if (!r.ok) return fail("truncated or mismatched weights");
    if (r.left != 0) return fail("unexpected data after the weights");
    return true;
}

int CellDecoder::cellSize() const
{
    return m_codeN * (1 << (int(m_channels.size()) - 1)) * m_stem;
}

// out[co][i][j][k] = b[co] + sum over ci and the 27 taps of w * in[ci][i + a - 1][j + b - 1][k + c - 1], indices wrapped.
void CellDecoder::conv3(const Conv& c, const std::vector<float>& in, int n, std::vector<float>& out) const
{
    const int p = n + 2;
    const size_t n3 = size_t(n) * n * n, p3 = size_t(p) * p * p;
    // The input with one wrapped layer on every side, so the inner loop has no index arithmetic.
    std::vector<float> pad(size_t(c.cin) * p3);
#pragma omp parallel for schedule(static)
    for (int ci = 0; ci < c.cin; ++ci) {
        const float* src = in.data() + size_t(ci) * n3;
        float* dst = pad.data() + size_t(ci) * p3;
        for (int i = 0; i < p; ++i) {
            const int si = (i + n - 1) % n;
            for (int j = 0; j < p; ++j) {
                const int sj = (j + n - 1) % n;
                const float* s = src + (size_t(si) * n + sj) * n;
                float* d = dst + (size_t(i) * p + j) * p;
                d[0] = s[n - 1];
                std::memcpy(d + 1, s, size_t(n) * sizeof(float));
                d[n + 1] = s[0];
            }
        }
    }
    out.assign(size_t(c.cout) * n3, 0.0f);
#pragma omp parallel for schedule(static)
    for (int co = 0; co < c.cout; ++co) {
        float* o = out.data() + size_t(co) * n3;
        const float* wco = c.w.data() + size_t(co) * c.cin * 27;
        const float bias = c.b[size_t(co)];
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                float* row = o + (size_t(i) * n + j) * n;
                for (int k = 0; k < n; ++k) row[k] = bias;
                for (int ci = 0; ci < c.cin; ++ci) {
                    const float* w = wco + size_t(ci) * 27;
                    const float* pc = pad.data() + size_t(ci) * p3;
                    for (int a = 0; a < 3; ++a) {
                        for (int b = 0; b < 3; ++b) {
                            const float* s = pc + (size_t(i + a) * p + (j + b)) * p;
                            const float w0 = w[a * 9 + b * 3], w1 = w[a * 9 + b * 3 + 1], w2 = w[a * 9 + b * 3 + 2];
                            for (int k = 0; k < n; ++k) row[k] += w0 * s[k] + w1 * s[k + 1] + w2 * s[k + 2];
                        }
                    }
                }
            }
        }
    }
}

// GroupNorm (mean and variance over each group of channels and all positions) followed by SiLU, in place.
void CellDecoder::normSilu(const Norm& g, std::vector<float>& x, int n) const
{
    const size_t n3 = size_t(n) * n * n;
    const int per = g.channels / m_groups;
#pragma omp parallel for schedule(static)
    for (int grp = 0; grp < m_groups; ++grp) {
        float* v = x.data() + size_t(grp) * per * n3;
        const size_t count = size_t(per) * n3;
        double sum = 0.0, sq = 0.0;
        for (size_t t = 0; t < count; ++t) { sum += v[t]; sq += double(v[t]) * v[t]; }
        const double mean = sum / double(count);
        const double var = std::max(0.0, sq / double(count) - mean * mean);
        const float inv = float(1.0 / std::sqrt(var + 1e-5)), m = float(mean);
        for (int c = 0; c < per; ++c) {
            const float w = g.w[size_t(grp * per + c)] * inv, b = g.b[size_t(grp * per + c)];
            float* vc = v + size_t(c) * n3;
            for (size_t t = 0; t < n3; ++t) {
                const float y = (vc[t] - m) * w + b;
                vc[t] = y / (1.0f + std::exp(-y));
            }
        }
    }
}

std::vector<float> CellDecoder::decodeLogits(const std::vector<float>& code) const
{
    if (!loaded() || code.size() != size_t(m_zc) * m_codeN * m_codeN * m_codeN) return {};
    size_t ic = 0, in = 0; // next convolution, next norm
    int n = m_codeN;
    std::vector<float> x, h, t;
    conv3(m_convs[ic++], code, n, x);
    for (int lvl = int(m_channels.size()) - 1; lvl >= 0; --lvl) {
        for (int b = 0; b < m_blocks[size_t(lvl)]; ++b) {
            // Residual block: x + conv(silu(norm(conv(silu(norm(x)))))).
            h = x;
            normSilu(m_norms[in++], h, n);
            conv3(m_convs[ic++], h, n, t);
            normSilu(m_norms[in++], t, n);
            conv3(m_convs[ic++], t, n, h);
            for (size_t i = 0; i < x.size(); ++i) x[i] += h[i];
        }
        if (lvl > 0) {
            // Every value repeated 2 x 2 x 2, then a convolution.
            const int c = m_channels[size_t(lvl)], m = 2 * n;
            const size_t n3 = size_t(n) * n * n, m3 = size_t(m) * m * m;
            h.assign(size_t(c) * m3, 0.0f);
#pragma omp parallel for schedule(static)
            for (int ch = 0; ch < c; ++ch)
                for (int i = 0; i < m; ++i)
                    for (int j = 0; j < m; ++j) {
                        const float* s = x.data() + size_t(ch) * n3 + (size_t(i / 2) * n + j / 2) * n;
                        float* d = h.data() + size_t(ch) * m3 + (size_t(i) * m + j) * m;
                        for (int k = 0; k < m; ++k) d[k] = s[k / 2];
                    }
            conv3(m_convs[ic++], h, m, x);
            n = m;
        }
    }
    normSilu(m_norms[in++], x, n);
    conv3(m_convs[ic++], x, n, h);
    // Channel a*s*s + b*s + c of position (i, j, k) is the voxel (s*i + a, s*j + b, s*k + c).
    const int s = m_stem, big = n * s;
    const size_t n3 = size_t(n) * n * n;
    std::vector<float> out(size_t(big) * big * big);
    for (int a = 0; a < s; ++a)
        for (int b = 0; b < s; ++b)
            for (int c = 0; c < s; ++c) {
                const float* src = h.data() + size_t((a * s + b) * s + c) * n3;
                for (int i = 0; i < n; ++i)
                    for (int j = 0; j < n; ++j)
                        for (int k = 0; k < n; ++k)
                            out[(size_t(s * i + a) * big + (s * j + b)) * big + (s * k + c)] = src[(size_t(i) * n + j) * n + k];
            }
    return out;
}

std::vector<uint8_t> CellDecoder::decode(const std::vector<float>& code) const
{
    const std::vector<float> logits = decodeLogits(code);
    // sigmoid(x) >= t  <=>  x >= log(t / (1 - t))
    const float cut = std::log(m_threshold / (1.0f - m_threshold));
    std::vector<uint8_t> solid(logits.size());
    for (size_t i = 0; i < logits.size(); ++i) solid[i] = logits[i] >= cut ? 1 : 0;
    return solid;
}

std::vector<float> CellDecoder::randomCode(uint32_t seed, double smooth) const
{
    if (!loaded()) return {};
    const int n = m_codeN;
    const size_t n3 = size_t(n) * n * n;
    std::vector<float> code(size_t(m_zc) * n3);
    std::mt19937 rng(seed);
    // Box-Muller on the raw generator: std::normal_distribution differs between standard libraries.
    auto uniform = [&]() { return (double(rng()) + 0.5) / 4294967296.0; };
    for (size_t i = 0; i < code.size(); i += 2) {
        const double r = std::sqrt(-2.0 * std::log(uniform())), a = 6.283185307179586 * uniform();
        code[i] = float(r * std::cos(a));
        if (i + 1 < code.size()) code[i + 1] = float(r * std::sin(a));
    }
    std::vector<double> kernel;
    int radius = 0;
    if (smooth > 1e-3) {
        radius = std::min(n / 2, int(std::ceil(3.0 * smooth)));
        double total = 0.0;
        for (int d = -radius; d <= radius; ++d) {
            kernel.push_back(std::exp(-0.5 * d * d / (smooth * smooth)));
            total += kernel.back();
        }
        for (double& k : kernel) k /= total;
    }
    std::vector<float> line(static_cast<size_t>(n));
    for (int c = 0; c < m_zc; ++c) {
        float* v = code.data() + size_t(c) * n3;
        if (!kernel.empty()) {
            // Separable periodic blur: one pass along each axis.
            const size_t strides[3] = { size_t(n) * n, size_t(n), 1 };
            for (size_t stride : strides)
                for (size_t start = 0; start < n3; ++start) {
                    if ((start / stride) % size_t(n) != 0) continue; // first element of a line along this axis
                    for (int i = 0; i < n; ++i) {
                        double acc = 0.0;
                        for (int d = -radius; d <= radius; ++d)
                            acc += kernel[size_t(d + radius)] * v[start + size_t((i + d + n) % n) * stride];
                        line[size_t(i)] = float(acc);
                    }
                    for (int i = 0; i < n; ++i) v[start + size_t(i) * stride] = line[size_t(i)];
                }
        }
        // Zero mean and unit spread over the channel, then the channel's own mean and spread.
        double sum = 0.0, sq = 0.0;
        for (size_t i = 0; i < n3; ++i) { sum += v[i]; sq += double(v[i]) * v[i]; }
        const double mean = sum / double(n3), sd = std::sqrt(std::max(1e-12, sq / double(n3) - mean * mean));
        for (size_t i = 0; i < n3; ++i) v[i] = float(m_mean[size_t(c)] + m_std[size_t(c)] * (v[i] - mean) / sd);
    }
    return code;
}

size_t CellDecoder::keepLargestBody(std::vector<uint8_t>& cell, int n, int* pieces)
{
    if (pieces) *pieces = 0;
    const size_t total = n > 0 ? size_t(n) * n * n : 0;
    if (total == 0 || cell.size() != total) return 0;
    // Flood fill from every solid voxel not reached yet; label = the body's number from 1.
    std::vector<int32_t> label(total, 0);
    std::vector<size_t> sizes;
    std::vector<size_t> stack;
    const size_t plane = size_t(n) * n;
    for (size_t start = 0; start < total; ++start) {
        if (!cell[start] || label[start]) continue;
        const int32_t id = int32_t(sizes.size()) + 1;
        size_t count = 0;
        label[start] = id;
        stack.push_back(start);
        while (!stack.empty()) {
            const size_t v = stack.back();
            stack.pop_back();
            ++count;
            const int i = int(v / plane), j = int(v / size_t(n) % size_t(n)), k = int(v % size_t(n));
            const size_t next[6] = {
                size_t((i + 1) % n) * plane + size_t(j) * n + k, size_t((i + n - 1) % n) * plane + size_t(j) * n + k,
                size_t(i) * plane + size_t((j + 1) % n) * n + k, size_t(i) * plane + size_t((j + n - 1) % n) * n + k,
                size_t(i) * plane + size_t(j) * n + size_t((k + 1) % n), size_t(i) * plane + size_t(j) * n + size_t((k + n - 1) % n) };
            for (size_t w : next)
                if (cell[w] && !label[w]) {
                    label[w] = id;
                    stack.push_back(w);
                }
        }
        sizes.push_back(count);
    }
    if (sizes.size() < 2) return 0;
    const int32_t keep = int32_t(std::max_element(sizes.begin(), sizes.end()) - sizes.begin()) + 1;
    size_t removed = 0;
    for (size_t v = 0; v < total; ++v)
        if (cell[v] && label[v] != keep) {
            cell[v] = 0;
            ++removed;
        }
    if (pieces) *pieces = int(sizes.size()) - 1;
    return removed;
}

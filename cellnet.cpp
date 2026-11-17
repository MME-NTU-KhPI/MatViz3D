#include "cellnet.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef MATVIZ_USE_ACCELERATE
#include <Accelerate/Accelerate.h>
#endif

namespace cellnet {

namespace {

// Room for n values in a buffer that is reused between the layers (it only grows).
float* room(std::vector<float>& v, size_t n)
{
    if (v.size() < n) v.resize(n);
    return v.data();
}

} // namespace

void gemmAdd(int m, int n, int k, const float* A, const float* B, float* C)
{
#ifdef MATVIZ_USE_ACCELERATE
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, m, n, k, 1.0f, A, k, B, n, 1.0f, C, n);
#pragma clang diagnostic pop
#else
    // Blocks of columns, so the piece of B a row of C runs through stays in the cache.
    const int block = 256, blocks = (n + block - 1) / block;
#pragma omp parallel for collapse(2) schedule(static)
    for (int jb = 0; jb < blocks; ++jb)
        for (int i = 0; i < m; ++i) {
            const int j0 = jb * block, j1 = std::min(n, j0 + block);
            float* c = C + size_t(i) * n;
            const float* a = A + size_t(i) * k;
            for (int p = 0; p < k; ++p) {
                const float w = a[p];
                const float* b = B + size_t(p) * n;
                for (int j = j0; j < j1; ++j) c[j] += w * b[j];
            }
        }
#endif
}

std::vector<float> dense(const Linear& l, const std::vector<float>& x)
{
    std::vector<float> y(size_t(l.out));
    for (int o = 0; o < l.out; ++o) {
        const float* w = l.w.data() + size_t(o) * l.in;
        float acc = l.b[size_t(o)];
        for (int i = 0; i < l.in; ++i) acc += w[i] * x[size_t(i)];
        y[size_t(o)] = acc;
    }
    return y;
}

void silu(std::vector<float>& v)
{
    for (float& y : v) y = y / (1.0f + std::exp(-y));
}

std::vector<float> levelWave(int level, int half)
{
    std::vector<float> wave(size_t(2 * half));
    const float rate = float(-std::log(10000.0));
    for (int i = 0; i < half; ++i) {
        const float angle = float(level) * std::exp(rate * float(i) / float(half));
        wave[size_t(i)] = std::sin(angle);
        wave[size_t(half + i)] = std::cos(angle);
    }
    return wave;
}

// Output o reads inputs o * stride - 1, o * stride, o * stride + 1: the input is unrolled into a matrix with a row
// per input channel and tap, then out = W * that matrix.
void conv3(const Conv& c, const std::vector<float>& in, int n, std::vector<float>& out, Scratch& s)
{
    const int m = n / c.stride, rows = c.cin * 27;
    const size_t n3 = size_t(n) * n * n, m3 = size_t(m) * m * m;
    int at[3][64]; // input index along one axis for a tap and an output position
    for (int a = 0; a < 3; ++a)
        for (int o = 0; o < m; ++o) at[a][o] = (o * c.stride + a - 1 + n) % n;
    float* col = room(s.col, size_t(rows) * m3);
#pragma omp parallel for schedule(static)
    for (int ci = 0; ci < c.cin; ++ci) {
        const float* src = in.data() + size_t(ci) * n3;
        for (int a = 0; a < 3; ++a)
            for (int b = 0; b < 3; ++b)
                for (int g = 0; g < 3; ++g) {
                    float* row = col + size_t(ci * 27 + a * 9 + b * 3 + g) * m3;
                    for (int i = 0; i < m; ++i)
                        for (int j = 0; j < m; ++j) {
                            const float* line = src + (size_t(at[a][i]) * n + at[b][j]) * n;
                            float* d = row + (size_t(i) * m + j) * m;
                            if (c.stride != 1) {
                                for (int k = 0; k < m; ++k) d[k] = line[at[g][k]];
                            } else if (g == 0) {
                                d[0] = line[n - 1];
                                std::memcpy(d + 1, line, size_t(n - 1) * sizeof(float));
                            } else if (g == 1) {
                                std::memcpy(d, line, size_t(n) * sizeof(float));
                            } else {
                                std::memcpy(d, line + 1, size_t(n - 1) * sizeof(float));
                                d[n - 1] = line[0];
                            }
                        }
                }
    }
    out.resize(size_t(c.cout) * m3);
    for (int co = 0; co < c.cout; ++co) std::fill_n(out.data() + size_t(co) * m3, m3, c.b[size_t(co)]);
    gemmAdd(c.cout, int(m3), rows, c.w.data(), col, out.data());
}

// Mean and variance over each group of channels and all positions.
void groupNorm(const Norm& g, std::vector<float>& x, size_t n3, bool withSilu, int groups)
{
    const int per = g.channels / groups;
#pragma omp parallel for schedule(static)
    for (int grp = 0; grp < groups; ++grp) {
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
            if (withSilu) {
                for (size_t t = 0; t < n3; ++t) {
                    const float y = (vc[t] - m) * w + b;
                    vc[t] = y / (1.0f + std::exp(-y));
                }
            } else {
                for (size_t t = 0; t < n3; ++t) vc[t] = (vc[t] - m) * w + b;
            }
        }
    }
}

// Two convolutions, the vector e added to the channels between them (in a block with `scale` the channels are first
// multiplied by 1 + a linear map of e), and the input added at the end (through a 1 x 1 x 1 convolution when the
// number of channels changes).
void res(const Res& r, std::vector<float>& x, int n, const std::vector<float>& e, Scratch& s, int groups)
{
    const size_t n3 = size_t(n) * n * n;
    std::vector<float> h = x, t;
    groupNorm(r.n1, h, n3, true, groups);
    conv3(r.c1, h, n, t, s);
    if (r.emb.out > 0) {
        const std::vector<float> shift = dense(r.emb, e);
        const std::vector<float> gain = r.scale.out > 0 ? dense(r.scale, e) : std::vector<float>();
        for (int c = 0; c < r.cout; ++c) {
            float* v = t.data() + size_t(c) * n3;
            const float add = shift[size_t(c)];
            if (gain.empty()) {
                for (size_t i = 0; i < n3; ++i) v[i] += add;
            } else {
                const float times = 1.0f + gain[size_t(c)];
                for (size_t i = 0; i < n3; ++i) v[i] = v[i] * times + add;
            }
        }
    }
    groupNorm(r.n2, t, n3, true, groups);
    conv3(r.c2, t, n, h, s);
    if (r.hasSkip) {
        for (int c = 0; c < r.cout; ++c) {
            float* v = h.data() + size_t(c) * n3;
            const float add = r.skip.b[size_t(c)];
            for (size_t i = 0; i < n3; ++i) v[i] += add;
        }
        gemmAdd(r.cout, int(n3), r.cin, r.skip.w.data(), x.data(), h.data());
    } else {
        for (size_t i = 0; i < h.size(); ++i) h[i] += x[i];
    }
    x.swap(h);
}

// Every position of the grid looks at all the others. For each of the heads the weights are
// softmax(q . k / sqrt(d)) over the positions, the output is the weighted sum of the values v.
void attention(const Attn& a, std::vector<float>& x, int n, Scratch& s, int groups, int heads)
{
    const int c = a.channels, L = n * n * n, d = c / heads;
    const size_t l = size_t(L);
    std::vector<float> h = x;
    groupNorm(a.n, h, l, false, groups);
    // Queries, keys and values of all positions at once: rows 0..c, c..2c, 2c..3c.
    float* qkv = room(s.a, size_t(3 * c) * l);
    for (int row = 0; row < 3 * c; ++row) std::fill_n(qkv + size_t(row) * l, l, a.in.b[size_t(row)]);
    gemmAdd(3 * c, L, c, a.in.w.data(), h.data(), qkv);
    float* mix = room(s.b, size_t(c) * l);
    std::fill_n(mix, size_t(c) * l, 0.0f);
    float* work = room(s.c, size_t(d) * l + 2 * l * l);
    float* qt = work;                  // queries of one head, a row per position
    float* w = work + size_t(d) * l;   // weights: row = the position that looks, column = the one looked at
    float* wt = w + l * l;             // the same, turned
    const float scale = 1.0f / std::sqrt(float(d));
    for (int head = 0; head < heads; ++head) {
        const float* Q = qkv + size_t(head * d) * l;
        const float* K = qkv + size_t(c + head * d) * l;
        const float* V = qkv + size_t(2 * c + head * d) * l;
        for (int j = 0; j < d; ++j)
            for (int p = 0; p < L; ++p) qt[size_t(p) * d + j] = Q[size_t(j) * l + p] * scale;
        std::fill_n(w, l * l, 0.0f);
        gemmAdd(L, L, d, qt, K, w);
#pragma omp parallel for schedule(static)
        for (int p = 0; p < L; ++p) {
            float* row = w + size_t(p) * l;
            float top = row[0];
            for (int q = 1; q < L; ++q) top = std::max(top, row[q]);
            float sum = 0.0f;
            for (int q = 0; q < L; ++q) { row[q] = std::exp(row[q] - top); sum += row[q]; }
            const float inv = 1.0f / sum;
            for (int q = 0; q < L; ++q) wt[size_t(q) * l + p] = row[q] * inv;
        }
        gemmAdd(d, L, L, V, wt, mix + size_t(head * d) * l);
    }
    for (int ch = 0; ch < c; ++ch) {
        float* v = x.data() + size_t(ch) * l;
        const float add = a.out.b[size_t(ch)];
        for (size_t i = 0; i < l; ++i) v[i] += add;
    }
    gemmAdd(c, L, c, a.out.w.data(), mix, x.data());
}

// Every position of the grid looks at the tokens of the request. A token goes through `key` to the channels of the
// map and from there to a key and a value; the queries come from the map. For each of the heads the weights are
// softmax(q . k / sqrt(d)) over the tokens, the output is the weighted sum of their values.
void crossAttention(const Cross& a, std::vector<float>& x, int n, const std::vector<float>& tokens, int count, Scratch& s,
                    int groups, int heads)
{
    const int c = a.channels, L = n * n * n, d = c / heads, length = a.key.in;
    const size_t l = size_t(L);
    std::vector<float> h = x;
    groupNorm(a.n, h, l, false, groups);
    // Keys and values of the tokens, a row per token: rows c..2c and 2c..3c of `in` on the token brought to c numbers.
    std::vector<float> keys(size_t(count) * c), values(size_t(count) * c);
    for (int t = 0; t < count; ++t) {
        const std::vector<float> k =
            dense(a.key, std::vector<float>(tokens.begin() + long(t) * length, tokens.begin() + long(t + 1) * length));
        for (int ch = 0; ch < c; ++ch) {
            const float* wk = a.in.w.data() + size_t(c + ch) * c;
            const float* wv = a.in.w.data() + size_t(2 * c + ch) * c;
            float key = a.in.b[size_t(c + ch)], value = a.in.b[size_t(2 * c + ch)];
            for (int i = 0; i < c; ++i) {
                key += wk[i] * k[size_t(i)];
                value += wv[i] * k[size_t(i)];
            }
            keys[size_t(t) * c + ch] = key;
            values[size_t(t) * c + ch] = value;
        }
    }
    // Queries of all positions at once: rows 0..c of `in`.
    float* q = room(s.a, size_t(c) * l);
    for (int row = 0; row < c; ++row) std::fill_n(q + size_t(row) * l, l, a.in.b[size_t(row)]);
    gemmAdd(c, L, c, a.in.w.data(), h.data(), q);
    float* mix = room(s.b, size_t(c) * l);
    std::fill_n(mix, size_t(c) * l, 0.0f);
    float* w = room(s.c, size_t(count) * l); // weights: row = the token looked at, column = the position that looks
    std::vector<float> kh(size_t(count) * d), vt(size_t(d) * count);
    const float scale = 1.0f / std::sqrt(float(d));
    for (int head = 0; head < heads; ++head) {
        for (int t = 0; t < count; ++t)
            for (int j = 0; j < d; ++j) {
                kh[size_t(t) * d + j] = keys[size_t(t) * c + head * d + j] * scale;
                vt[size_t(j) * count + t] = values[size_t(t) * c + head * d + j];
            }
        std::fill_n(w, size_t(count) * l, 0.0f);
        gemmAdd(count, L, d, kh.data(), q + size_t(head * d) * l, w);
#pragma omp parallel for schedule(static)
        for (int p = 0; p < L; ++p) {
            float top = w[p];
            for (int t = 1; t < count; ++t) top = std::max(top, w[size_t(t) * l + p]);
            float sum = 0.0f;
            for (int t = 0; t < count; ++t) {
                float& v = w[size_t(t) * l + p];
                v = std::exp(v - top);
                sum += v;
            }
            const float inv = 1.0f / sum;
            for (int t = 0; t < count; ++t) w[size_t(t) * l + p] *= inv;
        }
        gemmAdd(d, L, count, vt.data(), w, mix + size_t(head * d) * l);
    }
    for (int ch = 0; ch < c; ++ch) {
        float* v = x.data() + size_t(ch) * l;
        const float add = a.out.b[size_t(ch)];
        for (size_t i = 0; i < l; ++i) v[i] += add;
    }
    gemmAdd(c, L, c, a.out.w.data(), mix, x.data());
}

} // namespace cellnet

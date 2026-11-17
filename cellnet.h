#ifndef CELLNET_H
#define CELLNET_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

/**
 * The layers shared by the networks of the Neural tab that work on the code of a
 * cell: the diffusion network (CellDiffusion) and the property predictor
 * (CellPredictor). Plain C++ (no Qt, no ML library): circular 3 x 3 x 3
 * convolutions as one matrix product over the unrolled input, GroupNorm, SiLU,
 * residual blocks with a vector added between their two convolutions, self-
 * attention, attention to the tokens of a request, and the reader of the
 * exported weight files.
 *
 * The matrix product is Accelerate's sgemm on macOS (define MATVIZ_USE_ACCELERATE),
 * elsewhere a plain OpenMP loop, several times slower.
 *
 * Arrays are flat, the first index slowest: a map is [channel][i][j][k].
 */
namespace cellnet {

struct Conv { int cin = 0, cout = 0, stride = 1; std::vector<float> w, b; };  // w[cout][cin][3][3][3]
struct Norm { int channels = 0; std::vector<float> w, b; };
struct Linear { int in = 0, out = 0; std::vector<float> w, b; };              // w[out][in]
/// emb.out == 0: a block without the added vector (the blocks of the autoencoder).
/// scale.out > 0: the vector also scales the channels, by 1 + a linear map of it, before it is added.
struct Res { int cin = 0, cout = 0; Norm n1, n2; Conv c1, c2; Linear emb; bool hasSkip = false; Linear skip, scale; };
struct Attn { bool used = false; int channels = 0; Norm n; Linear in, out; };
/// Attention of the grid to the tokens of a request: `key` brings a token to the channels of the map, `in` holds
/// the queries of the grid and the keys and values of the tokens (as in Attn), `out` the mix that is added.
struct Cross { bool used = false; int channels = 0; Norm n; Linear key, in, out; };
/// Buffers reused between the layers of one pass (they only grow).
struct Scratch { std::vector<float> col, a, b, c; };

/// Cursor over an exported file.
struct FileReader {
    std::FILE* f = nullptr;
    bool ok = true;
    uint64_t pos = 0;

    ~FileReader() { if (f) std::fclose(f); }
    bool take(void* dst, size_t n)
    {
        if (!ok || !f || std::fread(dst, 1, n, f) != n) { ok = false; return false; }
        pos += n;
        return true;
    }
    int32_t i32() { int32_t v = 0; take(&v, 4); return v; }
    float f32() { float v = 0; take(&v, 4); return v; }
    std::vector<float> floats(size_t n)
    {
        std::vector<float> v;
        if (!ok || n > (size_t(1) << 28)) { ok = false; return v; }
        v.resize(n);
        take(v.data(), 4 * n);
        return v;
    }
    /// A string: its length, then the bytes.
    std::string str(int32_t longest)
    {
        const int32_t n = i32();
        if (!ok || n < 0 || n > longest) { ok = false; return {}; }
        std::string s(size_t(n), '\0');
        if (n > 0) take(&s[0], size_t(n));
        return s;
    }
    /// One tensor: its length, then the values; the length has to be the expected one.
    std::vector<float> tensor(size_t expected)
    {
        const int32_t n = i32();
        if (n < 0 || size_t(n) != expected) { ok = false; return {}; }
        return floats(expected);
    }
};

/// Stands in for the reader when only the size of the weights is wanted.
struct Counter {
    uint64_t bytes = 0;
    size_t weights = 0;
    std::vector<float> tensor(size_t expected)
    {
        bytes += 4 + 4 * uint64_t(expected);
        weights += expected;
        return {};
    }
};

// The layers read from a source of tensors (FileReader or Counter), in the order PyTorch's state dict lists them.
template <class Source> Conv readConv(Source& src, int cin, int cout, int stride)
{
    Conv c;
    c.cin = cin;
    c.cout = cout;
    c.stride = stride;
    c.w = src.tensor(size_t(cout) * cin * 27);
    c.b = src.tensor(size_t(cout));
    return c;
}
template <class Source> Norm readNorm(Source& src, int channels)
{
    Norm g;
    g.channels = channels;
    g.w = src.tensor(size_t(channels));
    g.b = src.tensor(size_t(channels));
    return g;
}
template <class Source> Linear readLinear(Source& src, int in, int out)
{
    Linear l;
    l.in = in;
    l.out = out;
    l.w = src.tensor(size_t(out) * in);
    l.b = src.tensor(size_t(out));
    return l;
}
/// emb: length of the vector added between the convolutions, 0 = a block without it; scaled: it scales the channels too.
template <class Source> Res readRes(Source& src, int cin, int cout, int emb, bool scaled = false)
{
    Res r;
    r.cin = cin;
    r.cout = cout;
    r.n1 = readNorm(src, cin);
    r.c1 = readConv(src, cin, cout, 1);
    if (emb > 0) r.emb = readLinear(src, emb, cout);
    r.n2 = readNorm(src, cout);
    r.c2 = readConv(src, cout, cout, 1);
    if (cin != cout) {
        r.hasSkip = true;
        r.skip = readLinear(src, cin, cout); // 1 x 1 x 1 convolution
    }
    if (scaled && emb > 0) r.scale = readLinear(src, emb, cout);
    return r;
}
template <class Source> Attn readAttn(Source& src, int c)
{
    Attn a;
    a.used = true;
    a.channels = c;
    a.n = readNorm(src, c);
    a.in = readLinear(src, c, 3 * c); // queries, keys, values
    a.out = readLinear(src, c, c);
    return a;
}
/// tokenLength: numbers in one token of the request.
template <class Source> Cross readCross(Source& src, int c, int tokenLength)
{
    Cross a;
    a.used = true;
    a.channels = c;
    a.n = readNorm(src, c);
    a.key = readLinear(src, tokenLength, c);
    a.in = readLinear(src, c, 3 * c); // queries of the grid, keys and values of the tokens
    a.out = readLinear(src, c, c);
    return a;
}

/// C (m x n) += A (m x k) * B (k x n), every matrix row by row.
void gemmAdd(int m, int n, int k, const float* A, const float* B, float* C);
/// y = W x + b of a fully connected layer.
std::vector<float> dense(const Linear& l, const std::vector<float>& x);
void silu(std::vector<float>& v);
/// The noise level as `half` sines and `half` cosines of falling frequency (in float, as PyTorch computes it).
std::vector<float> levelWave(int level, int half);
/// 3 x 3 x 3 convolution that wraps around, stride 1 or 2; `in` is [cin][n][n][n], n <= 64.
void conv3(const Conv& c, const std::vector<float>& in, int n, std::vector<float>& out, Scratch& s);
/// GroupNorm over n3 positions, then SiLU when asked; in place.
void groupNorm(const Norm& g, std::vector<float>& x, size_t n3, bool withSilu, int groups);
/// Residual block; e is the vector added between the convolutions (not read when the block has none) and, in a
/// block with `scale`, scaling the channels before that. In place.
void res(const Res& r, std::vector<float>& x, int n, const std::vector<float>& e, Scratch& s, int groups);
/// Self-attention over the n^3 positions; in place (x + output).
void attention(const Attn& a, std::vector<float>& x, int n, Scratch& s, int groups, int heads);
/// Attention of the n^3 positions to `count` tokens (rows of a.key.in numbers each); in place (x + output).
void crossAttention(const Cross& a, std::vector<float>& x, int n, const std::vector<float>& tokens, int count, Scratch& s,
                    int groups, int heads);

} // namespace cellnet

#endif // CELLNET_H

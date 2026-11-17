#include "celldiffusion.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>

using cellnet::Counter;
using cellnet::FileReader;
using cellnet::dense;
using cellnet::silu;

// The same walk as UNet.__init__ in cellgen diffusion.py; the state dict lists the tensors in this order: the two
// tables of the tokens first (they belong to the network itself, not to a layer), the layers of the first recipe,
// then what the second recipe adds - the attention to the tokens after every block and the two layers of the tokens.
template <class Source> void CellDiffusion::build(Source& src)
{
    const int ce = 4 * m_ch[0]; // length of the vector that carries the noise level and the request, and of a token
    auto conv = [&](int cin, int cout, int stride) { return cellnet::readConv(src, cin, cout, stride); };
    auto norm = [&](int channels) { return cellnet::readNorm(src, channels); };
    auto linear = [&](int in, int out) { return cellnet::readLinear(src, in, out); };
    auto res = [&](int cin, int cout) { return cellnet::readRes(src, cin, cout, ce, m_tokens); };
    auto attn = [&](int c) { return cellnet::readAttn(src, c); };

    m_down.clear();
    m_up.clear();
    const int levels = int(m_ch.size());
    if (m_tokens) {
        const size_t count = m_targets.size() + 2; // the targets and the two numbers of the material
        m_tokenWeight = src.tensor(count * 2 * size_t(ce));
        m_tokenBias = src.tensor(count * size_t(ce));
    }
    m_time0 = linear(m_ch[0], ce);
    m_time1 = linear(ce, ce);
    m_cond0 = linear(m_nc, ce);
    m_cond1 = linear(ce, ce);
    m_inp = conv(m_zc, m_ch[0], 1);
    std::vector<int> kept{ m_ch[0] }; // channels of the maps the way down keeps for the way up
    int c = m_ch[0];
    for (int i = 0; i < levels; ++i) {
        const int co = m_ch[size_t(i)];
        for (int b = 0; b < m_nb; ++b) {
            Down d;
            d.res = res(c, co);
            if (m_attn[size_t(i)]) d.attn = attn(co);
            m_down.push_back(std::move(d));
            c = co;
            kept.push_back(c);
        }
        if (i < levels - 1) {
            Down d;
            d.isConv = true;
            d.conv = conv(c, c, 2);
            m_down.push_back(std::move(d));
            kept.push_back(c);
        }
    }
    m_m1 = res(c, c);
    m_ma = attn(c);
    m_m2 = res(c, c);
    for (int i = levels - 1; i >= 0; --i) {
        const int co = m_ch[size_t(i)];
        for (int j = 0; j <= m_nb; ++j) {
            Up u;
            u.res = res(c + kept.back(), co);
            kept.pop_back();
            if (m_attn[size_t(i)]) u.attn = attn(co);
            if (j == m_nb && i > 0) {
                u.hasUp = true;
                u.conv = conv(co, co, 1);
            }
            m_up.push_back(std::move(u));
            c = co;
        }
    }
    m_outNorm = norm(c);
    m_out = conv(c, m_predictsV ? 2 * m_zc : m_zc, 1); // v: a head of the noise and a head of the code
    if (m_tokens) {
        for (Down& d : m_down)
            if (!d.isConv) d.cross = cellnet::readCross(src, d.res.cout, ce);
        m_midCross = cellnet::readCross(src, m_m1.cout, ce);
        for (Up& u : m_up) u.cross = cellnet::readCross(src, u.res.cout, ce);
        m_token0 = linear(ce, ce);
        m_token1 = linear(ce, ce);
    }
}

bool CellDiffusion::open(const std::string& path, std::string* error)
{
    auto fail = [&](const char* what) {
        if (error) *error = what;
        *this = CellDiffusion();
        return false;
    };
    *this = CellDiffusion();
    FileReader r;
    r.f = std::fopen(path.c_str(), "rb");
    if (!r.f) return fail("cannot open the file");
    // "MVCELLF1": the first recipe (run dif2). "MVCELLF2" names the recipe after the sampler settings.
    char magic[8] = {};
    if (!r.take(magic, 8)) return fail("not a cell diffusion file");
    const bool second = std::memcmp(magic, "MVCELLF2", 8) == 0;
    if (!second && std::memcmp(magic, "MVCELLF1", 8) != 0) return fail("not a cell diffusion file");
    m_zc = r.i32();
    m_codeN = r.i32();
    m_nc = r.i32();
    m_nb = r.i32();
    const int levels = r.i32();
    if (!r.ok || m_zc < 1 || m_zc > 64 || m_codeN < 4 || m_codeN > 64 || m_nc < 2 || m_nc > 4096 || m_nb < 1 || m_nb > 8
        || levels < 1 || levels > 5 || m_codeN % (1 << (levels - 1)) != 0)
        return fail("bad header");
    for (int i = 0; i < levels; ++i) m_ch.push_back(r.i32());
    for (int i = 0; i < levels; ++i) m_attn.push_back(r.i32());
    m_heads = r.i32();
    m_groups = r.i32();
    m_T = r.i32();
    m_start = r.i32();
    m_steps = r.i32();
    if (!r.ok || m_heads < 1 || m_heads > 64 || m_groups < 1 || m_T < 2 || m_T > 100000 || m_start < 0 || m_start >= m_T
        || m_steps < 1 || m_steps > m_T)
        return fail("bad header");
    if (second) {
        const int returns = r.i32(), request = r.i32(); // 0 the noise / 1 v; 0 an added vector / 1 tokens as well
        if (!r.ok || returns < 0 || returns > 1 || request < 0 || request > 1) return fail("bad header");
        m_predictsV = returns == 1;
        m_tokens = request == 1;
    }
    for (int i = 0; i < levels; ++i) {
        const int c = m_ch[size_t(i)];
        if (c < m_groups || c > 1024 || c % m_groups != 0 || c % 2 != 0 || c % m_heads != 0) return fail("bad layer sizes");
    }
    m_offset = r.f32();
    m_clamp = r.f32();
    m_guidance = r.f32();
    r.i32(); // training step, part of the name
    m_name = r.str(64);
    m_note = r.str(4000);
    const int nt = r.i32();
    if (!r.ok || !(m_clamp > 0) || nt < 0 || 2 * nt + 2 != m_nc) return fail("bad header");
    for (int j = 0; j < nt; ++j) {
        Target t;
        t.key = r.str(32);
        t.kind = r.i32();
        t.lo = r.f32();
        t.hi = r.f32();
        t.mean = r.f32();
        t.std = r.f32();
        if (!r.ok || t.kind < 0 || t.kind > 3 || !(t.std > 0)) return fail("bad target table");
        m_targets.push_back(std::move(t));
    }
    for (float& v : m_matMean) v = r.f32();
    for (float& v : m_matStd) v = r.f32();
    m_codeMean = r.floats(size_t(m_zc));
    m_codeStd = r.floats(size_t(m_zc));
    if (!r.ok || !(m_matStd[0] > 0) || !(m_matStd[1] > 0)) return fail("truncated header");
    m_weightsAt = r.pos;
    // Share of the code left at every noise level (cosine schedule, Nichol & Dhariwal 2021).
    m_left.resize(size_t(m_T));
    auto shape = [&](int i) {
        const double v = std::cos((double(i) / m_T + 0.008) / 1.008 * 1.5707963267948966);
        return v * v;
    };
    double product = 1.0;
    for (int t = 0; t < m_T; ++t) {
        product *= 1.0 - std::min(std::max(1.0 - shape(t + 1) / shape(t), 0.0), 0.999);
        m_left[size_t(t)] = product;
    }

    // The size the weights must have for these layer sizes against the size of the file.
    Counter counter;
    build(counter);
    m_weights = counter.weights;
    if (std::fseek(r.f, 0, SEEK_END) != 0) return fail("cannot read the file");
    const long size = std::ftell(r.f);
    if (size < 0 || uint64_t(size) != m_weightsAt + counter.bytes) return fail("the weights do not fit the header (truncated file or another network)");
    m_path = path;
    m_opened = true;
    return true;
}

bool CellDiffusion::load(std::string* error)
{
    if (m_loaded) return true;
    auto fail = [&](const char* what) {
        if (error) *error = what;
        return false;
    };
    if (!m_opened) return fail("the file is not opened");
    FileReader r;
    r.f = std::fopen(m_path.c_str(), "rb");
    if (!r.f || std::fseek(r.f, long(m_weightsAt), SEEK_SET) != 0) return fail("cannot open the file");
    build(r);
    char extra = 0;
    if (!r.ok || std::fread(&extra, 1, 1, r.f) == 1) return fail("truncated or mismatched weights");
    m_loaded = true;
    return true;
}

bool CellDiffusion::targetRange(const std::string& key, double eGpa, double* lo, double* hi) const
{
    for (const Target& t : m_targets) {
        if (t.key != key) continue;
        double a = t.lo, b = t.hi;
        if (t.kind == 1 || t.kind == 3) { a = std::exp(a); b = std::exp(b); }
        if (t.kind == 2) { a = std::expm1(a); b = std::expm1(b); }
        if (t.kind == 3) { a *= eGpa * 1e9; b *= eGpa * 1e9; }
        if (lo) *lo = a;
        if (hi) *hi = b;
        return true;
    }
    return false;
}

// The request as cellgen Conditions.request builds it: 24 targets, 24 flags "given", the material.
std::vector<float> CellDiffusion::request(const std::map<std::string, double>& targets, double eGpa, double nu, double aniso,
                                          RequestNotes* notes) const
{
    if (!m_opened) return {};
    std::vector<float> v(size_t(m_nc), 0.0f);
    const size_t nt = m_targets.size();
    for (const auto& item : targets) {
        size_t j = 0;
        while (j < nt && m_targets[j].key != item.first) ++j;
        double x = item.second;
        bool usable = j < nt && std::isfinite(x);
        if (usable) {
            const int kind = m_targets[j].kind;
            if (kind == 3) x /= eGpa * 1e9;
            if (kind == 1 || kind == 3) usable = x > 0.0;
            if (kind == 2) usable = x > -1.0;
            if (usable && (kind == 1 || kind == 3)) x = std::log(x);
            if (usable && kind == 2) x = std::log1p(x);
        }
        if (!usable) {
            if (notes) notes->unknown.push_back(item.first);
            continue;
        }
        const Target& t = m_targets[j];
        const double inside = std::min(std::max(x, double(t.lo)), double(t.hi));
        v[j] = float((inside - t.mean) / t.std);
        v[nt + j] = 1.0f;
        if (notes) {
            notes->used.push_back(item.first);
            if (inside != x) notes->clipped.push_back(item.first);
        }
    }
    v[2 * nt] = float((nu - m_matMean[0]) / m_matStd[0]);
    v[2 * nt + 1] = float((aniso - m_matMean[1]) / m_matStd[1]);
    return v;
}

std::vector<float> CellDiffusion::startNoise(uint32_t seed) const
{
    if (!m_opened) return {};
    const size_t n3 = size_t(m_codeN) * m_codeN * m_codeN, total = size_t(m_zc) * n3;
    std::vector<float> e(total);
    std::mt19937 rng(seed);
    // Box-Muller on the raw generator: std::normal_distribution differs between standard libraries.
    auto uniform = [&]() { return (double(rng()) + 0.5) / 4294967296.0; };
    for (size_t i = 0; i < total; i += 2) {
        const double r = std::sqrt(-2.0 * std::log(uniform())), a = 6.283185307179586 * uniform();
        e[i] = float(r * std::cos(a));
        if (i + 1 < total) e[i + 1] = float(r * std::sin(a));
    }
    if (m_offset != 0.0f) {
        // One more number per channel, shared by all its positions: it noises the mean of the channel,
        // which holds the density of the cell, as strongly as everything else (cellgen diffusion_noise).
        const float scale = float(1.0 / std::sqrt(1.0 + double(m_offset) * m_offset));
        for (int c = 0; c < m_zc; ++c) {
            const double r = std::sqrt(-2.0 * std::log(uniform())), a = 6.283185307179586 * uniform();
            const float shift = m_offset * float(r * std::cos(a));
            float* v = e.data() + size_t(c) * n3;
            for (size_t i = 0; i < n3; ++i) v[i] = (v[i] + shift) * scale;
        }
    }
    return e;
}

// The request as the tokens of cellgen UNet.tokens: one per target (its value and its flag "given") and one per
// number of the material (flag 1). A token is value * its first row + flag * its second row + its bias, then two layers.
std::vector<float> CellDiffusion::requestTokens(const std::vector<float>& request) const
{
    if (!m_tokens) return {};
    const size_t nt = m_targets.size(), count = nt + 2, ce = size_t(m_token0.in);
    std::vector<float> tokens(count * ce), in(ce);
    for (size_t t = 0; t < count; ++t) {
        const float value = t < nt ? request[t] : request[2 * nt + (t - nt)], flag = t < nt ? request[nt + t] : 1.0f;
        const float* w = m_tokenWeight.data() + t * 2 * ce;
        const float* b = m_tokenBias.data() + t * ce;
        for (size_t i = 0; i < ce; ++i) in[i] = value * w[i] + flag * w[ce + i] + b[i];
        std::vector<float> token = dense(m_token0, in);
        silu(token);
        token = dense(m_token1, token);
        std::copy(token.begin(), token.end(), tokens.begin() + long(t * ce));
    }
    return tokens;
}

// The U-Net: 16^3 -> 8^3 -> 4^3 and back; on the way up every block gets the map kept at the same place on the way down.
// In the second recipe every block is followed by the attention of the grid to the tokens of the request.
void CellDiffusion::forward(const std::vector<float>& x, int level, const std::vector<float>& request,
                            const std::vector<float>& tokens, Scratch& s, std::vector<float>& out) const
{
    const int count = int(m_targets.size()) + 2;
    // The noise level as sines and cosines of falling frequency (in float, as PyTorch computes it), then two
    // layers; the request through two layers of its own; their sum goes to every residual block.
    std::vector<float> e = dense(m_time0, cellnet::levelWave(level, m_ch[0] / 2));
    silu(e);
    e = dense(m_time1, e);
    std::vector<float> q = dense(m_cond0, request);
    silu(q);
    q = dense(m_cond1, q);
    for (size_t i = 0; i < e.size(); ++i) e[i] += q[i];

    int n = m_codeN;
    std::vector<float> h, t;
    cellnet::conv3(m_inp, x, n, h, s);
    std::vector<std::vector<float>> kept;
    kept.push_back(h);
    for (const Down& d : m_down) {
        if (d.isConv) {
            cellnet::conv3(d.conv, h, n, t, s);
            h.swap(t);
            n /= 2;
        } else {
            cellnet::res(d.res, h, n, e, s, m_groups);
            if (d.attn.used) cellnet::attention(d.attn, h, n, s, m_groups, m_heads);
            if (d.cross.used) cellnet::crossAttention(d.cross, h, n, tokens, count, s, m_groups, m_heads);
        }
        kept.push_back(h);
    }
    cellnet::res(m_m1, h, n, e, s, m_groups);
    cellnet::attention(m_ma, h, n, s, m_groups, m_heads);
    if (m_midCross.used) cellnet::crossAttention(m_midCross, h, n, tokens, count, s, m_groups, m_heads);
    cellnet::res(m_m2, h, n, e, s, m_groups);
    for (const Up& u : m_up) {
        // The channels of the current map first, then those of the kept one.
        h.insert(h.end(), kept.back().begin(), kept.back().end());
        kept.pop_back();
        cellnet::res(u.res, h, n, e, s, m_groups);
        if (u.attn.used) cellnet::attention(u.attn, h, n, s, m_groups, m_heads);
        if (u.cross.used) cellnet::crossAttention(u.cross, h, n, tokens, count, s, m_groups, m_heads);
        if (u.hasUp) {
            // Every value repeated 2 x 2 x 2, then a convolution.
            const int c = u.res.cout, m = 2 * n;
            const size_t n3 = size_t(n) * n * n, m3 = size_t(m) * m * m;
            t.resize(size_t(c) * m3);
#pragma omp parallel for schedule(static)
            for (int ch = 0; ch < c; ++ch)
                for (int i = 0; i < m; ++i)
                    for (int j = 0; j < m; ++j) {
                        const float* src = h.data() + size_t(ch) * n3 + (size_t(i / 2) * n + j / 2) * n;
                        float* dst = t.data() + size_t(ch) * m3 + (size_t(i) * m + j) * m;
                        for (int k = 0; k < m; ++k) dst[k] = src[k / 2];
                    }
            cellnet::conv3(u.conv, t, m, h, s);
            n = m;
        }
    }
    cellnet::groupNorm(m_outNorm, h, size_t(n) * n * n, true, m_groups);
    cellnet::conv3(m_out, h, n, out, s);
    if (m_predictsV) {
        // Two heads: v = sqrt(a) * head of the noise - sqrt(1 - a) * head of the code, a = share of the code left
        // (in float, as the network computes it).
        const float a = float(m_left[size_t(level)]), keep = std::sqrt(a), noisy = std::sqrt(1.0f - a);
        const size_t half = out.size() / 2;
        for (size_t i = 0; i < half; ++i) out[i] = keep * out[i] - noisy * out[half + i];
        out.resize(half);
    }
}

std::vector<float> CellDiffusion::predict(const std::vector<float>& x, int level, const std::vector<float>& request) const
{
    if (!m_loaded || x.size() != codeLength() || request.size() != size_t(m_nc) || level < 0 || level >= m_T) return {};
    Scratch s;
    std::vector<float> out;
    forward(x, level, request, requestTokens(request), s, out);
    return out;
}

// DDIM without added noise, as cellgen diffusion.sample: at every level the network names the noise (or v), the
// clean code is estimated from it (and kept inside +-clamp), and the code is noised back to the next, lower level.
// With a guidance other than 1 the network is asked twice per level, with the request and with its targets hidden,
// and the answer is hidden + guidance * (with the request - hidden).
std::vector<float> CellDiffusion::sample(const std::vector<float>& request, const std::vector<float>& noise,
                                         const std::function<bool(int, int)>& progress, int steps) const
{
    if (!m_loaded || noise.size() != codeLength() || request.size() != size_t(m_nc)) return {};
    if (steps < 1) steps = m_steps;
    // The levels visited: evenly from the start level down to 0 (halves round to even, as numpy does).
    std::vector<int> ts(static_cast<size_t>(steps), m_start);
    for (int i = 1; i < steps; ++i) ts[size_t(i)] = int(std::nearbyint(m_start - double(m_start) * i / (steps - 1)));
    if (steps > 1) ts.back() = 0;

    // The request with the targets hidden (the material stays), for guidance other than 1.
    std::vector<float> bare = request;
    std::fill(bare.begin(), bare.begin() + long(2 * m_targets.size()), 0.0f);
    const bool guided = m_guidance != 1.0f;
    const std::vector<float> tokens = requestTokens(request), bareTokens = guided ? requestTokens(bare) : std::vector<float>();
    Scratch s;
    std::vector<float> x = noise, e, eBare, x0(x.size());
    for (int i = 0; i < steps; ++i) {
        if (progress && !progress(i, steps)) return {};
        const int t = ts[size_t(i)];
        forward(x, t, request, tokens, s, e);
        if (guided) {
            forward(x, t, bare, bareTokens, s, eBare);
            for (size_t j = 0; j < e.size(); ++j) e[j] = eBare[j] + m_guidance * (e[j] - eBare[j]);
        }
        const double a = m_left[size_t(t)];
        const float keep = float(std::sqrt(a)), noisy = float(std::sqrt(1.0 - a));
        if (m_predictsV) {
            // code = sqrt(a) x - sqrt(1 - a) v: no division, so it holds at pure noise too.
            for (size_t j = 0; j < x.size(); ++j) x0[j] = std::min(std::max(keep * x[j] - noisy * e[j], -m_clamp), m_clamp);
        } else {
            for (size_t j = 0; j < x.size(); ++j) x0[j] = std::min(std::max((x[j] - noisy * e[j]) / keep, -m_clamp), m_clamp);
        }
        if (i + 1 == steps) break;
        const double a2 = m_left[size_t(ts[size_t(i + 1)])];
        const float keep2 = float(std::sqrt(a2)), noisy2 = float(std::sqrt(1.0 - a2));
        for (size_t j = 0; j < x.size(); ++j) x[j] = keep2 * x0[j] + noisy2 * ((x[j] - keep * x0[j]) / noisy);
    }
    return x0;
}

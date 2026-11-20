#include "cellpredictor.h"

#include <cmath>
#include <cstring>

using cellnet::Counter;
using cellnet::FileReader;
using cellnet::dense;
using cellnet::silu;

// The same walks as Encoder.__init__ in cellgen model.py and Predictor.__init__ in predictor.py; the state dicts
// list the tensors in this order.
template <class Source> void CellPredictor::build(Source& src)
{
    // Encoder: stem^3 channels of packed voxels -> level 0, then a stride-2 convolution before every next level.
    const int encLevels = int(m_encCh.size());
    m_encDown.clear();
    m_encRes.assign(size_t(encLevels), {});
    m_encInp = cellnet::readConv(src, m_stem * m_stem * m_stem, m_encCh[0], 1);
    for (int lvl = 0; lvl < encLevels; ++lvl) {
        const int c = m_encCh[size_t(lvl)];
        if (lvl > 0) m_encDown.push_back(cellnet::readConv(src, m_encCh[size_t(lvl - 1)], c, 2));
        for (int b = 0; b < m_encBlocks[size_t(lvl)]; ++b) m_encRes[size_t(lvl)].push_back(cellnet::readRes(src, c, c, 0));
    }
    m_encNorm = cellnet::readNorm(src, m_encCh.back());
    m_encOut = cellnet::readConv(src, m_encCh.back(), 2 * m_zc, 1); // mean and log-variance of the code

    // Predictor.
    const int levels = int(m_ch.size()), ce = 4 * m_ch[0]; // ce: length of the vector that carries the noise level and the material
    m_down.clear();
    m_time0 = cellnet::readLinear(src, m_ch[0], ce);
    m_time1 = cellnet::readLinear(src, ce, ce);
    m_mat0 = cellnet::readLinear(src, 2, ce);
    m_mat1 = cellnet::readLinear(src, ce, ce);
    m_inp = cellnet::readConv(src, m_zc, m_ch[0], 1);
    int c = m_ch[0];
    for (int i = 0; i < levels; ++i) {
        const int co = m_ch[size_t(i)];
        for (int b = 0; b < m_nb; ++b) {
            Down d;
            d.res = cellnet::readRes(src, c, co, ce);
            if (m_attn[size_t(i)]) d.attn = cellnet::readAttn(src, co);
            m_down.push_back(std::move(d));
            c = co;
        }
        if (i < levels - 1) {
            Down d;
            d.isConv = true;
            d.conv = cellnet::readConv(src, c, c, 2);
            m_down.push_back(std::move(d));
        }
    }
    m_norm = cellnet::readNorm(src, c);
    m_head0 = cellnet::readLinear(src, c, m_hidden);
    m_head1 = cellnet::readLinear(src, m_hidden, int(m_targets.size()));
}

bool CellPredictor::load(const std::string& path, std::string* error)
{
    auto fail = [&](const char* what) {
        if (error) *error = what;
        *this = CellPredictor();
        return false;
    };
    *this = CellPredictor();
    FileReader r;
    r.f = std::fopen(path.c_str(), "rb");
    if (!r.f) return fail("cannot open the file");
    char magic[8] = {};
    if (!r.take(magic, 8) || std::memcmp(magic, "MVCELLP1", 8) != 0) return fail("not a cell predictor file");
    m_cellN = r.i32();
    m_zc = r.i32();
    m_codeN = r.i32();
    m_stem = r.i32();
    m_groups = r.i32();
    m_heads = r.i32();
    const int encLevels = r.i32();
    if (!r.ok || m_zc < 1 || m_zc > 64 || m_codeN < 4 || m_codeN > 64 || m_stem < 1 || m_stem > 4 || m_groups < 1 || m_heads < 1
        || m_heads > 64 || encLevels < 1 || encLevels > 5 || m_cellN != m_codeN * m_stem * (1 << (encLevels - 1)) || m_cellN / m_stem > 64)
        return fail("bad header");
    for (int i = 0; i < encLevels; ++i) m_encCh.push_back(r.i32());
    for (int i = 0; i < encLevels; ++i) m_encBlocks.push_back(r.i32());
    const int levels = r.i32();
    if (!r.ok || levels < 1 || levels > 5 || m_codeN % (1 << (levels - 1)) != 0) return fail("bad header");
    for (int i = 0; i < levels; ++i) m_ch.push_back(r.i32());
    for (int i = 0; i < levels; ++i) m_attn.push_back(r.i32());
    m_nb = r.i32();
    m_hidden = r.i32();
    const int nm = r.i32();
    if (!r.ok || m_nb < 1 || m_nb > 8 || m_hidden < 1 || m_hidden > 65536 || nm != 2) return fail("bad header");
    for (int i = 0; i < encLevels; ++i) {
        const int c = m_encCh[size_t(i)], b = m_encBlocks[size_t(i)];
        if (c < m_groups || c > 1024 || c % m_groups != 0 || b < 0 || b > 8) return fail("bad layer sizes");
    }
    for (int i = 0; i < levels; ++i) {
        const int c = m_ch[size_t(i)];
        if (c < m_groups || c > 1024 || c % m_groups != 0 || c % 2 != 0 || c % m_heads != 0) return fail("bad layer sizes");
    }
    r.i32(); // training step, part of the name
    m_name = r.str(64);
    m_note = r.str(4000);
    const int nt = r.i32();
    if (!r.ok || nt < 1 || nt > 4096) return fail("bad header");
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
    for (float v : m_codeStd)
        if (!(v > 0)) return fail("bad code statistics");

    // The size the weights must have for these layer sizes against the size of the file.
    Counter counter;
    build(counter);
    const uint64_t weightsAt = r.pos;
    if (std::fseek(r.f, 0, SEEK_END) != 0) return fail("cannot read the file");
    const long size = std::ftell(r.f);
    if (size < 0 || uint64_t(size) != weightsAt + counter.bytes) return fail("the weights do not fit the header (truncated file or another network)");
    if (std::fseek(r.f, long(weightsAt), SEEK_SET) != 0) return fail("cannot read the file");
    build(r);
    if (!r.ok) return fail("truncated or mismatched weights");
    m_weights = counter.weights;
    m_path = path;
    m_loaded = true;
    return true;
}

// Encoder of the autoencoder (cellgen model.py): every stem^3 block of voxels becomes stem^3 channels (solid +1,
// void -1), then convolutions and residual blocks down to the code grid; the first half of the output channels is
// the mean of the code, which is the code of the cell.
std::vector<float> CellPredictor::encode(const std::vector<uint8_t>& cell) const
{
    const size_t n = size_t(m_cellN);
    if (!m_loaded || cell.size() != n * n * n) return {};
    const int s = m_stem;
    int m = m_cellN / s;
    const size_t m3 = size_t(m) * m * m;
    std::vector<float> x(size_t(s * s * s) * m3);
    for (int a = 0; a < s; ++a)
        for (int b = 0; b < s; ++b)
            for (int c = 0; c < s; ++c) {
                float* dst = x.data() + size_t((a * s + b) * s + c) * m3;
                for (int i = 0; i < m; ++i)
                    for (int j = 0; j < m; ++j) {
                        const uint8_t* src = cell.data() + (size_t(i * s + a) * n + size_t(j * s + b)) * n + size_t(c);
                        float* d = dst + (size_t(i) * m + j) * m;
                        for (int k = 0; k < m; ++k) d[k] = src[size_t(k) * s] ? 1.0f : -1.0f;
                    }
            }
    cellnet::Scratch scratch;
    const std::vector<float> none;
    std::vector<float> h, t;
    cellnet::conv3(m_encInp, x, m, h, scratch);
    for (size_t lvl = 0; lvl < m_encRes.size(); ++lvl) {
        if (lvl > 0) {
            cellnet::conv3(m_encDown[lvl - 1], h, m, t, scratch);
            h.swap(t);
            m /= 2;
        }
        for (const cellnet::Res& r : m_encRes[lvl]) cellnet::res(r, h, m, none, scratch, m_groups);
    }
    const size_t c3 = size_t(m) * m * m;
    cellnet::groupNorm(m_encNorm, h, c3, true, m_groups);
    cellnet::conv3(m_encOut, h, m, t, scratch);
    std::vector<float> code(size_t(m_zc) * c3);
    for (int c = 0; c < m_zc; ++c) {
        const float mean = m_codeMean[size_t(c)], inv = 1.0f / m_codeStd[size_t(c)];
        const float* src = t.data() + size_t(c) * c3;
        float* dst = code.data() + size_t(c) * c3;
        for (size_t i = 0; i < c3; ++i) dst[i] = (src[i] - mean) * inv;
    }
    return code;
}

// The predictor: the noise level and the material become one vector that goes to every residual block; the map is
// compressed 16^3 -> 8^3 -> 4^3, averaged over the grid and read by two fully connected layers.
std::vector<float> CellPredictor::scaled(const std::vector<float>& code, int level, double nu, double aniso) const
{
    if (!m_loaded || code.size() != codeLength()) return {};
    std::vector<float> e = dense(m_time0, cellnet::levelWave(level, m_ch[0] / 2));
    silu(e);
    e = dense(m_time1, e);
    std::vector<float> q = dense(m_mat0, { float((nu - m_matMean[0]) / m_matStd[0]), float((aniso - m_matMean[1]) / m_matStd[1]) });
    silu(q);
    q = dense(m_mat1, q);
    for (size_t i = 0; i < e.size(); ++i) e[i] += q[i];

    int n = m_codeN;
    cellnet::Scratch scratch;
    std::vector<float> h, t;
    cellnet::conv3(m_inp, code, n, h, scratch);
    for (const Down& d : m_down) {
        if (d.isConv) {
            cellnet::conv3(d.conv, h, n, t, scratch);
            h.swap(t);
            n /= 2;
        } else {
            cellnet::res(d.res, h, n, e, scratch, m_groups);
            if (d.attn.used) cellnet::attention(d.attn, h, n, scratch, m_groups, m_heads);
        }
    }
    const size_t n3 = size_t(n) * n * n;
    cellnet::groupNorm(m_norm, h, n3, true, m_groups);
    std::vector<float> pooled(size_t(m_norm.channels));
    for (int c = 0; c < m_norm.channels; ++c) {
        const float* v = h.data() + size_t(c) * n3;
        double sum = 0.0;
        for (size_t i = 0; i < n3; ++i) sum += v[i];
        pooled[size_t(c)] = float(sum / double(n3));
    }
    std::vector<float> y = dense(m_head0, pooled);
    silu(y);
    return dense(m_head1, y);
}

double CellPredictor::value(size_t index, float scaled, double eGpa) const
{
    const Target& t = m_targets[index];
    const double v = double(scaled) * t.std + t.mean;
    switch (t.kind) {
    case 1: return std::exp(v);
    case 2: return std::expm1(v);
    case 3: return std::exp(v) * eGpa * 1e9;
    default: return v;
    }
}

std::map<std::string, double> CellPredictor::predict(const std::vector<uint8_t>& cell, double eGpa, double nu, double aniso) const
{
    std::map<std::string, double> out;
    const std::vector<float> z = scaled(encode(cell), 0, nu, aniso);
    for (size_t j = 0; j < z.size(); ++j) out[m_targets[j].key] = value(j, z[j], eGpa);
    return out;
}

#ifndef CELLDIFFUSION_H
#define CELLDIFFUSION_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "cellnet.h"

/**
 * @brief Diffusion network of the cell generator: requested properties -> code 16 x 16 x 16 x 4.
 *
 * The second network of the Neural tab. CellDecoder turns a code into a voxel cell;
 * this one writes the code for a request. It starts from noise and removes it in
 * 50 steps (DDIM); at every step a 3D U-Net looks at the noisy code, the noise
 * level and the 50 numbers of the request. The file says which of two recipes the
 * network was trained with:
 *   - the first (run dif2, 23.5 M weights): the network returns the noise, the
 *     request is one vector added in every block, the sampling starts at level 980;
 *   - the second (runs dif3 - dif6, 25.5 M weights): the network returns
 *     v = sqrt(a) noise - sqrt(1 - a) code (a = share of the code left at the level),
 *     from which the clean code follows at any level, pure noise included, so the
 *     sampling starts at level 999; the request also enters as 26 tokens (24 targets
 *     with their flags, 2 numbers of the material) that every position of the grid
 *     looks at after every block, and the vector of the level and the request scales
 *     the channels of a block as well as shifting them.
 * With a guidance other than 1 every step asks the network twice, with the request
 * and with its targets hidden, and takes hidden + guidance * (with - hidden).
 *
 * The network is trained outside the program (cellgen: diffusion.py, train_dif.py)
 * and exported by tools/neural/export_cell_diffusion.py; this is its forward pass
 * and sampler in plain C++ (no Qt, no ML library) on the layers of cellnet.h. A
 * convolution is one matrix product (weights x the input unrolled over the 27 taps);
 * on macOS the product is Accelerate's sgemm (define MATVIZ_USE_ACCELERATE), elsewhere
 * a plain OpenMP loop, several times slower.
 *
 * The file is about 100 MB, so open() reads its header only (sizes, the constants of
 * the request) and load() the weights; load(), predict() and sample() are for one
 * thread at a time.
 *
 * Arrays are flat, the first index slowest: a code is [channel][i][j][k], as in CellDecoder.
 */
class CellDiffusion
{
public:
    /// One target of a request and the constants the training scaled it with.
    struct Target {
        std::string key;        ///< "Ex", "density", ...: the names of cellgen props.KEYS
        int kind = 0;           ///< 0 as is, 1 ln(x), 2 ln(1 + x), 3 ln(x / E of the material)
        float lo = 0, hi = 0;   ///< range of the training cells, in the scale of `kind`
        float mean = 0, std = 1;
    };
    /// What request() did with the targets it was given.
    struct RequestNotes {
        std::vector<std::string> used;      ///< passed to the network
        std::vector<std::string> clipped;   ///< passed, but outside the training range: its edge was taken
        std::vector<std::string> unknown;   ///< not passed: no such target, or not positive where a logarithm is taken
    };

    /// Read the header of the exported file and check its size. False (and *error) when it is not one.
    bool open(const std::string& path, std::string* error = nullptr);
    bool opened() const { return m_opened; }
    /// Read the weights of the opened file (once; later calls return at once).
    bool load(std::string* error = nullptr);
    bool loaded() const { return m_loaded; }

    const std::string& path() const { return m_path; }
    /// "dif6 step 5000": the training run the weights come from.
    const std::string& name() const { return m_name; }
    /// What was measured on the cells of this model (written at export).
    const std::string& note() const { return m_note; }
    size_t weightCount() const { return m_weights; }
    int codeChannels() const { return m_zc; }
    int codeSize() const { return m_codeN; }
    size_t codeLength() const { return size_t(m_zc) * m_codeN * m_codeN * m_codeN; }
    int requestSize() const { return m_nc; }
    /// Steps of the sampler and the noise level (of levels()) it starts from.
    int steps() const { return m_steps; }
    int startLevel() const { return m_start; }
    int levels() const { return m_T; }
    float offset() const { return m_offset; }
    float clamp() const { return m_clamp; }
    float guidance() const { return m_guidance; }
    /// The second recipe: the network returns v instead of the noise.
    bool predictsV() const { return m_predictsV; }
    /// The second recipe: the request also enters as tokens the grid looks at.
    bool usesTokens() const { return m_tokens; }
    /// Passes of the network per sampling step: 2 with a guidance other than 1.
    int passesPerStep() const { return m_guidance != 1.0f ? 2 : 1; }
    const std::vector<Target>& targets() const { return m_targets; }
    /// Range of a target among the training cells in its own units (Pa for a modulus), for a material of eGpa.
    bool targetRange(const std::string& key, double eGpa, double* lo, double* hi) const;
    /// Mean and spread of the code channels: code of the decoder = network's code * spread + mean.
    const std::vector<float>& codeMean() const { return m_codeMean; }
    const std::vector<float>& codeSpread() const { return m_codeStd; }

    /**
     * The 50 numbers of a request. targets: key -> value in the units of the dataset
     * (moduli in Pa, ratios and the density as they are); any subset may be given.
     * Material: Young's modulus in GPa, Poisson's ratio, and E across the layers / E
     * along them (1 = isotropic). Needs open() only.
     */
    std::vector<float> request(const std::map<std::string, double>& targets, double eGpa, double nu, double aniso,
                               RequestNotes* notes = nullptr) const;
    /// The noise the sampling starts from: white noise plus one shared number per channel (the offset of the training).
    std::vector<float> startNoise(uint32_t seed) const;
    /// One pass of the network on the code x at the noise level `level`: the noise it sees in x, or v (predictsV()).
    std::vector<float> predict(const std::vector<float>& x, int level, const std::vector<float>& request) const;
    /**
     * The code for a request, in the network's scale (zero mean, unit spread per channel).
     * progress(step, steps) is called before every step; returning false stops the
     * sampling (an empty vector comes back). steps = 0 takes the count of the file.
     */
    std::vector<float> sample(const std::vector<float>& request, const std::vector<float>& noise,
                              const std::function<bool(int, int)>& progress = {}, int steps = 0) const;

private:
    using Conv = cellnet::Conv;
    using Norm = cellnet::Norm;
    using Linear = cellnet::Linear;
    using Res = cellnet::Res;
    using Attn = cellnet::Attn;
    using Cross = cellnet::Cross;
    using Scratch = cellnet::Scratch;
    struct Down { bool isConv = false; Res res; Attn attn; Conv conv; Cross cross; };
    struct Up { Res res; Attn attn; bool hasUp = false; Conv conv; Cross cross; };

    template <class Source> void build(Source& src);
    /// The request as tokens (a row of 4 * channels[0] numbers each); empty in the first recipe.
    std::vector<float> requestTokens(const std::vector<float>& request) const;
    void forward(const std::vector<float>& x, int level, const std::vector<float>& request, const std::vector<float>& tokens,
                 Scratch& s, std::vector<float>& out) const;

    bool m_opened = false, m_loaded = false;
    std::string m_path, m_name, m_note;
    uint64_t m_weightsAt = 0;       // offset of the first tensor in the file
    size_t m_weights = 0;
    int m_zc = 0, m_codeN = 0, m_nc = 0, m_nb = 0, m_heads = 4, m_groups = 8, m_T = 1000, m_start = 980, m_steps = 50;
    std::vector<int> m_ch, m_attn;
    float m_offset = 0, m_clamp = 7, m_guidance = 1;
    bool m_predictsV = false, m_tokens = false;
    std::vector<double> m_left;     // share of the code left at every noise level (cosine schedule)
    std::vector<Target> m_targets;
    float m_matMean[2] = { 0, 0 }, m_matStd[2] = { 1, 1 };
    std::vector<float> m_codeMean, m_codeStd;
    // The layers, in the order of the forward pass.
    std::vector<float> m_tokenWeight, m_tokenBias;  // [token][value, flag][numbers], [token][numbers]
    Linear m_time0, m_time1, m_cond0, m_cond1, m_token0, m_token1;
    Conv m_inp, m_out;
    std::vector<Down> m_down;
    Res m_m1, m_m2;
    Attn m_ma;
    Cross m_midCross;
    std::vector<Up> m_up;
    Norm m_outNorm;
};

#endif // CELLDIFFUSION_H

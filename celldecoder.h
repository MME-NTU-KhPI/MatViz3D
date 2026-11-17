#ifndef CELLDECODER_H
#define CELLDECODER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Decoder of the cell autoencoder: code 16 x 16 x 16 x 4 -> voxel cell 64^3.
 *
 * The network is trained outside the program (cellgen: model.py, train_ae.py) and
 * exported by tools/neural/export_cell_decoder.py; this is its forward pass in
 * plain C++ (no Qt, no ML library): circular 3x3x3 convolutions, GroupNorm, SiLU,
 * nearest-neighbour upsampling, and the last step that unpacks 8 channels into
 * 2 x 2 x 2 voxels. Every convolution wraps around, so the cell is periodic.
 *
 * Arrays are flat, the first index slowest: a code is [channel][i][j][k], a cell
 * [i][j][k] with i, j, k the program's voxel axes x, y, z.
 */
class CellDecoder
{
public:
    /// Read the exported file from memory. False (and *error) when it is not one.
    bool load(const unsigned char* data, size_t size, std::string* error = nullptr);
    bool loaded() const { return !m_convs.empty(); }

    int codeChannels() const { return m_zc; }
    int codeSize() const { return m_codeN; }
    /// Voxels per edge of the decoded cell.
    int cellSize() const;
    /// Share of solid from which a voxel counts as solid.
    float threshold() const { return m_threshold; }
    size_t weightCount() const { return m_weights; }
    /// "ae1 step 16965": the training run the weights come from.
    const std::string& name() const { return m_name; }
    const std::vector<float>& codeMean() const { return m_mean; }
    const std::vector<float>& codeSpread() const { return m_std; }

    /// Logits of the share of solid, cellSize()^3 values; sigmoid(logit) >= threshold() is solid.
    std::vector<float> decodeLogits(const std::vector<float>& code) const;
    /// The decoded cell: 1 = solid, 0 = void.
    std::vector<uint8_t> decode(const std::vector<float>& code) const;

    /**
     * A random code with the per-channel mean and spread of the training codes.
     * smooth > 0 blurs the noise with a periodic Gaussian of that many code
     * positions and scales it back to the channel's spread (codes of real cells
     * are smooth: neighbouring positions correlate at 0.64).
     */
    std::vector<float> randomCode(uint32_t seed, double smooth = 0.0) const;

    /**
     * Keep the largest connected body of a cell [i][j][k] of n^3 voxels and clear the
     * rest. Bodies are 6-connected and the cell is periodic: a piece that leaves through
     * a face continues at the opposite one. Returns the voxels cleared; *pieces is the
     * number of bodies removed. A generated cell often carries a few loose specks next
     * to its body, and a loose piece cannot be printed.
     */
    static size_t keepLargestBody(std::vector<uint8_t>& cell, int n, int* pieces = nullptr);

private:
    struct Conv { int cin = 0, cout = 0; std::vector<float> w, b; };
    struct Norm { int channels = 0; std::vector<float> w, b; };

    void conv3(const Conv& c, const std::vector<float>& in, int n, std::vector<float>& out) const;
    void normSilu(const Norm& g, std::vector<float>& x, int n) const;

    int m_zc = 0, m_codeN = 0, m_stem = 1, m_groups = 8;
    std::vector<int> m_channels, m_blocks;
    float m_threshold = 0.5f;
    std::vector<float> m_mean, m_std;
    std::string m_name;
    size_t m_weights = 0;
    // In the order of the forward pass.
    std::vector<Conv> m_convs;
    std::vector<Norm> m_norms;
};

#endif // CELLDECODER_H

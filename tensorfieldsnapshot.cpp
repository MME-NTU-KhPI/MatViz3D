#include "tensorfieldsnapshot.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QDebug>
#include <QElapsedTimer>

#include "ansyswrapper.h"
#include "stressresult.h"

namespace {

// Corner node offsets of one element, identical to the node_coordinates table
// in OpenGLWidgetQML::calculateScene().  Order is irrelevant here (we only
// average), but keeping it the same makes the two easy to diff.
constexpr float kNodeOffsets[8][3] = {
    {0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0},
    {1, 0, 0}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0},
};

mvt::Sym3 toSym3(const std::array<float, 6>& c)
{
    // Pipeline order: xx, yy, zz, xy, yz, xz.
    return mvt::Sym3{ c[0], c[1], c[2], c[3], c[4], c[5] };
}

/// 99th percentile of max|eigenvalue| over occupied voxels.
///
/// Subsampled with a stride chosen to keep this near 200k eigendecompositions
/// regardless of N, so the cost is flat (~40 ms) instead of growing as N^3.
/// A percentile only needs a representative sample, and a regular stride over a
/// grain structure is representative -- the quantity has no high-frequency
/// structure that aliasing could hide.
float percentileLambda(const TensorFieldSnapshot& s, TensorSource src)
{
    if (!s.has(src)) return 0.0f;

    const size_t total  = static_cast<size_t>(s.N) * s.N * s.N;
    const size_t target = 200000;
    const size_t stride = std::max<size_t>(1, total / target);

    std::vector<float> peak;
    peak.reserve(total / stride + 1);

    const auto& src_v = (src == TensorSource::Stress) ? s.stress : s.strain;
    for (size_t idx = 0; idx < total; idx += stride) {
        if (s.grainId[idx] == 0) continue;
        const mvt::Eig3 e = mvt::eigenSym3(toSym3(src_v[idx]));
        peak.push_back(static_cast<float>(
            std::max(std::abs(e.lambda[0]), std::abs(e.lambda[2]))));
    }
    if (peak.empty()) return 0.0f;

    const size_t k = static_cast<size_t>(0.99 * (peak.size() - 1));
    std::nth_element(peak.begin(), peak.begin() + k, peak.end());
    const float v = peak[k];

    // Degenerate but nonzero fields (e.g. a perfectly uniform stress state)
    // still need a positive reference; fall back to the sample maximum.
    if (v > 0.0f) return v;
    return *std::max_element(peak.begin(), peak.end());
}

void finalize(TensorFieldSnapshot& s)
{
    s.lambdaRefStress = percentileLambda(s, TensorSource::Stress);
    s.lambdaRefStrain = percentileLambda(s, TensorSource::Strain);
}

void fillGrainIds(TensorFieldSnapshot& s, int32_t*** voxels)
{
    s.grainId.assign(static_cast<size_t>(s.N) * s.N * s.N, 0);
    if (!voxels) return;
    for (int i = 0; i < s.N; ++i)          // y
        for (int j = 0; j < s.N; ++j)      // z
            for (int k = 0; k < s.N; ++k)  // x
                s.grainId[static_cast<size_t>(s.denseIndex(k, i, j))] = voxels[k][i][j];
}

} // namespace

// ---------------------------------------------------------------------------
//  Sampling
// ---------------------------------------------------------------------------
bool TensorFieldSnapshot::voxelTensor(int x, int y, int z, TensorSource s, mvt::Sym3& out) const
{
    if (!has(s) || !occupied(x, y, z)) return false;
    const size_t idx = static_cast<size_t>(denseIndex(x, y, z));
    out = toSym3((s == TensorSource::Stress) ? stress[idx] : strain[idx]);
    return true;
}

bool TensorFieldSnapshot::sampleTensor(const mvt::Vec3& p, TensorSource s, mvt::Sym3& out) const
{
    if (!has(s)) return false;

    const int x0 = static_cast<int>(std::floor(p[0]));
    const int y0 = static_cast<int>(std::floor(p[1]));
    const int z0 = static_cast<int>(std::floor(p[2]));
    const double fx = p[0] - x0, fy = p[1] - y0, fz = p[2] - z0;

    // Gather the 8 corners, noting which are usable.
    std::array<float, 6> c[8];
    bool ok[8];
    int  firstOk = -1;
    for (int n = 0; n < 8; ++n) {
        const int xx = x0 + ((n & 1) ? 1 : 0);
        const int yy = y0 + ((n & 2) ? 1 : 0);
        const int zz = z0 + ((n & 4) ? 1 : 0);
        ok[n] = occupied(xx, yy, zz);
        if (ok[n]) {
            const size_t idx = static_cast<size_t>(denseIndex(xx, yy, zz));
            c[n] = (s == TensorSource::Stress) ? stress[idx] : strain[idx];
            if (firstOk < 0) firstOk = n;
        }
    }
    if (firstOk < 0) return false;

    // Void fill: nearest occupied corner, by Hamming distance in the corner
    // cube (adjacent corners differ in one bit).  Substituting a zero tensor
    // instead would pull the interpolated principal direction toward the
    // boundary and put a spurious bend in every streamline that grazes one.
    for (int n = 0; n < 8; ++n) {
        if (ok[n]) continue;
        int best = firstOk, bestDist = 4;
        for (int m = 0; m < 8; ++m) {
            if (!ok[m]) continue;
            int d = 0;
            for (int b = 0; b < 3; ++b) if (((n ^ m) >> b) & 1) ++d;
            if (d < bestDist) { bestDist = d; best = m; }
        }
        c[n] = c[best];
    }

    const double w[8] = {
        (1 - fx) * (1 - fy) * (1 - fz), fx * (1 - fy) * (1 - fz),
        (1 - fx) * fy       * (1 - fz), fx * fy       * (1 - fz),
        (1 - fx) * (1 - fy) * fz,       fx * (1 - fy) * fz,
        (1 - fx) * fy       * fz,       fx * fy       * fz,
    };

    double acc[6] = {0, 0, 0, 0, 0, 0};
    for (int n = 0; n < 8; ++n)
        for (int t = 0; t < 6; ++t) acc[t] += w[n] * c[n][t];

    out = mvt::Sym3{ acc[0], acc[1], acc[2], acc[3], acc[4], acc[5] };
    return true;
}

// ---------------------------------------------------------------------------
//  FFT builder
// ---------------------------------------------------------------------------
std::shared_ptr<const TensorFieldSnapshot>
buildSnapshotFromFFT(const FieldVisualizationData& field, int32_t*** voxels, int component)
{
    QElapsedTimer timer;
    timer.start();

    auto s = std::make_shared<TensorFieldSnapshot>();
    s->N = field.numCubes;
    if (s->N <= 0) return s;

    const size_t total = static_cast<size_t>(s->N) * s->N * s->N;
    fillGrainIds(*s, voxels);

    const int sIdx[6] = { SX, SY, SZ, SXY, SYZ, SXZ };
    const int eIdx[6] = { EpsX, EpsY, EpsZ, EpsXY, EpsYZ, EpsXZ };

    auto sourceValid = [&](const int idx[6]) {
        for (int t = 0; t < 6; ++t)
            if (!field.componentValid[idx[t]] || field.perVoxel[idx[t]].size() != total)
                return false;
        return true;
    };

    if (sourceValid(sIdx)) {
        s->hasStress = true;
        s->stress.resize(total);
        for (size_t v = 0; v < total; ++v)
            for (int t = 0; t < 6; ++t)
                s->stress[v][t] = field.perVoxel[sIdx[t]][v];
    }

    if (sourceValid(eIdx)) {
        s->hasStrain = true;
        s->strain.resize(total);
        for (size_t v = 0; v < total; ++v) {
            for (int t = 0; t < 3; ++t) s->strain[v][t] = field.perVoxel[eIdx[t]][v];
            // Engineering -> tensor shear.  See kEngineeringToTensorShear.
            for (int t = 3; t < 6; ++t)
                s->strain[v][t] = field.perVoxel[eIdx[t]][v] * kEngineeringToTensorShear;
        }
    }

    // The FFT solver has no nodal displacement field, so hasDisp stays false;
    // the deformed view approximates it affinely from macroStrain instead.

    if (component >= 0 && component <= EpsEQV &&
        field.componentValid[component] && field.perVoxel[component].size() == total)
    {
        s->scalarMin = field.componentMin[component];
        s->scalarMax = field.componentMax[component];
        const float span = (s->scalarMax > s->scalarMin) ? (s->scalarMax - s->scalarMin) : 0.0f;
        s->scalar.resize(total);
        for (size_t v = 0; v < total; ++v) {
            const float raw = field.perVoxel[component][v];
            s->scalar[v] = (span > 0.0f)
                             ? std::clamp((raw - s->scalarMin) / span, 0.0f, 1.0f)
                             : 1.0f;
        }
    }

    finalize(*s);
    qDebug() << "buildSnapshotFromFFT: N =" << s->N
             << "stress =" << s->hasStress << "strain =" << s->hasStrain
             << "lambdaRef(s/e) =" << s->lambdaRefStress << s->lambdaRefStrain
             << "in" << timer.elapsed() << "ms";
    return s;
}

// ---------------------------------------------------------------------------
//  ANSYS builder
// ---------------------------------------------------------------------------
std::shared_ptr<const TensorFieldSnapshot>
buildSnapshotFromAnsys(ansysWrapper& wr, int numCubes, int32_t*** voxels, int component)
{
    QElapsedTimer timer;
    timer.start();

    auto s = std::make_shared<TensorFieldSnapshot>();
    s->N = numCubes;
    if (s->N <= 0) return s;

    const size_t total = static_cast<size_t>(s->N) * s->N * s->N;
    fillGrainIds(*s, voxels);

    s->hasStress = true;
    s->hasStrain = true;
    s->hasDisp   = true;
    s->stress.assign(total, {});
    s->strain.assign(total, {});
    s->disp.assign(total, {});

    const bool wantScalar = (component >= 0 && component <= EpsEQV);
    std::vector<float> rawScalar;
    if (wantScalar) rawScalar.assign(total, 0.0f);

    const int sIdx[6] = { SX, SY, SZ, SXY, SYZ, SXZ };
    const int eIdx[6] = { EpsX, EpsY, EpsZ, EpsXY, EpsYZ, EpsXZ };

    float rawMin =  std::numeric_limits<float>::max();
    float rawMax = -std::numeric_limits<float>::max();

    for (int i = 0; i < s->N; ++i) {           // y
        for (int j = 0; j < s->N; ++j) {       // z
            for (int k = 0; k < s->N; ++k) {   // x
                const size_t idx = static_cast<size_t>(s->denseIndex(k, i, j));
                if (s->grainId[idx] == 0) continue;

                double accS[6] = {0}, accE[6] = {0}, accU[3] = {0}, accScalar = 0.0;

                for (int l = 0; l < 8; ++l) {
                    n3d::node3d key;
                    key.data[0] = kNodeOffsets[l][0] + k;
                    key.data[1] = kNodeOffsets[l][1] + i;
                    key.data[2] = kNodeOffsets[l][2] + j;

                    for (int t = 0; t < 6; ++t) {
                        accS[t] += wr.getValByCoord(key, sIdx[t]);
                        accE[t] += wr.getValByCoord(key, eIdx[t]);
                    }
                    accU[0] += wr.getValByCoord(key, UX);
                    accU[1] += wr.getValByCoord(key, UY);
                    accU[2] += wr.getValByCoord(key, UZ);
                    if (wantScalar) accScalar += wr.getValByCoord(key, component);
                }

                for (int t = 0; t < 6; ++t) s->stress[idx][t] = static_cast<float>(accS[t] / 8.0);
                for (int t = 0; t < 3; ++t) s->strain[idx][t] = static_cast<float>(accE[t] / 8.0);
                // ANSYS's nodal EPTO output is engineering shear, same as the
                // FFT path.  See kEngineeringToTensorShear.
                for (int t = 3; t < 6; ++t)
                    s->strain[idx][t] = static_cast<float>(accE[t] / 8.0) * kEngineeringToTensorShear;
                for (int t = 0; t < 3; ++t) s->disp[idx][t] = static_cast<float>(accU[t] / 8.0);

                if (wantScalar) {
                    const float v = static_cast<float>(accScalar / 8.0);
                    rawScalar[idx] = v;
                    rawMin = std::min(rawMin, v);
                    rawMax = std::max(rawMax, v);
                }
            }
        }
    }

    if (wantScalar && rawMax >= rawMin) {
        s->scalarMin = rawMin;
        s->scalarMax = rawMax;
        const float span = (rawMax > rawMin) ? (rawMax - rawMin) : 0.0f;
        s->scalar.resize(total);
        for (size_t v = 0; v < total; ++v)
            s->scalar[v] = (span > 0.0f) ? std::clamp((rawScalar[v] - rawMin) / span, 0.0f, 1.0f)
                                         : 1.0f;
    }

    finalize(*s);
    qDebug() << "buildSnapshotFromAnsys: N =" << s->N
             << "lambdaRef(s/e) =" << s->lambdaRefStress << s->lambdaRefStrain
             << "in" << timer.elapsed() << "ms";
    return s;
}

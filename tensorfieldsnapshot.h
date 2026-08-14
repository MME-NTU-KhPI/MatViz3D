// ============================================================================
//  tensorfieldsnapshot.h  -  One immutable, source-agnostic view of a solved
//                            per-voxel tensor field.
//
//  Both solvers produce per-voxel stress and strain, but in different shapes:
//  the FFT solver hands back dense arrays indexed (z*N+y)*N+x, while ANSYS
//  hands back a nodal result table you query by coordinate.  Rather than make
//  every geometry builder branch on which solver ran, both are normalized once
//  into this struct.  buildSnapshotFromAnsys() averages the 8 corner nodes of
//  each element, so downstream code only ever sees per-voxel data.
//
//  Two conventions are fixed here and must not be re-litigated downstream:
//
//    * Component order is the PIPELINE order (xx, yy, zz, xy, yz, xz), the same
//      order SingleShotResult::macro_stress and FieldVisualizationData use.
//      It is NOT Voigt (which would be xx,yy,zz,yz,xz,xy).
//
//    * Shear strain is stored as TENSOR strain (eps_xy), not engineering strain
//      (gamma_xy = 2 eps_xy).  Both solvers *report* engineering shear, so the
//      builders divide by two on the way in.  Skipping that halves nothing
//      visible -- it rotates every strain eigenvector by a wrong amount, which
//      looks plausible and is entirely wrong.  See kEngineeringToTensorShear.
//
//  Snapshots are immutable once built and are handed around as
//  shared_ptr<const TensorFieldSnapshot>, so a QtConcurrent worker can hold one
//  safely while the main thread moves on.  Building one touches no Qt singleton
//  and no GL state.
// ============================================================================
#ifndef TENSORFIELDSNAPSHOT_H
#define TENSORFIELDSNAPSHOT_H

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "tensormath.hpp"

struct FieldVisualizationData;
class ansysWrapper;

/// Which of the two stored tensor fields a builder should read.
enum class TensorSource { Stress, Strain };

/// Engineering shear (gamma = 2 eps) -> tensor shear, applied on the way in.
inline constexpr float kEngineeringToTensorShear = 0.5f;

struct TensorFieldSnapshot
{
    int  N = 0;                       ///< grid edge length in voxels
    bool hasStress = false;
    bool hasStrain = false;
    bool hasDisp   = false;

    /// Grain id per voxel, 0 == void.  Subsumes an occupancy mask and is also
    /// what the exploded-view offset is looked up by.
    std::vector<int32_t> grainId;

    std::vector<std::array<float, 6>> stress;   ///< pipeline, Pa
    std::vector<std::array<float, 6>> strain;   ///< pipeline, TENSOR shear
    std::vector<std::array<float, 3>> disp;     ///< corner-averaged displacement

    /// The field component currently selected in the UI, already normalized to
    /// [0,1] over its own range -- i.e. exactly what calculateScene() feeds to
    /// scalarToColor() -- so a glyph coloured "by field component" matches the
    /// voxel underneath it band for band.  scalarMin/scalarMax carry the raw
    /// range that normalization came from, for the legend.
    std::vector<float> scalar;
    float scalarMin = 0.0f;
    float scalarMax = 0.0f;

    /// Robust reference magnitude for glyph scaling: the 99th percentile of
    /// max|eigenvalue| over occupied voxels.  A plain maximum would let one
    /// stress-concentration outlier at a grain corner shrink every other glyph
    /// in the model to a dot.
    float lambdaRefStress = 0.0f;
    float lambdaRefStrain = 0.0f;

    /// Matches FieldVisualizationData::denseIndex() and the voxels[k][i][j]
    /// loop order in OpenGLWidgetQML::calculateScene() (k=x, i=y, j=z).
    int denseIndex(int x, int y, int z) const { return (z * N + y) * N + x; }

    bool inBounds(int x, int y, int z) const
    {
        return x >= 0 && y >= 0 && z >= 0 && x < N && y < N && z < N;
    }

    bool occupied(int x, int y, int z) const
    {
        return inBounds(x, y, z) && grainId[static_cast<size_t>(denseIndex(x, y, z))] != 0;
    }

    bool has(TensorSource s) const
    {
        return (s == TensorSource::Stress) ? hasStress : hasStrain;
    }

    float lambdaRef(TensorSource s) const
    {
        return (s == TensorSource::Stress) ? lambdaRefStress : lambdaRefStrain;
    }

    /// Tensor at one voxel.  False if out of bounds, void, or that source is
    /// not populated.
    bool voxelTensor(int x, int y, int z, TensorSource s, mvt::Sym3& out) const;

    /// Trilinearly interpolated tensor at a continuous point in grid-index
    /// space (integer coordinates land on voxel centres).
    ///
    /// The six components are interpolated and the result is eigendecomposed by
    /// the caller -- never the other way round.  Interpolating eigenvectors is
    /// not a well-posed operation: they are defined only up to sign, and within
    /// a degenerate subspace only up to an arbitrary rotation.
    ///
    /// Void corners are filled from the nearest occupied corner rather than
    /// treated as a zero tensor, which would drag interpolated directions
    /// toward the boundary and bend streamlines that should simply stop.
    /// False if fewer than one corner is occupied.
    bool sampleTensor(const mvt::Vec3& p, TensorSource s, mvt::Sym3& out) const;
};

/// Dense FFT per-voxel arrays -> snapshot.  `voxels` supplies grain ids;
/// `component` is the tensor_components value currently being colour-mapped.
std::shared_ptr<const TensorFieldSnapshot>
buildSnapshotFromFFT(const FieldVisualizationData& field,
                     int32_t*** voxels,
                     int component);

/// ANSYS nodal results -> snapshot, averaging the 8 corner nodes of each
/// element with the same node offsets calculateScene() uses.  This is the
/// expensive builder (8 * N^3 hash lookups per component); callers should build
/// it lazily, only once a tensor overlay is actually switched on.
std::shared_ptr<const TensorFieldSnapshot>
buildSnapshotFromAnsys(ansysWrapper& wr,
                       int numCubes,
                       int32_t*** voxels,
                       int component);

#endif // TENSORFIELDSNAPSHOT_H

#ifndef GRAIN_ANALYZER_H
#define GRAIN_ANALYZER_H

#include <map>
#include <vector>
#include <cstdint>
#include <cmath>
#include <QString>

/**
 * @brief Pure computational class for grain microstructure analysis.
 *
 * No Qt UI dependencies. Can be used from any module:
 *   - Probability_Algorithm::CleanUp() → CSV export
 *   - Statistics (UI) → histogram display
 *   - Future Python bridge / HDF5 export
 */
class GrainAnalyzer
{
public:
    // ── 3D per-grain metrics ────────────────────────────────────────────
    struct GrainStats3D {
        int    volume       = 0;    // [voxels]
        double esr          = 0.0;  // Equivalent Sphere Radius = (3V/4π)^(1/3)
        double norm_volume  = 0.0;  // volume / max_volume  [0..1]
        double surface_area = 0.0;  // exposed faces [voxels^2]
        double moment_inertia = 0.0; // Mean principal moment of inertia = (I1+I2+I3)/3

        // 3D Inertia Tensor & Principal Components
        double Ixx = 0.0, Iyy = 0.0, Izz = 0.0;
        double Ixy = 0.0, Ixz = 0.0, Iyz = 0.0;
        double I1 = 0.0, I2 = 0.0, I3 = 0.0; // Principal moments (I1 >= I2 >= I3)
        double semi_a = 0.0, semi_b = 0.0, semi_c = 0.0; // Equivalent ellipsoid semi-axes (a >= b >= c)
        double aspect_ratio = 1.0; // a / c (Elongation index)
        double sphericity_inertia = 1.0; // c / a (Inertial sphericity)
        double fractional_anisotropy = 0.0; // FA [0..1]
    };

    // ── 2D per-layer grain metrics ──────────────────────────────────────
    struct GrainStats2D {
        int    label        = 0;
        int    size         = 0;    // [pixels]
        int    perimeter    = 0;    // boundary pixels
        double norm_area    = 0.0;  // size / layer_area  [0..1]
        double ecr          = 0.0;  // Equivalent Circle Radius = sqrt(normArea/π)
        double shape_factor = 0.0;  // 4π·size / perimeter²
    };

    /**
     * @brief Main entry point. Single pass over voxels computes all metrics.
     * @param voxels  3D array [x][y][z], grain_id > 0, 0 = empty
     * @param numCubes edge length of the cubic RVE
     * @return map: grain_id → GrainStats
     */
    static std::map<int32_t, GrainStats3D> analyze3D(
        int32_t*** voxels, int numCubes);

    /**
     * @brief 2D layer-by-layer analysis (connected components per z-slice).
     * Computes size, perimeter, normArea, ECR, shape factor for every
     * grain region in every layer. Pure computation — no Qt UI.
     * @return flat vector of all 2D grain regions across all layers
     */
    static std::vector<GrainStats2D> analyze2D(
        int32_t*** voxels, int numCubes);

    /**
     * @brief Write per-grain 3D stats to CSV.
     * Format: grain_id;volume;esr;norm_volume;surface_area;moment_inertia
     */
    static void writeToCSV3D(
        const std::map<int32_t, GrainStats3D>& stats,
        const QString& filePath);

    /**
     * @brief Write per-layer 2D stats to CSV.
     * Format: label;size;perimeter;norm_area;ecr;shape_factor
     */
    static void writeToCSV2D(
        const std::vector<GrainStats2D>& stats,
        const QString& filePath);
};

#endif // GRAIN_ANALYZER_H

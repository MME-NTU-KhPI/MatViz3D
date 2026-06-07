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
    struct GrainStats {
        int    volume       = 0;    // [voxels]
        double esr          = 0.0;  // Equivalent Sphere Radius = (3V/4π)^(1/3)
        double norm_volume  = 0.0;  // volume / max_volume  [0..1]
        double surface_area = 0.0;  // exposed faces [voxels^2]
        double moment_inertia = 0.0; // (2/5) * ESR^2
    };

    /**
     * @brief Main entry point. Single pass over voxels computes all metrics.
     * @param voxels  3D array [x][y][z], grain_id > 0, 0 = empty
     * @param numCubes edge length of the cubic RVE
     * @return map: grain_id → GrainStats
     */
    static std::map<int32_t, GrainStats> analyze(
        int32_t*** voxels, int numCubes);

    /**
     * @brief Write per-grain stats to CSV.
     * Format: grain_id;volume;esr;norm_volume;surface_area;moment_inertia
     */
    static void writeToCSV(
        const std::map<int32_t, GrainStats>& stats,
        const QString& filePath);
};

#endif // GRAIN_ANALYZER_H

#ifndef POLYCRYSTALL_H
#define POLYCRYSTALL_H

#include "parent_algorithm.h"
#include <QString>
#include <array>

/**
 * @brief Cellular automaton grain growth for polycrystalline microstructures
 *        supporting Moore (26-neighbor), von Neumann (6-neighbor), and Radial (18-neighbor)
 *        growth stencils.
 */
class Polycrystall : public Parent_Algorithm
{
public:
    enum class Neighborhood {
        Moore,      // 26 neighbors (face, edge, corner)
        Neumann,    // 6 neighbors (face)
        Radial      // 18 neighbors (face, edge)
    };

    Polycrystall();
    Polycrystall(short int numCubes, int numColors, Neighborhood neighborhood = Neighborhood::Moore);

    void Initialization(bool isWaveGeneration) override;
    void Next_Iteration() override;
    bool getDone() const override;

    void setNeighborhood(Neighborhood neighborhood);
    Neighborhood getNeighborhood() const;

    static Neighborhood neighborhoodFromString(const QString& name);
    static QString neighborhoodToString(Neighborhood neighborhood);

private:
    Neighborhood m_neighborhood = Neighborhood::Moore;

    template <size_t N>
    void stepNeighborhood(const std::array<std::array<int32_t, 3>, N>& offsets);
};

#endif // POLYCRYSTALL_H

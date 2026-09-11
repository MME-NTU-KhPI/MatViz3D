#include "polycrystall.h"
#include "algorithmplugin.h"
#include "parameters.h"
#include <cmath>
#include <QDebug>
#include <omp.h>
#include <array>

namespace {

const std::array<std::array<int32_t, 3>, 6> NEUMANN_OFFSETS = {{
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
}};

const std::array<std::array<int32_t, 3>, 18> RADIAL_OFFSETS = {{
    {-1, 0, 0}, {1, 0, 0}, {0, -1, 0},
    {0, 1, 0}, {0, 0, -1}, {0, 0, 1},
    {-1, -1, 0}, {-1, 1, 0}, {1, -1, 0},
    {1, 1, 0}, {-1, 0, -1}, {-1, 0, 1},
    {1, 0, -1}, {1, 0, 1}, {0, -1, -1},
    {0, -1, 1}, {0, 1, -1}, {0, 1, 1}
}};

const std::array<std::array<int32_t, 3>, 26> MOORE_OFFSETS = {{
    {-1, -1, -1}, {-1, -1, 0}, {-1, -1, 1},
    {-1, 0, -1},  {-1, 0, 0},  {-1, 0, 1},
    {-1, 1, -1},  {-1, 1, 0},  {-1, 1, 1},
    {0, -1, -1},  {0, -1, 0},  {0, -1, 1},
    {0, 0, -1},                {0, 0, 1},
    {0, 1, -1},   {0, 1, 0},   {0, 1, 1},
    {1, -1, -1},  {1, -1, 0},  {1, -1, 1},
    {1, 0, -1},   {1, 0, 0},   {1, 0, 1},
    {1, 1, -1},   {1, 1, 0},   {1, 1, 1}
}};

} // anonymous namespace

Polycrystall::Polycrystall()
{
}

Polycrystall::Polycrystall(short int numCubes, int numColors, Neighborhood neighborhood, bool isThinLayer, const QString& layerDirection)
    : m_neighborhood(neighborhood)
{
    this->numCubes = numCubes;
    this->numColors = numColors;
    this->flags.isThinLayer = isThinLayer;
    this->m_layerDirection = layerDirection;
}

void Polycrystall::setNeighborhood(Neighborhood neighborhood)
{
    m_neighborhood = neighborhood;
}

Polycrystall::Neighborhood Polycrystall::getNeighborhood() const
{
    return m_neighborhood;
}

Polycrystall::Neighborhood Polycrystall::neighborhoodFromString(const QString& name)
{
    QString s = name.trimmed();
    if (s.contains("neumann", Qt::CaseInsensitive) || s == "6")
        return Neighborhood::Neumann;
    if (s.contains("radial", Qt::CaseInsensitive) || s == "18")
        return Neighborhood::Radial;
    return Neighborhood::Moore;
}

QString Polycrystall::neighborhoodToString(Neighborhood neighborhood)
{
    switch (neighborhood) {
    case Neighborhood::Neumann:
        return QStringLiteral("von Neumann (6)");
    case Neighborhood::Radial:
        return QStringLiteral("Radial (18)");
    case Neighborhood::Moore:
    default:
        return QStringLiteral("Moore (26)");
    }
}

void Polycrystall::Initialization(bool isWaveGeneration)
{
    flags.isThinLayer = flags.isThinLayer || Parameters::instance()->getIsThinLayer();
    if (!Parameters::instance()->getLayerDirection().isEmpty()) {
        m_layerDirection = Parameters::instance()->getLayerDirection();
    }
    Parent_Algorithm::Initialization(isWaveGeneration);

    QString neighDesc;
    switch (m_neighborhood) {
    case Neighborhood::Neumann:
        neighDesc = "6-neighbor (von Neumann)";
        break;
    case Neighborhood::Radial:
        neighDesc = "18-neighbor (Radial)";
        break;
    case Neighborhood::Moore:
    default:
        neighDesc = "26-neighbor (Moore)";
        break;
    }

    QString flagsDesc;
    if (flags.isPeriodicStructure) flagsDesc += ", periodic";
    if (flags.isThinLayer) flagsDesc += QString(", thin layer (%1)").arg(m_layerDirection);

    qDebug().noquote()
        << QString("[Polycrystall] %1^3 grid (%2 voxels), %3 seeds, %4%5")
               .arg(numCubes)
               .arg(static_cast<uint64_t>(numCubes) * numCubes * numCubes)
               .arg(seedPoints.size())
               .arg(neighDesc)
               .arg(flagsDesc);
}

template <size_t N>
void Polycrystall::stepNeighborhood(const std::array<std::array<int32_t, 3>, N>& offsets)
{
    const unsigned int counter_max = pow(numCubes, 3);

    const size_t current_size = grains.size();
    std::vector<Coordinate> newGrains;
    newGrains.reserve(current_size * N);
    unsigned int local_counter = 0;

#pragma omp parallel reduction(+:local_counter)
    {
        std::vector<Coordinate> privateGrains;
        privateGrains.reserve(current_size * N / omp_get_max_threads());

#pragma omp for schedule(guided) nowait
        for (size_t i = 0; i < current_size; i++)
        {
            const Coordinate& temp = grains[i];
            const int32_t x = temp.x, y = temp.y, z = temp.z;
            const int32_t current_value = voxels[x][y][z];

#pragma omp simd
            for (const auto& offset : offsets)
            {
                int32_t newX = x + offset[0];
                int32_t newY = y + offset[1];
                int32_t newZ = z + offset[2];

                if (flags.isPeriodicStructure == 1)
                {
                    newX = (newX + numCubes) % numCubes;
                    newY = (newY + numCubes) % numCubes;
                    newZ = (newZ + numCubes) % numCubes;
                }

                if (newX >= 0 && newX < numCubes &&
                    newY >= 0 && newY < numCubes &&
                    newZ >= 0 && newZ < numCubes)
                {
                    if (__sync_bool_compare_and_swap(&voxels[newX][newY][newZ], 0, current_value))
                    {
                        privateGrains.push_back({newX, newY, newZ});
                        local_counter++;
                    }
                }
            }
        }
#pragma omp critical
        {
            newGrains.insert(newGrains.end(),
                             std::make_move_iterator(privateGrains.begin()),
                             std::make_move_iterator(privateGrains.end()));
        }
    }

    filled_voxels += local_counter;
    grains = std::move(newGrains);
    IterationNumber++;

    const double fraction = (counter_max > 0) ? (static_cast<double>(filled_voxels) / counter_max) : 1.0;
    const double pct = fraction * 100.0;

    if (getDone()) {
        qDebug().noquote()
            << QString("[Polycrystall] Step %1: filled %2/%3 (%4%) | growth complete")
                   .arg(IterationNumber, 2)
                   .arg(filled_voxels)
                   .arg(counter_max)
                   .arg(pct, 5, 'f', 1);
    } else {
        qDebug().noquote()
            << QString("[Polycrystall] Step %1: filled %2/%3 (%4%) | active front: %5 voxels")
                   .arg(IterationNumber, 2)
                   .arg(filled_voxels)
                   .arg(counter_max)
                   .arg(pct, 5, 'f', 1)
                   .arg(grains.size());
    }
}

void Polycrystall::Next_Iteration()
{
    if (getDone()) return;

    switch (m_neighborhood) {
    case Neighborhood::Neumann:
        stepNeighborhood(NEUMANN_OFFSETS);
        break;
    case Neighborhood::Radial:
        stepNeighborhood(RADIAL_OFFSETS);
        break;
    case Neighborhood::Moore:
    default:
        stepNeighborhood(MOORE_OFFSETS);
        break;
    }
}

bool Polycrystall::getDone() const
{
    return grains.empty() || Parent_Algorithm::getDone();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Plugin registration
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<ParamField> polycrystallSchema()
{
    ParamField dirField{ "layer_direction", "Layer direction", ParamField::Enum, "+Z", {}, {},
                         { "+Z", "-Z", "+X", "-X", "+Y", "-Y" }, "main" };
    dirField.visibleIf = "is_thin_layer";

    std::vector<ParamField> s = {
        { "size",        "Cube size",     ParamField::Int,        10, 1, 500, {}, "main" },
        { "points",      "Points",        ParamField::PointsMode, 10, 1, 100000,
         { "Size", "Concentration" }, "main" },
        { "is_periodic", "Periodic cell", ParamField::Bool,       false, {}, {}, {}, "main" },
        { "is_thin_layer", "Thin layer",  ParamField::Bool,       false, {}, {}, {}, "main" },
        dirField,
        { "polycrystall_neighborhood", "Neighborhood", ParamField::Enum, "Moore (26)", {}, {},
         { "Moore (26)", "von Neumann (6)", "Radial (18)" }, "main" },
    };

    s.push_back(materialParamField());

    const std::vector<ParamField> tex = textureParamFields();
    s.insert(s.end(), tex.begin(), tex.end());

    return s;
}

MATVIZ_REGISTER_ALGORITHM(AlgorithmPlugin{
    "Polycrystall",
    "Cellular automaton polycrystalline grain growth with selectable neighborhood "
    "(Moore 26-neighbor, von Neumann 6-neighbor, Radial 18-neighbor), "
    "with material and crystallographic texture selection.",
    /*order=*/ 2,
    polycrystallSchema(),
    [](const Parameters& p) {
        return std::make_shared<Polycrystall>(
            static_cast<short int>(p.getSize()),
            p.getPoints(),
            Polycrystall::neighborhoodFromString(p.getPolycrystallNeighborhood()),
            p.getIsThinLayer(),
            p.getLayerDirection()
        );
    }
});

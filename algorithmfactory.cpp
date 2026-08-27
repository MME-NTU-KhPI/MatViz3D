#include "algorithmfactory.h"
#include "parameters.h"
#include "probability_algorithm.h"
#include "probability_circle.h"
#include "probability_ellipse.h"
#include "dlca.h"

// Legacy half-registration for the algorithms that have not been converted to
// self-registering AlgorithmPlugins yet. Plugin-based algorithms (voronoi.cpp,
// composite.cpp, moore.cpp, neumann.cpp, radial.cpp) are already in the factory
// before main() runs and are not listed here.
void registerAlgorithms() {
    auto& factory = AlgorithmFactory::instance();
    Parameters& params = *Parameters::instance();

    factory.registerAlgorithm("DLCA", [&params](const Parameters&) {
        return std::make_shared<DLCA>(params.getSize(), params.getPoints());
    });

    factory.registerAlgorithm("Probability Algorithm", [&params](const Parameters&) {
        return std::make_shared<Probability_Algorithm>(params.getSize(), params.getPoints());
    });

    factory.registerAlgorithm("Probability Circle", [&params](const Parameters&) {
        return std::make_shared<Probability_Circle>(params.getSize(), params.getPoints());
    });

    factory.registerAlgorithm("Probability Ellipse", [&params](const Parameters&) {
        return std::make_shared<Probability_Ellipse>(params.getSize(), params.getPoints());
    });
}

void registerSchemas()
{
    auto& factory = AlgorithmFactory::instance();

    // Algorithms that have been migrated to AlgorithmPlugin carry their own
    // schema and are absent from this function entirely -- see voronoi.cpp,
    // composite.cpp, moore.cpp, neumann.cpp, radial.cpp.
    std::vector<ParamField> base = baseAlgorithmSchema();

    factory.registerSchema("Probability Circle", base);
    factory.registerSchema("Probability Ellipse", base);

    // DLCA — base + material
    auto dlca = base;
    dlca.push_back(
        { "material", "Material", ParamField::Enum, "bcc", {}, {}, { "fcc", "bcc" }, "main" }
        );
    factory.registerSchema("DLCA", dlca);

    // Probability Algorithm — base + advanced
    auto probAlg = base;                        // Cube size, Points, Wave* → main → block Data
    probAlg.insert(probAlg.end(), {
                                   { "stefan_number",        "Stefan number",  ParamField::Double, 100.0, 1.0, 1000.0, {}, "main" },
                                   { "initial_nuclei_count", "Initial nuclei", ParamField::Int,    1,     1,   100,    {}, "main" },

                                   { "halfaxis_a",          "Half-axis a",           ParamField::Double, 1.5, 0.1, 100.0, {}, "main" },
                                   { "halfaxis_b",          "Half-axis b",           ParamField::Double, 1.5, 0.1, 100.0, {}, "main" },
                                   { "halfaxis_c",          "Half-axis c",           ParamField::Double, 1.5, 0.1, 100.0, {}, "main" },
                                   { "orientation_angle_a", "Orientation angle (A)", ParamField::Double, 0.0, 0.0, 360.0, {}, "main" },
                                   { "orientation_angle_b", "Orientation angle (B)", ParamField::Double, 0.0, 0.0, 360.0, {}, "main" },
                                   { "orientation_angle_c", "Orientation angle (C)", ParamField::Double, 0.0, 0.0, 360.0, {}, "main" },
                                   });
    factory.registerSchema("Probability Algorithm", probAlg);
}

#include "algorithmfactory.h"
#include "parameters.h"
#include "neumann.h"
#include "moore.h"
#include "radial.h"
#include "composite.h"
#include "probability_algorithm.h"
#include "probability_circle.h"
#include "probability_ellipse.h"
#include "dlca.h"

void registerAlgorithms() {
    auto& factory = AlgorithmFactory::instance();
    Parameters& params = *Parameters::instance();

    factory.registerAlgorithm("Neumann", [&params](const Parameters&) {
        return std::make_shared<Neumann>(params.getSize(), params.getPoints());
    });

    factory.registerAlgorithm("Moore", [&params](const Parameters&) {
        return std::make_shared<Moore>(params.getSize(), params.getPoints());
    });

    factory.registerAlgorithm("Radial", [&params](const Parameters&) {
        return std::make_shared<Radial>(params.getSize(), params.getPoints());
    });

    factory.registerAlgorithm("Composite", [&params](const Parameters&) {
        return std::make_shared<Composite>(params.getSize(), params.getPoints());
    });

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

    std::vector<ParamField> base = {
                                    { "size", "Cube size", ParamField::Int, 10, 1, 500, {}, "main" },

                                    { "points", "Points", ParamField::PointsMode, 10, 1, 100000,
                                     { "Size", "Concentration" }, "main" },

                                    { "wave_coefficient", "Wave coefficient", ParamField::Double, 0.0, 0.0, 10.0, {}, "main" },
                                    { "wave_spread",      "Wave spread",      ParamField::Double, 0.0, 0.0, 10.0, {}, "main" },
                                    };

    // Neumann / Moore / Radial / Composite
    factory.registerSchema("Neumann", base);
    factory.registerSchema("Moore",   base);
    factory.registerSchema("Radial",  base);
    factory.registerSchema("Composite", base);
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

#include "algorithmfactory.h"
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

    factory.registerAlgorithm("Neumann", [](const Parameters&) {
        return std::make_shared<Neumann>(Parameters::size, Parameters::points);
    });

    factory.registerAlgorithm("Moore", [](const Parameters&) {
        return std::make_shared<Moore>(Parameters::size, Parameters::points);
    });

    factory.registerAlgorithm("Radial", [](const Parameters&) {
        return std::make_shared<Radial>(Parameters::size, Parameters::points);
    });

    factory.registerAlgorithm("Composite", [](const Parameters&) {
        return std::make_shared<Composite>(Parameters::size, Parameters::points);
    });

    factory.registerAlgorithm("DLCA", [](const Parameters& params) {
        return std::make_shared<DLCA>(params.size, params.points);
    });

    factory.registerAlgorithm("Probability Algorithm", [](const Parameters& params) {
        return std::make_shared<Probability_Algorithm>(params.size, params.points);
    });

    factory.registerAlgorithm("Probability Circle", [](const Parameters& params) {
        return std::make_shared<Probability_Circle>(params.size, params.points);
    });

    factory.registerAlgorithm("Probability Ellipse", [](const Parameters& params) {
        return std::make_shared<Probability_Ellipse>(params.size, params.points);
    });
}


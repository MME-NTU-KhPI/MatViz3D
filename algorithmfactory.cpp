#include "algorithmfactory.h"
#include "neumann.h"
#include "moore.h"
#include "radial.h"
#include "composite.h"
#include "probability_algorithm.h"
#include "dlca.h"

void registerAlgorithms() {
    auto& factory = AlgorithmFactory::instance();

    factory.registerAlgorithm("Neumann", [](const Parameters&) {
        return std::make_unique<Neumann>(Parameters::size, Parameters::points);
    });

    factory.registerAlgorithm("Moore", [](const Parameters&) {
        return std::make_unique<Moore>(Parameters::size, Parameters::points);
    });

    factory.registerAlgorithm("Radial", [](const Parameters&) {
        return std::make_unique<Radial>(Parameters::size, Parameters::points);
    });

    factory.registerAlgorithm("Composite", [](const Parameters&) {
        return std::make_unique<Composite>(Parameters::size, Parameters::points);
    });

    factory.registerAlgorithm("DLCA", [](const Parameters& params) {
        return std::make_unique<DLCA>(params.size, params.points);
    });

    factory.registerAlgorithm("Probability Algorithm", [](const Parameters& params) {
        return std::make_unique<Probability_Algorithm>(params.size, params.points);
    });
}


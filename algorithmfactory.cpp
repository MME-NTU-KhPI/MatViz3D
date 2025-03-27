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


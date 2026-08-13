#ifndef ALGORITHMPLUGIN_H
#define ALGORITHMPLUGIN_H

#include <QString>
#include <functional>
#include <memory>
#include <vector>
#include "paramfield.h"

class Parent_Algorithm;
class Parameters;

/**
 * @brief Everything the application needs to know about one structure
 *        generation algorithm, in a single value.
 *
 * The name, the parameter schema the UI/CLI builds its panel from, and the
 * factory that produces the object all travel together, so an algorithm can be
 * described entirely inside its own translation unit -- nothing else in the
 * codebase has to name it.
 */
struct AlgorithmPlugin {
    /// Shown in the algorithm combo box and accepted by --algorithm.
    QString name;

    /// One-line summary (tooltip / --help text). Optional.
    QString description;

    /// Sort key for the UI list; equal orders keep registration order.
    int order = 100;

    /// Parameter fields rendered by DynamicParamBlock.qml.
    std::vector<ParamField> schema;

    /// Produces a ready-to-run instance. Called on every run, never cached.
    std::function<std::shared_ptr<Parent_Algorithm>(const Parameters&)> create;
};

/**
 * @brief Self-registration hook: constructing one at namespace scope adds the
 *        plugin to AlgorithmFactory before main() runs.
 *
 * AlgorithmFactory::instance() is a function-local static, so it is guaranteed
 * to be alive by the time any registrar's constructor reaches it regardless of
 * translation unit initialisation order.
 */
class AlgorithmRegistrar
{
public:
    explicit AlgorithmRegistrar(AlgorithmPlugin plugin);
};

/**
 * @brief The fields every grain-growth algorithm needs (cube size, seed count,
 *        nucleation wave). Start a plugin schema from this and append.
 */
std::vector<ParamField> baseAlgorithmSchema();

/**
 * @brief Material picked from material_properties.db.
 *
 * Options are resolved at panel-build time via the "materials" provider; the
 * chosen row supplies the cubic constants both stress solvers use and the
 * lattice the texture presets are built for.
 */
ParamField materialParamField();

/**
 * @brief Texture preset dropdown + scatter angle + a button that opens the
 *        interactive texture editor.
 *
 * Both the dropdown's "Custom (editor)" entry and the button raise the
 * "open_texture_editor" action; everything else writes straight into
 * Parameters::textureComponents, which is what both solvers read.
 */
std::vector<ParamField> textureParamFields();

#define MATVIZ_PLUGIN_JOIN2(a, b) a##b
#define MATVIZ_PLUGIN_JOIN(a, b) MATVIZ_PLUGIN_JOIN2(a, b)

/**
 * Declares the plugin for one algorithm. This is the complete wiring -- there
 * is no central list to edit:
 *
 *   MATVIZ_REGISTER_ALGORITHM(AlgorithmPlugin{
 *       "My Algorithm", "what it does", 10, mySchema(),
 *       [](const Parameters& p) { return std::make_shared<My>(p.getSize(), p.getPoints()); }
 *   });
 */
#define MATVIZ_REGISTER_ALGORITHM(...) \
    static const AlgorithmRegistrar MATVIZ_PLUGIN_JOIN(matviz_algorithm_registrar_, __LINE__){ __VA_ARGS__ }

#endif // ALGORITHMPLUGIN_H

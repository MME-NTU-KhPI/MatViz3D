#include "algorithmplugin.h"
#include "algorithmfactory.h"

AlgorithmRegistrar::AlgorithmRegistrar(AlgorithmPlugin plugin)
{
    // Safe during static initialisation: AlgorithmFactory::instance() holds a
    // function-local static, and the plugin carries only QString/QVariant/
    // std::function members, none of which need QCoreApplication to exist.
    AlgorithmFactory::instance().registerPlugin(std::move(plugin));
}

std::vector<ParamField> baseAlgorithmSchema()
{
    return {
        { "size", "Cube size", ParamField::Int, 10, 1, 500, {}, "main" },

        { "points", "Points", ParamField::PointsMode, 10, 1, 100000,
         { "Size", "Concentration" }, "main" },

        { "wave_coefficient", "Wave coefficient", ParamField::Double, 0.0, 0.0, 10.0, {}, "main" },
        { "wave_spread",      "Wave spread",      ParamField::Double, 0.0, 0.0, 10.0, {}, "main" },
    };
}

ParamField materialParamField()
{
    ParamField f{ "db_material", "Material", ParamField::Enum, "Cu", {}, {}, {}, "main" };
    f.optionsProvider = "materials";
    return f;
}

std::vector<ParamField> textureParamFields()
{
    ParamField preset{ "texture_preset", "Texture", ParamField::Enum,
                       "Random", {}, {},
                       { "Random", "Extrusion", "Rolling", "Recrystallization",
                         "Shear", "Scattered cube", "Custom (editor)" },
                       "main" };
    // Picking the custom entry opens the editor, so "Custom" actually takes the
    // user somewhere instead of silently doing nothing until they find the menu.
    preset.action        = "open_texture_editor";
    preset.actionOnValue = "Custom (editor)";

    return {
        preset,
        { "texture_scatter", "Texture scatter (deg)", ParamField::Double, 11.0, 0.0, 90.0, {}, "main" },
    };
}

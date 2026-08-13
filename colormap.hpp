// ============================================================================
//  colormap.hpp  -  The colour palettes shared by every view in MatViz3D.
//
//  Extracted from openglwidgetqml.cpp so the elastic-surface view in the
//  material database window colours its geometry from the same tables as the
//  main field view, instead of growing a second set that drifts.
//  OpenGLWidgetQML delegates to these and keeps its own enum as an alias, so
//  the QML-facing colorMapPalette property and the legend are unchanged.
//
//  Palette ordinals are part of the QML API (MainWindow.qml's colormap menu
//  sets glWidget.colorMapPalette = 0..4) -- do not reorder them.
// ============================================================================
#pragma once

#include <QOpenGLFunctions>

#include <algorithm>
#include <array>
#include <vector>

namespace matviz_cmap {

enum class Palette {
    Rainbow   = 0,   ///< sequential; the original hardcoded 9-band map
    CoolWarm  = 1,   ///< diverging, white at mid-range
    RdBu      = 2,   ///< diverging, reversed (red = high)
    Viridis   = 3,   ///< perceptually uniform sequential
    Grayscale = 4    ///< black (low) -> white (high)
};
inline constexpr int kPaletteCount = 5;

struct ColorStop { float t; GLubyte r, g, b; };

// Sequential, matches the original hardcoded 9-band map (blue -> cyan ->
// green -> yellow -> red) -- kept as the default so existing plots don't
// change look.
inline const std::vector<ColorStop>& rainbowStops()
{
    static const std::vector<ColorStop> stops = {
        {0.000f, 0,   0,   255},
        {0.125f, 0,   178, 255},
        {0.250f, 0,   255, 255},
        {0.375f, 0,   255, 178},
        {0.500f, 0,   255, 0  },
        {0.625f, 178, 255, 0  },
        {0.750f, 255, 255, 0  },
        {0.875f, 255, 178, 0  },
        {1.000f, 255, 0,   0  },
    };
    return stops;
}

// Diverging, white at the middle of the current min/max range.
inline const std::vector<ColorStop>& coolWarmStops()
{
    static const std::vector<ColorStop> stops = {
        {0.0f, 0,   0,   255},
        {0.5f, 255, 255, 255},
        {1.0f, 255, 0,   0  },
    };
    return stops;
}

// Diverging, reversed from CoolWarm -- red = high (tension), blue = low
// (compression), a common solid-mechanics convention.
inline const std::vector<ColorStop>& rdBuStops()
{
    static const std::vector<ColorStop> stops = {
        {0.0f, 255, 0,   0  },
        {0.5f, 255, 255, 255},
        {1.0f, 0,   0,   255},
    };
    return stops;
}

// Perceptually uniform sequential map (approximates matplotlib's viridis).
inline const std::vector<ColorStop>& viridisStops()
{
    static const std::vector<ColorStop> stops = {
        {0.00f, 68,  1,   84 },
        {0.25f, 59,  82,  139},
        {0.50f, 33,  145, 140},
        {0.75f, 94,  201, 98 },
        {1.00f, 253, 231, 37 },
    };
    return stops;
}

// Sequential, black (low) -> white (high).
inline const std::vector<ColorStop>& grayscaleStops()
{
    static const std::vector<ColorStop> stops = {
        {0.0f, 0,   0,   0  },
        {1.0f, 255, 255, 255},
    };
    return stops;
}

inline const std::vector<ColorStop>& stopsForPalette(Palette palette)
{
    switch (palette) {
        case Palette::CoolWarm:  return coolWarmStops();
        case Palette::RdBu:      return rdBuStops();
        case Palette::Viridis:   return viridisStops();
        case Palette::Grayscale: return grayscaleStops();
        case Palette::Rainbow:
        default:                 return rainbowStops();
    }
}

/// Piecewise-linear interpolation between a palette's colour stops at t in [0,1].
inline std::array<GLubyte, 4> interpolateStops(const std::vector<ColorStop>& stops, float t)
{
    t = std::min(1.0f, std::max(0.0f, t));
    for (size_t i = 1; i < stops.size(); ++i) {
        if (t <= stops[i].t || i == stops.size() - 1) {
            const ColorStop& a = stops[i - 1];
            const ColorStop& b = stops[i];
            const float span = b.t - a.t;
            const float f = span > 0.0f ? (t - a.t) / span : 0.0f;
            return {
                GLubyte(a.r + f * (int(b.r) - int(a.r))),
                GLubyte(a.g + f * (int(b.g) - int(a.g))),
                GLubyte(a.b + f * (int(b.b) - int(a.b))),
                255
            };
        }
    }
    return { stops.back().r, stops.back().g, stops.back().b, 255 };
}

/// Discretize a palette into numLevels bands.
inline std::vector<std::array<GLubyte, 4>> createColorMap(int numLevels, Palette palette)
{
    numLevels = std::max(2, numLevels);
    const std::vector<ColorStop>& stops = stopsForPalette(palette);

    std::vector<std::array<GLubyte, 4>> colorMap(numLevels);
    for (int i = 0; i < numLevels; ++i) {
        const float t = float(i) / float(numLevels - 1);
        colorMap[i] = interpolateStops(stops, t);
    }
    return colorMap;
}

/// Look up a normalized value in a discretized map.
///
/// NOTE: the original in openglwidgetqml.cpp did not clamp, so a value outside
/// [0,1] indexed out of bounds -- every caller happened to clamp first. The
/// clamp here can only change behaviour in cases that were undefined before.
inline std::array<GLubyte, 4> scalarToColor(float value,
                                            const std::vector<std::array<GLubyte, 4>>& colorMap)
{
    if (colorMap.empty()) return { 255, 255, 255, 255 };
    const int last = int(colorMap.size()) - 1;
    int index = static_cast<int>(value * float(last));
    index = std::min(last, std::max(0, index));
    return colorMap[size_t(index)];
}

} // namespace matviz_cmap

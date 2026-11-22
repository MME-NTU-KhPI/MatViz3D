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
//  sets glWidget.colorMapPalette = 0..6, AnisotropySurfacePanel.qml lists
//  them by index) -- do not reorder them; append.
//
//  Turbo, Viridis, Cividis and the cool-warm pair are sampled at 17 stops
//  from matplotlib's tables (matplotlib 3.x: turbo, viridis, cividis,
//  coolwarm = Moreland 2009); piecewise-linear between 17 stops is within a
//  couple of 8-bit levels of the full tables.
// ============================================================================
#pragma once

#include <QOpenGLFunctions>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace matviz_cmap {

enum class Palette {
    Rainbow   = 0,   ///< sequential; the original hardcoded 9-band map
    CoolWarm  = 1,   ///< diverging, white at mid-range
    RdBu      = 2,   ///< diverging, reversed (red = high)
    Viridis   = 3,   ///< perceptually uniform sequential
    Grayscale = 4,   ///< black (low) -> white (high)
    Turbo     = 5,   ///< rainbow-like but perceptually smoother (Google, 2019)
    Cividis   = 6    ///< uniform sequential, readable with colour-vision deficiency
};
inline constexpr int kPaletteCount = 7;

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

// Diverging (Moreland's cool-warm), light grey at the middle of the current
// min/max range. Before 2026-11-22 this was pure blue -> white -> pure red:
// the white middle vanished on a white page (OKLab dE 0.02 to the paper)
// and the pure ends were the darkest colours of the map.
inline const std::vector<ColorStop>& coolWarmStops()
{
    static const std::vector<ColorStop> stops = {
        {0.0000f,  59,  76, 192},
        {0.0625f,  78, 104, 216},
        {0.1250f,  98, 130, 234},
        {0.1875f, 119, 154, 247},
        {0.2500f, 141, 176, 254},
        {0.3125f, 163, 194, 254},
        {0.3750f, 185, 208, 249},
        {0.4375f, 204, 217, 237},
        {0.5000f, 221, 220, 220},
        {0.5625f, 236, 211, 197},
        {0.6250f, 245, 196, 172},
        {0.6875f, 247, 176, 147},
        {0.7500f, 244, 152, 122},
        {0.8125f, 235, 125,  98},
        {0.8750f, 221,  95,  75},
        {0.9375f, 202,  59,  55},
        {1.0000f, 180,   4,  38},
    };
    return stops;
}

// Diverging, CoolWarm mirrored: red at the low end, blue at the high end, as
// the stops have always been (the comment here used to say "red = high",
// which the stops never did).
inline const std::vector<ColorStop>& rdBuStops()
{
    static const std::vector<ColorStop> stops = {
        {0.0000f, 180,   4,  38},
        {0.0625f, 202,  59,  55},
        {0.1250f, 221,  95,  75},
        {0.1875f, 235, 125,  98},
        {0.2500f, 244, 152, 122},
        {0.3125f, 247, 176, 147},
        {0.3750f, 245, 196, 172},
        {0.4375f, 236, 211, 197},
        {0.5000f, 221, 220, 220},
        {0.5625f, 204, 217, 237},
        {0.6250f, 185, 208, 249},
        {0.6875f, 163, 194, 254},
        {0.7500f, 141, 176, 254},
        {0.8125f, 119, 154, 247},
        {0.8750f,  98, 130, 234},
        {0.9375f,  78, 104, 216},
        {1.0000f,  59,  76, 192},
    };
    return stops;
}

// Perceptually uniform sequential map (matplotlib's viridis).
inline const std::vector<ColorStop>& viridisStops()
{
    static const std::vector<ColorStop> stops = {
        {0.0000f,  68,   1,  84},
        {0.0625f,  72,  24, 106},
        {0.1250f,  71,  45, 123},
        {0.1875f,  66,  64, 134},
        {0.2500f,  59,  82, 139},
        {0.3125f,  51,  99, 141},
        {0.3750f,  44, 114, 142},
        {0.4375f,  38, 130, 142},
        {0.5000f,  33, 145, 140},
        {0.5625f,  31, 160, 136},
        {0.6250f,  40, 174, 128},
        {0.6875f,  63, 188, 115},
        {0.7500f,  94, 201,  98},
        {0.8125f, 132, 212,  75},
        {0.8750f, 173, 220,  48},
        {0.9375f, 216, 226,  25},
        {1.0000f, 253, 231,  37},
    };
    return stops;
}

// Turbo: a rainbow with even lightness steps and no false bands.
inline const std::vector<ColorStop>& turboStops()
{
    static const std::vector<ColorStop> stops = {
        {0.0000f,  48,  18,  59},
        {0.0625f,  64,  64, 162},
        {0.1250f,  70, 107, 227},
        {0.1875f,  66, 148, 255},
        {0.2500f,  40, 188, 235},
        {0.3125f,  24, 221, 194},
        {0.3750f,  50, 242, 152},
        {0.4375f, 109, 254,  98},
        {0.5000f, 164, 252,  60},
        {0.5625f, 205, 236,  52},
        {0.6250f, 238, 207,  58},
        {0.6875f, 253, 172,  52},
        {0.7500f, 251, 126,  33},
        {0.8125f, 235,  80,  14},
        {0.8750f, 208,  47,   5},
        {0.9375f, 169,  22,   1},
        {1.0000f, 122,   4,   3},
    };
    return stops;
}

// Cividis: uniform in lightness, the same for normal and deuteranomalous vision.
inline const std::vector<ColorStop>& cividisStops()
{
    static const std::vector<ColorStop> stops = {
        {0.0000f,   0,  34,  78},
        {0.0625f,   0,  46, 106},
        {0.1250f,  26,  56, 111},
        {0.1875f,  50,  67, 109},
        {0.2500f,  67,  78, 108},
        {0.3125f,  83,  90, 109},
        {0.3750f,  97, 101, 111},
        {0.4375f, 111, 112, 115},
        {0.5000f, 125, 124, 120},
        {0.5625f, 140, 136, 120},
        {0.6250f, 155, 148, 118},
        {0.6875f, 171, 160, 114},
        {0.7500f, 188, 174, 108},
        {0.8125f, 205, 187,  99},
        {0.8750f, 222, 201,  88},
        {0.9375f, 240, 216,  70},
        {1.0000f, 254, 232,  56},
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
        case Palette::Turbo:     return turboStops();
        case Palette::Cividis:   return cividisStops();
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

// ---------------------------------------------------------------------------
//  Grain colours
// ---------------------------------------------------------------------------
namespace detail {
inline float srgbEncode(float c)
{
    c = std::min(1.0f, std::max(0.0f, c));
    return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}
/// OKLab (Ottosson 2020) -> linear sRGB; components may fall outside [0,1].
inline void oklabToLinear(float L, float a, float b, float rgb[3])
{
    const float l_ = L + 0.3963377774f * a + 0.2158037573f * b;
    const float m_ = L - 0.1055613458f * a - 0.0638541728f * b;
    const float s_ = L - 0.0894841775f * a - 1.2914855480f * b;
    const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    rgb[0] =  4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    rgb[1] = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    rgb[2] = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
}
} // namespace detail

/// Colour of grain `index` (0-based), the same for any number of grains.
///
/// OKLCH with the hue stepped by the golden ratio (so grains with
/// neighbouring ids get far-apart hues), chroma 0.19 reduced only as far as
/// the hue's sRGB gamut needs, and a lightness that follows the hue the way
/// natural colours do -- 0.71 + 0.11 cos(h - 110 deg), yellow light, blue
/// darker -- alternated by +-0.045 between neighbouring ids. (A lightness
/// cycle independent of hue made dark yellows read as olive and brown.) Replaces evenly spaced HSV hues at full saturation and value,
/// whose neighbours became indistinguishable as the grain count grew (OKLab
/// dE between ids i and i+1: 0.008 for 30 grains, 0 for 200; now >= 0.24),
/// and whose blues and reds were the darkest colours on the dark backdrop.
inline std::array<GLubyte, 4> grainColor(int index)
{
    const int    i      = std::max(0, index);
    const double golden = 0.6180339887498949;
    const double turns  = std::fmod(double(i) * golden, 1.0);
    const float  h = float(turns * 6.283185307179586);
    const float  yellow = 110.0f * 3.14159265f / 180.0f;   // OKLCH hue of yellow
    const float  L = 0.71f + 0.11f * std::cos(h - yellow) + ((i % 2) ? 0.045f : -0.045f);

    float rgb[3] = {0, 0, 0};
    for (float C = 0.19f; C >= 0.0f; C -= 0.005f) {
        detail::oklabToLinear(L, C * std::cos(h), C * std::sin(h), rgb);
        if (rgb[0] >= -1e-4f && rgb[1] >= -1e-4f && rgb[2] >= -1e-4f &&
            rgb[0] <= 1.0001f && rgb[1] <= 1.0001f && rgb[2] <= 1.0001f)
            break;
    }
    return { GLubyte(std::lround(detail::srgbEncode(rgb[0]) * 255.0f)),
             GLubyte(std::lround(detail::srgbEncode(rgb[1]) * 255.0f)),
             GLubyte(std::lround(detail::srgbEncode(rgb[2]) * 255.0f)),
             255 };
}

} // namespace matviz_cmap

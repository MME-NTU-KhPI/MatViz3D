#ifndef LATTICEFAMILY_H
#define LATTICEFAMILY_H

#include <QString>
#include <QStringList>
#include <vector>

/**
 * @brief The unit-cell families the Lattice algorithm can build.
 *
 * Kept in a header of its own, with nothing from Qt beyond QString, so that
 * Parameters (combo-box label <-> CLI key), the command-line parser
 * (validation) and the algorithm itself all read the one table below and
 * cannot drift apart.
 */
enum class LatticeFamily {
    GyroidSheet, GyroidSkeletal,
    SchwarzPSheet, SchwarzPSkeletal,
    SchwarzDSheet, SchwarzDSkeletal,
    IWPSheet, IWPSkeletal,
    SimpleCubic, BCC, FCC, Octet,
    Honeycomb, Reentrant,
    Spinodoid,
    FourierSheet, FourierSkeletal,
    NoiseSheet, NoiseSkeletal,
    // Plate lattices: thin plates on the {100} (SC), {110} (BCC) and {111}
    // (FCC) plane families of the cubic cell, alone or united; optional
    // circular holes at the panel centres (`plate_hole`).
    PlateSC, PlateBCC, PlateFCC, PlateSCBCC, PlateSCFCC, PlateBCCFCC, PlateSCBCCFCC,
    // Site primitives: spheres at the SC / BCC / FCC lattice sites and
    // cylinders along the axes through the face centres. Alone they are
    // sphere packings and a three-way channel cross; their main use is as
    // the second body of `lattice_union` / `lattice_subtract`.
    SpheresSC, SpheresBCC, SpheresFCC, CylindersAxes,
    // Cubic assembly: random struts, triangular plates, spherical shells and
    // carved spherical windows drawn (seeded) in the asymmetric unit of the
    // cubic cell and repeated by all its 48 symmetries -- the free-form,
    // topology-optimization-like cells with full cubic symmetry.
    CubicAssembly,
    // 3D honeycombs (closed-cell foams): the walls of the Voronoi partition
    // of a site set -- BCC sites give Kelvin cells (truncated octahedra),
    // FCC sites rhombic dodecahedra, A15 sites the Weaire-Phelan foam, and
    // seeded random sites a random foam. `foam_mode` keeps the walls, walls
    // with a window in every face, or the edges only (an open-cell foam).
    FoamKelvin, FoamRhombic, FoamWeairePhelan, FoamRandom,
    // A grown fungal mycelium (mycelium.h): hyphae forage on a substrate,
    // branch, fuse and taper, seeded; `mycelium_mode` substrate, polypore
    // (skeletal + binding hyphae), mat (layers + anchors) or layered (graded).
    Mycelium
};

struct LatticeFamilyInfo {
    LatticeFamily family;
    const char*   key;    ///< CLI / config / HDF5 spelling
    const char*   label;  ///< combo-box text
    /// True when the solid is the sublevel set of a *signed* level-set
    /// function (skeletal TPMS, spinodoid) rather than of a distance-like
    /// field (sheet TPMS, struts, walls). Thickness grading and jitter are
    /// applied additively to the former and multiplicatively to the latter.
    bool signedLevelSet;
};

inline const std::vector<LatticeFamilyInfo>& latticeFamilies()
{
    static const std::vector<LatticeFamilyInfo> t = {
        { LatticeFamily::GyroidSheet,      "gyroid_sheet",       "Gyroid (sheet)",                 false },
        { LatticeFamily::GyroidSkeletal,   "gyroid_skeletal",    "Gyroid (skeletal)",              true  },
        { LatticeFamily::SchwarzPSheet,    "schwarz_p_sheet",    "Schwarz P (sheet)",              false },
        { LatticeFamily::SchwarzPSkeletal, "schwarz_p_skeletal", "Schwarz P (skeletal)",           true  },
        { LatticeFamily::SchwarzDSheet,    "schwarz_d_sheet",    "Schwarz D (sheet)",              false },
        { LatticeFamily::SchwarzDSkeletal, "schwarz_d_skeletal", "Schwarz D (skeletal)",           true  },
        { LatticeFamily::IWPSheet,         "iwp_sheet",          "IWP (sheet)",                    false },
        { LatticeFamily::IWPSkeletal,      "iwp_skeletal",       "IWP (skeletal)",                 true  },
        { LatticeFamily::SimpleCubic,      "simple_cubic",       "Simple cubic struts",            false },
        { LatticeFamily::BCC,              "bcc",                "BCC struts",                     false },
        { LatticeFamily::FCC,              "fcc",                "FCC struts",                     false },
        { LatticeFamily::Octet,            "octet",              "Octet truss",                    false },
        { LatticeFamily::Honeycomb,        "honeycomb",          "Hexagonal honeycomb",            false },
        { LatticeFamily::Reentrant,        "reentrant",          "Re-entrant honeycomb (auxetic)", false },
        { LatticeFamily::Spinodoid,        "spinodoid",          "Spinodoid (stochastic)",         true  },
        { LatticeFamily::FourierSheet,     "fourier_sheet",      "Fourier series (sheet)",         false },
        { LatticeFamily::FourierSkeletal,  "fourier_skeletal",   "Fourier series (skeletal)",      true  },
        { LatticeFamily::NoiseSheet,       "noise_sheet",        "Smoothed noise (sheet)",         false },
        { LatticeFamily::NoiseSkeletal,    "noise_skeletal",     "Smoothed noise (skeletal)",      true  },
        { LatticeFamily::PlateSC,          "plate_sc",           "Plate lattice {100} (SC)",       false },
        { LatticeFamily::PlateBCC,         "plate_bcc",          "Plate lattice {110} (BCC)",      false },
        { LatticeFamily::PlateFCC,         "plate_fcc",          "Plate lattice {111} (FCC)",      false },
        { LatticeFamily::PlateSCBCC,       "plate_sc_bcc",       "Plate lattice SC+BCC",           false },
        { LatticeFamily::PlateSCFCC,       "plate_sc_fcc",       "Plate lattice SC+FCC",           false },
        { LatticeFamily::PlateBCCFCC,      "plate_bcc_fcc",      "Plate lattice BCC+FCC",          false },
        { LatticeFamily::PlateSCBCCFCC,    "plate_sc_bcc_fcc",   "Plate lattice SC+BCC+FCC",       false },
        { LatticeFamily::SpheresSC,        "spheres_sc",         "Spheres at SC sites",            false },
        { LatticeFamily::SpheresBCC,       "spheres_bcc",        "Spheres at BCC sites",           false },
        { LatticeFamily::SpheresFCC,       "spheres_fcc",        "Spheres at FCC sites",           false },
        { LatticeFamily::CylindersAxes,    "cylinders_axes",     "Cylinders along the axes",       false },
        { LatticeFamily::CubicAssembly,    "cubic_assembly",     "Cubic assembly (plates, struts, shells)", false },
        { LatticeFamily::FoamKelvin,       "foam_kelvin",        "Foam: Kelvin cells (BCC sites)",  false },
        { LatticeFamily::FoamRhombic,      "foam_rhombic",       "Foam: rhombic dodecahedra (FCC sites)", false },
        { LatticeFamily::FoamWeairePhelan, "foam_weaire_phelan", "Foam: Weaire-Phelan (A15 sites)", false },
        { LatticeFamily::FoamRandom,       "foam_random",        "Foam: random Voronoi cells",      false },
        { LatticeFamily::Mycelium,         "mycelium",           "Mycelium (grown hyphae)",         false },
    };
    return t;
}

/// Bit mask of the plane families a plate lattice uses: 1 = {100}, 2 = {110},
/// 4 = {111}; 0 for every other family.
inline int plateOrientations(LatticeFamily f)
{
    switch (f) {
        case LatticeFamily::PlateSC:       return 1;
        case LatticeFamily::PlateBCC:      return 2;
        case LatticeFamily::PlateFCC:      return 4;
        case LatticeFamily::PlateSCBCC:    return 1 | 2;
        case LatticeFamily::PlateSCFCC:    return 1 | 4;
        case LatticeFamily::PlateBCCFCC:   return 2 | 4;
        case LatticeFamily::PlateSCBCCFCC: return 1 | 2 | 4;
        default:                           return 0;
    }
}
inline bool isPlateFamily(LatticeFamily f) { return plateOrientations(f) != 0; }

/// Spheres at lattice sites and axis cylinders (distance to a point / line set).
inline bool isSiteFamily(LatticeFamily f)
{
    return f == LatticeFamily::SpheresSC || f == LatticeFamily::SpheresBCC
        || f == LatticeFamily::SpheresFCC || f == LatticeFamily::CylindersAxes;
}

/// 3D honeycombs: walls / windowed walls / edges of a Voronoi partition (one site table).
inline bool isFoamFamily(LatticeFamily f)
{
    return f == LatticeFamily::FoamKelvin || f == LatticeFamily::FoamRhombic
        || f == LatticeFamily::FoamWeairePhelan || f == LatticeFamily::FoamRandom;
}

/// Families built from strut / wall centre-line segments (one segment table).
inline bool isSegmentFamily(LatticeFamily f)
{
    switch (f) {
        case LatticeFamily::SimpleCubic: case LatticeFamily::BCC: case LatticeFamily::FCC:
        case LatticeFamily::Octet: case LatticeFamily::Honeycomb: case LatticeFamily::Reentrant:
            return true;
        default:
            return false;
    }
}

/// Edge of the grid the smoothed-noise families draw their white noise on,
/// whatever the cube size; the cube interpolates between its nodes. Drawn on
/// the cube's own grid, one seed gave another foam at 128^3 than at 64^3.
/// Written to geometry/noise_grid of a noise cell.
constexpr int kNoiseGrid = 64;

/// Families drawn from the seed (random field state that cannot be shared).
inline bool isRandomFamily(LatticeFamily f)
{
    switch (f) {
        case LatticeFamily::Spinodoid: case LatticeFamily::FourierSheet: case LatticeFamily::FourierSkeletal:
        case LatticeFamily::NoiseSheet: case LatticeFamily::NoiseSkeletal:
        case LatticeFamily::CubicAssembly: case LatticeFamily::FoamRandom: case LatticeFamily::Mycelium:
            return true;
        default:
            return false;
    }
}

inline const LatticeFamilyInfo& latticeFamilyInfo(LatticeFamily f)
{
    for (const LatticeFamilyInfo& i : latticeFamilies())
        if (i.family == f) return i;
    return latticeFamilies().front();
}

inline QString latticeFamilyKey(LatticeFamily f)   { return QString::fromLatin1(latticeFamilyInfo(f).key); }
inline QString latticeFamilyLabel(LatticeFamily f) { return QString::fromLatin1(latticeFamilyInfo(f).label); }

inline QStringList latticeFamilyKeys()
{
    QStringList l;
    for (const LatticeFamilyInfo& i : latticeFamilies()) l << QString::fromLatin1(i.key);
    return l;
}

inline QStringList latticeFamilyLabels()
{
    QStringList l;
    for (const LatticeFamilyInfo& i : latticeFamilies()) l << QString::fromLatin1(i.label);
    return l;
}

/**
 * @brief Accepts the CLI key ("gyroid_sheet"), the panel label
 *        ("Gyroid (sheet)") and loose spellings of either: case, spaces,
 *        hyphens, underscores and parentheses are ignored. A bare TPMS name
 *        ("gyroid") means its sheet variant.
 */
inline bool parseLatticeFamily(const QString& text, LatticeFamily& out)
{
    auto squash = [](const QString& s) {
        QString r;
        for (QChar c : s)
            if (c.isLetterOrNumber()) r += c.toLower();
        return r;
    };
    const QString q = squash(text);
    if (q.isEmpty()) return false;

    for (const LatticeFamilyInfo& i : latticeFamilies()) {
        if (q == squash(QString::fromLatin1(i.key)) || q == squash(QString::fromLatin1(i.label))) {
            out = i.family;
            return true;
        }
    }

    struct Alias { const char* text; LatticeFamily family; };
    static const Alias aliases[] = {
        { "gyroid",             LatticeFamily::GyroidSheet },
        { "schwarzp",           LatticeFamily::SchwarzPSheet },
        { "primitive",          LatticeFamily::SchwarzPSheet },
        { "schwarzd",           LatticeFamily::SchwarzDSheet },
        { "diamond",            LatticeFamily::SchwarzDSheet },
        { "iwp",                LatticeFamily::IWPSheet },
        { "sc",                 LatticeFamily::SimpleCubic },
        { "cubic",              LatticeFamily::SimpleCubic },
        { "octettruss",         LatticeFamily::Octet },
        { "hex",                LatticeFamily::Honeycomb },
        { "hexagonal",          LatticeFamily::Honeycomb },
        { "auxetic",            LatticeFamily::Reentrant },
        { "reentranthoneycomb", LatticeFamily::Reentrant },
        { "spinodal",           LatticeFamily::Spinodoid },
        { "fourier",            LatticeFamily::FourierSheet },
        { "waves",              LatticeFamily::FourierSheet },
        { "noise",              LatticeFamily::NoiseSkeletal },
        { "smoothednoise",      LatticeFamily::NoiseSkeletal },
        { "plate",              LatticeFamily::PlateSC },
        { "plates",             LatticeFamily::PlateSC },
        { "platelattice",       LatticeFamily::PlateSC },
        { "spheres",            LatticeFamily::SpheresSC },
        { "cylinders",          LatticeFamily::CylindersAxes },
        { "channels",           LatticeFamily::CylindersAxes },
        { "assembly",           LatticeFamily::CubicAssembly },
        { "cubicrandom",        LatticeFamily::CubicAssembly },
        { "randomcubic",        LatticeFamily::CubicAssembly },
        { "foam",               LatticeFamily::FoamKelvin },
        { "kelvin",             LatticeFamily::FoamKelvin },
        { "kelvinfoam",         LatticeFamily::FoamKelvin },
        { "honeycomb3d",        LatticeFamily::FoamKelvin },
        { "rhombicdodecahedra", LatticeFamily::FoamRhombic },
        { "weairephelan",       LatticeFamily::FoamWeairePhelan },
        { "randomfoam",         LatticeFamily::FoamRandom },
        { "voronoifoam",        LatticeFamily::FoamRandom },
        { "fungus",             LatticeFamily::Mycelium },
        { "fungal",             LatticeFamily::Mycelium },
        { "hyphae",             LatticeFamily::Mycelium },
        { "hyphalnetwork",      LatticeFamily::Mycelium },
    };
    for (const Alias& a : aliases)
        if (q == QLatin1String(a.text)) { out = a.family; return true; }

    return false;
}

#endif // LATTICEFAMILY_H

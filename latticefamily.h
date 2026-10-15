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
    FourierSheet, FourierSkeletal
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
    };
    return t;
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
    };
    for (const Alias& a : aliases)
        if (q == QLatin1String(a.text)) { out = a.family; return true; }

    return false;
}

#endif // LATTICEFAMILY_H

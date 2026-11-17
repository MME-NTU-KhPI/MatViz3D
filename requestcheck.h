#ifndef REQUESTCHECK_H
#define REQUESTCHECK_H

#include <map>
#include <string>
#include <vector>

/**
 * Conflicts between the target properties of the Neural tab, before anything is
 * generated. Plain C++ (no Qt, no data): only what holds for every cell of the
 * material, whatever its shape, and what the asked numbers say about each other.
 * Whether some cell of a dataset has such a combination is deliberately not a
 * criterion: the generator is meant to reach shapes no dataset has.
 *
 * Two kinds of conflict:
 *  - a bound: no shape of this material at this density can be that stiff.
 *    Voigt bound for any cell: the stiffness tensor of a porous cell is at most
 *    density x that of the solid, so a modulus along a direction is at most
 *    density x the solid's modulus along it (E x, E y <= rho E; E z <= rho r E with
 *    r = E across / E along the printed layers; G xy <= rho G; G xz, G yz <= rho r G;
 *    the Reuss bulk modulus K <= rho K of the solid). For a cell asked to be
 *    equally stiff in every direction (E max / min or G max / min at most 1.05,
 *    A^U at most 0.05) the Hashin-Shtrikman bounds of an isotropic porous solid,
 *    computed with the solid taken isotropic with E along the layers: a solid
 *    weaker across the layers (r < 1) is softer in every load, so the bound
 *    stays valid.
 *  - a joint bound: each modulus under its own limit, yet no cell has them together, because material lined up
 *    with the axes (E) and across them at 45° (G) draw on the same solid. The Hashin-Shtrikman trace bounds of an
 *    anisotropic porous cell (Milton & Kohn 1988): with A = C_s - C in Mandel form, Tr(Λh A⁻¹) <= 1 / (3 (K_s - k))
 *    and Tr(Λs A⁻¹) <= 5 / (2 (G_s - g)), k and g the isotropic Hashin-Shtrikman bounds. The compliances the request
 *    leaves open are searched (an orthotropic cell suffices: the bounds are convex in the compliance and the axis
 *    values do not change under the cell's mirror images). Reported with what does fit: the largest G with the asked
 *    E and the other way round, and the density both need. Read from E, G and nu along the axes; not for a cell asked
 *    to be isotropic (bound above) or a solid stiffer across the layers (r > 1).
 *  - a contradiction: the asked numbers cannot belong to one elastic material.
 *    E and Poisson's ratios whose compliance is not positive; a ratio target that
 *    disagrees with the moduli asked along the axes; a ratio over all directions
 *    (E max / min, G max / min) below a ratio of the axes, or nu min above an
 *    axis Poisson's ratio; a cell asked to be isotropic with ratios other than 1
 *    or with G other than E / (2 (1 + nu)); the bulk modulus or the Zener ratio
 *    other than what the asked E, G, nu make.
 *
 * Keys and units are those of the tab's request (PropertySelectorController::search):
 * moduli Ex ... Gmean and K in MPa, the ratios as plain numbers, the Poisson's
 * ratios, A^U, and the density (share of solid) under "density".
 */
namespace reqcheck {

/// The printing material: E along the layers in GPa, Poisson's ratio, E across the layers / E along them.
struct Material {
    double eGpa = 2.0;
    double nu = 0.38;
    double aniso = 1.0;
};

struct Issue {
    std::vector<std::string> keys;   ///< the targets involved (the density as "density")
    std::string text;                ///< one or two sentences with the numbers, English, UTF-8
};

/// How the form was filled in, where the numbers alone do not say it.
struct Form {
    /// "Same in all directions": E, G and nu were given once for every direction (sent as three equal axis
    /// values), so a ratio target that asks for unequal axes, or E max / min, G max / min above 1, contradicts it.
    bool sameInAllDirections = false;
};

/// Every conflict of the asked targets with each other and with the material. Empty: none found.
std::vector<Issue> check(const std::map<std::string, double>& targets, const Material& material, const Form& form = {});

/// The Voigt limit of a modulus target per unit density, MPa (0 for a key that is not a modulus).
double voigtPerDensity(const std::string& key, const Material& material);
/// Hashin-Shtrikman upper bounds of an isotropic porous cell of an isotropic solid (E in MPa, nu) at density rho, MPa.
void hashinShtrikman(double eMPa, double nu, double rho, double* E, double* G, double* K);
/// "E x", "ν xy", "E z / E x" ...: the name of a key as the tab shows it.
std::string label(const std::string& key);

} // namespace reqcheck

#endif // REQUESTCHECK_H

#ifndef PROPERTYSEARCH_H
#define PROPERTYSEARCH_H

#include <QString>
#include <array>
#include <functional>
#include <vector>

/**
 * Inverse lookup in a computed dataset: the accepted cells whose effective
 * elastic properties come closest to a set of targets, for the printing
 * material the user will actually print with.
 *
 * Every cell was solved with its own material law (E, nu, weakening across the
 * layers). A cell made of a single material is linear in it: scaling the law's
 * E scales C exactly (the void phase is a fixed fraction of the solid). A
 * change of nu or of the anisotropy is carried to first order by the cell's
 * sensitivity tensor (Hill's lemma, FFTSolverSession::sensitivity_tensor), an
 * upper estimate of C that grows less accurate the further the law moves, so
 * the ranking adds an uncertainty term for that step and prefers cells solved
 * with a law close to the user's.
 *
 * Matrices are in the pipeline order (xx, yy, zz, xy, yz, xz) with engineering
 * shear (C44 = G). Pure logic on top of HDF5Wrapper, no GUI.
 */
namespace propsearch {

using Mat6 = std::array<std::array<double, 6>, 6>;

/// A printing material: E in GPa, Poisson's ratio, E across the layers / E along them.
struct Law {
    double eGpa = 2.0;
    double nu = 0.38;
    double aniso = 1.0;
};

/// What the search needs of one accepted cell, read once from its HDF5 file.
struct Cell {
    QString id, group, family, file;
    double density = 0.0;   ///< achieved solid fraction
    double cellMm = 0.0;    ///< edge of one unit cell
    double wallMm = 0.0;    ///< estimated thinnest wall or strut (0 = unknown)
    Law law;                ///< the material the cell was solved with
    Mat6 C{};               ///< effective stiffness, Pa
};

/// Targets: the moduli and Poisson's ratios along the axes, the level of E and G
/// (geometric mean over the three axes / planes) and four anisotropy measures:
/// E z / E x, E y / E x, E max / E min over all directions, and the Zener ratio
/// 2 C44 / (C11 - C12) of the nearest cubic matrix (searched among near-cubic cells only).
enum Target { Ex, Ey, Ez, Gxy, Gyz, Gxz, NuXy, NuXz, NuYz, EMean, GMean, EzEx, EyEx, EmaxEmin, Zener, TargetCount };
using Values = std::array<double, TargetCount>;

/// Short key of a target as the QML side names it ("Ex", "Gxy", "nuXy", "EzEx", "zener", ...).
QString targetKey(int t);
bool isModulus(int t);
/// The anisotropy ratios; like the moduli they are compared on a log scale.
bool isRatio(int t);

/// A Zener target is searched among cells whose stiffness, for the user's
/// material, is within this distance of a cubic matrix (cubicDeviation()).
constexpr double kMaxCubicDeviation = 0.05;

struct Query {
    Law material;
    std::array<bool, TargetCount> use{};
    Values value{};         ///< moduli in Pa, ratios plain
    double rhoMin = 0.0, rhoMax = 1.0;
    QString family;         ///< empty = every family
    int count = 10;
};

struct Match {
    int cell = -1;          ///< index into the cell list
    Values predicted{};     ///< the cell's properties with the user's material
    double fit = 0.0;       ///< rms deviation from the targets (log for moduli and anisotropy ratios, plain for nu)
    double uncertainty = 0.0; ///< estimated relative error of the material step
    double score = 0.0;     ///< sqrt(fit^2 + uncertainty^2), the ranking key
    bool corrected = false; ///< the sensitivity tensor was applied
    double cubic = 0.0;     ///< cubicDeviation() of the predicted stiffness
};

/// Effective stiffness and what the search needs of one cell file. Files without
/// the shear_basis attribute (dataset v1) hold the tensor basis and are converted.
bool readCell(const QString& file, Cell& cell, QString* error = nullptr);
/// The cell's 1296-number sensitivity tensor, in the engineering basis.
bool readSensitivity(const QString& file, std::vector<double>& T);

/// E x/y/z, G xy/yz/xz (Pa), nu xy/xz/yz and the derived levels and ratios of a
/// stiffness matrix; NaN if singular. `allDirections` = false skips E max / E min
/// (a search over the sphere, the only costly entry).
Values moduli(const Mat6& C, bool allDirections = true);
/// Relative distance (Frobenius) of C from the cubic matrix with the mean C11, C12
/// and C44 of its three axes: 0 for a cubic or isotropic cell.
double cubicDeviation(const Mat6& C);
/// Extremes of the directional Young's modulus 1 / (n n : S : n n) over all unit n.
void directionalModulus(const Mat6& C, double& eMin, double& eMax);
/// The law's stiffness in Mandel form, pipeline order (what the tensor pairs with).
bool lawMandel(const Law& law, Mat6& M);
/// The cell's stiffness had it been printed in `law`: exact E scaling, then the
/// first-order nu / anisotropy step when T is given.
Mat6 predictStiffness(const Cell& cell, const Law& law, const std::vector<double>* T);
/// Size of the step from one law to another at equal E: the largest |ln| of the
/// eigenvalues of M_from^-1/2 M_to M_from^-1/2 (0 = same material).
double lawStep(const Law& from, const Law& to);
/// Estimated relative error of a prediction after a law step of size s: the
/// first-order (tensor) step errs by ~0.18 s^2, plain E scaling by ~0.5 s.
/// Calibrated on a plate_sc cell re-solved with four other laws.
inline double tensorStepError(double s) { return 0.18 * s * s; }
inline double scalingStepError(double s) { return 0.5 * s; }
double fitError(const Values& predicted, const Query& q);

/// Ranked matches, best first: every cell scored with the exact E scaling, the
/// best few hundred re-scored with their sensitivity tensors through `tensor`.
/// With a Zener target only cells within kMaxCubicDeviation of cubic remain.
std::vector<Match> search(const std::vector<Cell>& cells, const Query& q,
                          const std::function<bool(const Cell&, std::vector<double>&)>& tensor);

/// Hashin-Shtrikman upper bound on the Young's modulus of an isotropic porous
/// solid (the same formula as hs_upper_E in tools/dataset/make_dataset.py).
double hsUpperE(double Es, double nus, double porosity);

} // namespace propsearch

#endif // PROPERTYSEARCH_H

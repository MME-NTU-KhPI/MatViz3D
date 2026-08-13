#ifndef TEXTURELIBRARY_H
#define TEXTURELIBRARY_H

#include <vector>
#include <array>
#include <random>
#include <string>

/**
 * @brief Generation of crystallographic textures for export to ANSYS.
 *
 *   Cube  {001}<100>  -> ANSYS (   0.00,  0.00,   0.00)
 *   Goss  {110}<001>  -> ANSYS (-180.00, 45.00, -90.00)
 *   Copper{112}<11-1> -> ANSYS ( -39.23, 24.09, -26.57)
 *   Brass {110}<112>  -> ANSYS (-144.74, 45.00, -90.00)
 *   S     {123}<634>  -> ANSYS ( -27.03, 32.31, -18.43)
 */
class TextureLibrary
{
public:
    using Matrix3 = std::array<std::array<double, 3>, 3>;

    struct Component {
        int    hkl[3];
        int    uvw[3];
        double scatter_deg;
        double weight;
        std::string name;     // "Copper", "Goss", "<111> fiber", ...
        bool   is_fiber = false;
        bool   is_random = false;
        // Hard-capped perturbation: instead of the Gaussian applyScatter() used
        // by the other components, rotate the ideal orientation about a
        // uniformly random axis by an angle uniform in [0, scatter_deg]. No
        // grain can ever deviate from the ideal by more than scatter_deg.
        bool   is_capped = false;
    };

    enum class Lattice { FCC, BCC };

    // Keep the order in sync with TextureView.qml's processList (the QML index
    // is cast straight to this enum) and with parseProcess() in
    // commandline_parser.cpp.
    enum class Process { Random, Extrusion, Rolling, Recrystallization, Shear, ScatteredCube };

    enum class Mode {
        Cube,
        Random,
        Textured
    };

    explicit TextureLibrary(unsigned int seed = 0);

    void setSeed(unsigned int seed);
    void setMode(Mode mode);
    void setComponents(const std::vector<Component>& comps);
    void clear();

    void sampleNext(double angl[3], bool in_deg = true);
    void sampleNextBunge(double bunge[3], bool in_deg = true);

    static void bungeFromPassive(const Matrix3& g,
                                double& phi1, double& Phi, double& phi2);
    static void bungeToAnsys(double phi1, double Phi, double phi2,
                             double& thxy, double& thyz, double& thzx);

    static void millerToBunge(const int hkl[3], const int uvw[3],
                              double& phi1, double& Phi, double& phi2);

    static std::vector<Component> presetCatalog();

    static std::vector<Component> componentsForProcess(Process p, Lattice lat,
                                                       double scatter_deg);

    static bool runSelfTest();

    static std::vector<std::array<double,3>> fundamentalZoneBunge(double phi1, double Phi, double phi2);

private:
    static Matrix3 orientationFromMiller(const int hkl[3], const int uvw[3]);
    static Matrix3 orientationFromMillerPassive(const int hkl[3], const int uvw[3]);
    static Matrix3 bungeToMatrix(double phi1, double Phi, double phi2);   // active
    static void    matrixToAnsys(const Matrix3& R,
                                 double& thxy, double& thyz, double& thzx,
                                 bool in_deg);
    static Matrix3 randomMatrix(std::mt19937& rng);
    static Matrix3 fiberMatrix(const int uvw[3], std::mt19937& rng);
    static Matrix3 applyScatter(const Matrix3& ideal, double scatter_deg,
                                std::mt19937& rng);
    static Matrix3 applyCappedScatter(const Matrix3& ideal, double max_deg,
                                      std::mt19937& rng);
    static Matrix3 matmul(const Matrix3& A, const Matrix3& B);

    const Component& pickComponent();

    Mode                    m_mode = Mode::Cube;
    std::vector<Component>  m_components;
    std::vector<double>     m_cumWeights;
    std::mt19937            m_rng;
};

#endif // TEXTURELIBRARY_H

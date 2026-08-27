#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <QObject>
#include <QString>
#include <vector>
#include "texturelibrary.h"
#include "phasematerial.h"

class Parameters : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int size READ getSize WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(int points READ getPoints WRITE setPoints NOTIFY pointsChanged)
    Q_PROPERTY(QString algorithm READ getAlgorithm WRITE setAlgorithm NOTIFY algorithmChanged)
    Q_PROPERTY(unsigned int seed READ getSeed WRITE setSeed NOTIFY seedChanged)
    Q_PROPERTY(QString filename READ getFilename WRITE setFilename NOTIFY filenameChanged)
    Q_PROPERTY(int num_threads READ getNumThreads WRITE setNumThreads NOTIFY numThreadsChanged)
    Q_PROPERTY(QString working_directory READ getWorkingDirectory WRITE setWorkingDirectory NOTIFY workingDirectoryChanged)
    Q_PROPERTY(float wave_coefficient READ getWaveCoefficient WRITE setWaveCoefficient NOTIFY waveCoefficientChanged)

    Q_PROPERTY(QString prob_preset READ getProbPreset WRITE setProbPreset NOTIFY probPresetChanged)
    Q_PROPERTY(float halfaxis_a READ getHalfAxisA WRITE setHalfAxisA NOTIFY halfAxisAChanged)
    Q_PROPERTY(float halfaxis_b READ getHalfAxisB WRITE setHalfAxisB NOTIFY halfAxisBChanged)
    Q_PROPERTY(float halfaxis_c READ getHalfAxisC WRITE setHalfAxisC NOTIFY halfAxisCChanged)

    Q_PROPERTY(float orientation_angle_a READ getOrientationAngleA WRITE setOrientationAngleA NOTIFY orientationAngleAChanged)
    Q_PROPERTY(float orientation_angle_b READ getOrientationAngleB WRITE setOrientationAngleB NOTIFY orientationAngleBChanged)
    Q_PROPERTY(float orientation_angle_c READ getOrientationAngleC WRITE setOrientationAngleC NOTIFY orientationAngleCChanged)

    Q_PROPERTY(QString pointsMode READ getPointsMode WRITE setPointsMode NOTIFY pointsModeChanged)
    Q_PROPERTY(bool isAnimation READ getIsAnimation WRITE setIsAnimation NOTIFY isAnimationChanged)
    Q_PROPERTY(bool isGifRecording READ getIsGifRecording WRITE setIsGifRecording NOTIFY isGifRecordingChanged)

    Q_PROPERTY(bool hasProbParameters READ getHasProbParameters WRITE setHasProbParameters NOTIFY hasProbParametersChanged)
    Q_PROPERTY(double ellipse_order READ getEllipseOrder WRITE setEllipseOrder NOTIFY ellipseOrderChanged)

    Q_PROPERTY(QString material  READ getMaterial  WRITE setMaterial  NOTIFY materialChanged)
    Q_PROPERTY(QString material1 READ getMaterial1 WRITE setMaterial1 NOTIFY material1Changed)
    Q_PROPERTY(QString material2 READ getMaterial2 WRITE setMaterial2 NOTIFY material2Changed)

    // Minkowski exponent p of the distance used by tessellation algorithms:
    // 1 = Manhattan (octahedral grains), 2 = Euclidean, large = Chebyshev
    // (cuboidal grains).
    Q_PROPERTY(double minkowski_p READ getMinkowskiP WRITE setMinkowskiP NOTIFY minkowskiPChanged)

    // Wrap the RVE on a torus, so grains that leave one face re-enter the
    // opposite one. Required for a periodic homogenization cell.
    Q_PROPERTY(bool is_periodic READ getIsPeriodic WRITE setIsPeriodic NOTIFY isPeriodicChanged)

    // Material picked from material_properties.db. Selecting one loads its
    // cubic constants (used by both stress solvers) and its lattice type
    // (used to build the texture presets).
    Q_PROPERTY(QString db_material READ getDbMaterial WRITE setDbMaterial NOTIFY dbMaterialChanged)

    // ── Composite (fiber-reinforced RVE) ─────────────────────────────────
    // Reinforcement dimensionality: "1D" (fibers along Z), "2D" (X and Y) or
    // "3D" (X, Y and Z). Stored as the combo-box label, like texture_preset.
    Q_PROPERTY(QString composite_dim     READ getCompositeDim     WRITE setCompositeDim     NOTIFY compositeSettingsChanged)
    // In-plane arrangement of one fiber family: "Square" or "Hexagonal".
    Q_PROPERTY(QString composite_packing READ getCompositePacking WRITE setCompositePacking NOTIFY compositeSettingsChanged)

    // Target fiber volume fraction. This is the *input*: the fiber semi-axes
    // are solved so the rasterized structure actually hits it.
    Q_PROPERTY(double fiber_volume_fraction READ getFiberVolumeFraction WRITE setFiberVolumeFraction NOTIFY compositeSettingsChanged)
    Q_PROPERTY(int    fibers_per_row        READ getFibersPerRow        WRITE setFibersPerRow        NOTIFY compositeSettingsChanged)

    // RVE imperfections. a/b = 1 with no scatter and no jitter is the perfect
    // lattice of circular fibers.
    Q_PROPERTY(double fiber_aspect_ratio  READ getFiberAspectRatio  WRITE setFiberAspectRatio  NOTIFY compositeSettingsChanged)
    Q_PROPERTY(double fiber_angle_scatter READ getFiberAngleScatter WRITE setFiberAngleScatter NOTIFY compositeSettingsChanged)
    Q_PROPERTY(double fiber_center_jitter READ getFiberCenterJitter WRITE setFiberCenterJitter NOTIFY compositeSettingsChanged)
    Q_PROPERTY(bool   fiber_allow_overlap READ getFiberAllowOverlap WRITE setFiberAllowOverlap NOTIFY compositeSettingsChanged)

    // The two constituents, both from material_properties.db. The fiber's
    // material axes follow the fiber (axis 3 = fiber axis), so a transversely
    // isotropic row like C-fiber ends up stiff along the fiber.
    Q_PROPERTY(QString matrix_material READ getMatrixMaterial WRITE setMatrixMaterial NOTIFY compositeSettingsChanged)
    Q_PROPERTY(QString fiber_material  READ getFiberMaterial  WRITE setFiberMaterial  NOTIFY compositeSettingsChanged)

    Q_PROPERTY(QString texture_preset  READ getTexturePreset  WRITE setTexturePreset  NOTIFY textureSettingsChanged)
    Q_PROPERTY(double  texture_scatter READ getTextureScatter WRITE setTextureScatter NOTIFY textureSettingsChanged)

    Q_PROPERTY(float wave_spread          READ getWaveSpread         WRITE setWaveSpread         NOTIFY waveSpreadChanged)
    Q_PROPERTY(float stefan_number        READ getStefanNumber       WRITE setStefanNumber       NOTIFY stefanNumberChanged)
    Q_PROPERTY(int   initial_nuclei_count READ getInitialNucleiCount WRITE setInitialNucleiCount NOTIFY initialNucleiCountChanged)
    Q_PROPERTY(unsigned int num_rnd_loads READ getNumRndLoads        WRITE setNumRndLoads        NOTIFY numRndLoadsChanged)

public:
    explicit Parameters(QObject* parent = nullptr);

    static Parameters* instance()
    {
        static Parameters instance;
        return &instance;
    }

    int getSize() const { return size; }
    Q_INVOKABLE void setSize(int value);

    int getPoints() const { return points; }
    Q_INVOKABLE void setPoints(int value);

    QString getAlgorithm() const { return algorithm; }
    Q_INVOKABLE void setAlgorithm(const QString& value);

    unsigned int getSeed() const { return seed; }
    Q_INVOKABLE void setSeed(unsigned int value);

    QString getFilename() const { return filename; }
    Q_INVOKABLE void setFilename(const QString& value);

    int getNumThreads() const { return num_threads; }
    Q_INVOKABLE void setNumThreads(int value);

    QString getWorkingDirectory() const { return working_directory; }
    Q_INVOKABLE void setWorkingDirectory(const QString& value);

    float getWaveCoefficient() const { return wave_coefficient; }
    Q_INVOKABLE void setWaveCoefficient(float value);

    QString getProbPreset() const { return prob_preset; }
    Q_INVOKABLE void setProbPreset(const QString& value);

    float getHalfAxisA() const { return halfaxis_a; }
    Q_INVOKABLE void setHalfAxisA(float value);

    float getHalfAxisB() const { return halfaxis_b; }
    Q_INVOKABLE void setHalfAxisB(float value);

    float getHalfAxisC() const { return halfaxis_c; }
    Q_INVOKABLE void setHalfAxisC(float value);

    float getOrientationAngleA() const { return orientation_angle_a; }
    Q_INVOKABLE void setOrientationAngleA(float value);

    float getOrientationAngleB() const { return orientation_angle_b; }
    Q_INVOKABLE void setOrientationAngleB(float value);

    float getOrientationAngleC() const { return orientation_angle_c; }
    Q_INVOKABLE void setOrientationAngleC(float value);

    QString getPointsMode() const { return points_mode; }
    Q_INVOKABLE void setPointsMode(const QString& value);

    bool getIsAnimation() const { return isAnimation; }
    Q_INVOKABLE void setIsAnimation(bool value);

    bool getIsGifRecording() const { return isGifRecording; }
    Q_INVOKABLE void setIsGifRecording(bool value);

    bool getHasProbParameters() const { return hasProbParameters; }
    Q_INVOKABLE void setHasProbParameters(bool value);

    double getEllipseOrder() const { return ellipse_order; }
    Q_INVOKABLE void setEllipseOrder(double value);

    QString getStressSolver() const { return stressSolver; }
    Q_INVOKABLE void setStressSolver(const QString& value);

    QString getStressMode() const { return stressMode; }
    Q_INVOKABLE void setStressMode(const QString& value);

    const double* getStressEps() const { return stressEps; }
    void setStressEps(const double value[6]);

    QString getMaterial()  const { return m_material; }
    QString getMaterial1() const { return m_material1; }
    QString getMaterial2() const { return m_material2; }

    Q_INVOKABLE void setMaterial(const QString& value);
    Q_INVOKABLE void setMaterial1(const QString& value);
    Q_INVOKABLE void setMaterial2(const QString& value);

    Q_INVOKABLE void processPointInput(const QString &text);

    /**
     * @brief The current point count expressed in the units of the current
     *        points mode: a raw count under "count", a percentage of the cube
     *        volume under "density".
     *
     * Lets the UI convert the field in place when the mode is switched, so the
     * structure being described does not change just because the unit did.
     */
    Q_INVOKABLE QString pointsDisplayValue() const;

    double getMinkowskiP() const { return minkowski_p; }
    Q_INVOKABLE void setMinkowskiP(double value);

    bool getIsPeriodic() const { return is_periodic; }
    Q_INVOKABLE void setIsPeriodic(bool value);

    QString getDbMaterial() const { return db_material; }
    Q_INVOKABLE void setDbMaterial(const QString& value);

    // Returns the UI label ("Scattered cube"), not the normalised internal
    // spelling, so the combo box can match it against its own option list.
    QString getTexturePreset() const { return texturePresetLabel(); }
    Q_INVOKABLE void setTexturePreset(const QString& value);

    static QString texturePresetLabel();

    /// Forces the lattice the texture presets are built for, overriding the
    /// selected material's Type column. Empty string = follow the material.
    Q_INVOKABLE void setLatticeOverride(const QString& value);

    double getTextureScatter() const { return texture_scatter; }
    Q_INVOKABLE void setTextureScatter(double value);

    // ── Composite ─────────────────────────────────────────────────────────
    // Getters return the combo-box label so the panel can match its own option
    // list; the setters accept both the labels and the CLI spellings.
    QString getCompositeDim()     const { return compositeDimLabel(); }
    QString getCompositePacking() const { return compositePackingLabel(); }
    Q_INVOKABLE void setCompositeDim(const QString& value);
    Q_INVOKABLE void setCompositePacking(const QString& value);

    static QString compositeDimLabel();
    static QString compositePackingLabel();

    /// 1, 2 or 3 -- how many orthogonal fiber families the RVE carries.
    static int  compositeDimensions();
    /// True when the fibers of one family sit on a staggered (hexagonal)
    /// lattice rather than a plain rectangular one.
    static bool compositeHexagonal();

    double getFiberVolumeFraction() const { return fiber_volume_fraction; }
    int    getFibersPerRow()        const { return fibers_per_row; }
    double getFiberAspectRatio()    const { return fiber_aspect_ratio; }
    double getFiberAngleScatter()   const { return fiber_angle_scatter; }
    double getFiberCenterJitter()   const { return fiber_center_jitter; }
    bool   getFiberAllowOverlap()   const { return fiber_allow_overlap; }
    QString getMatrixMaterial()     const { return matrix_material; }
    QString getFiberMaterial()      const { return fiber_material; }

    Q_INVOKABLE void setFiberVolumeFraction(double value);
    Q_INVOKABLE void setFibersPerRow(int value);
    Q_INVOKABLE void setFiberAspectRatio(double value);
    Q_INVOKABLE void setFiberAngleScatter(double value);
    Q_INVOKABLE void setFiberCenterJitter(double value);
    Q_INVOKABLE void setFiberAllowOverlap(bool value);
    Q_INVOKABLE void setMatrixMaterial(const QString& value);
    Q_INVOKABLE void setFiberMaterial(const QString& value);

    /**
     * @brief Cubic single-crystal constants of the selected material, in Pa.
     *
     * Both stress solvers call this instead of hardcoding numbers, so the
     * material dropdown actually changes what is solved. Falls back to the
     * historical Cu values (168.4 / 121.4 / 75.4 GPa) when no material has
     * been selected or the database cannot be read.
     */
    static void cubicConstantsPa(double& c11, double& c12, double& c44);

    /**
     * @brief Lattice implied by the selected material's Type column, for
     *        building texture presets. Anything that is not 'bcc' (including
     *        the diamond-cubic / zincblende / rocksalt rows, which share the
     *        FCC sublattice) maps to FCC.
     */
    static TextureLibrary::Lattice materialLattice();

    /**
     * @brief Rebuilds textureComponents from texture_preset / texture_scatter
     *        and the material's lattice.
     *
     * A no-op while the preset is "custom", which is what the interactive
     * texture editor sets when it applies -- so an edited texture is never
     * silently overwritten by a preset rebuild.
     */
    static void rebuildTextureFromPreset();

    /// Marks the texture as hand-edited, so rebuildTextureFromPreset() stops
    /// touching it. Called when the texture editor applies.
    void markTextureCustom();

    float getWaveSpread()         const { return wave_spread; }
    float getStefanNumber()       const { return stefan_number; }
    int   getInitialNucleiCount() const { return initial_nuclei_count; }
    unsigned int getNumRndLoads() const { return num_rnd_loads; }

    Q_INVOKABLE void setWaveSpread(float value);
    Q_INVOKABLE void setStefanNumber(float value);
    Q_INVOKABLE void setInitialNucleiCount(int value);
    Q_INVOKABLE void setNumRndLoads(unsigned int value);

    static Parameters* m_instance;

    static int32_t*** voxels;

    static unsigned int seed;
    static QString filename;
    static int num_threads;
    static QString working_directory;
    static float wave_coefficient;
    static float wave_spread;
    static int initial_nuclei_count;
    static QString prob_preset;
    static float halfaxis_a;
    static float halfaxis_b;
    static float halfaxis_c;
    static float orientation_angle_a;
    static float orientation_angle_b;
    static float orientation_angle_c;
    static float stefan_number;
    static double ellipse_order;

    static std::vector<TextureLibrary::Component> textureComponents;

    /**
     * @brief Grain -> (material, orientation) table for multi-phase structures.
     *
     * Published by the generating algorithm (Composite) and read by BOTH stress
     * solvers, the same way textureComponents is. Left empty by every
     * single-phase algorithm, which is what keeps them on the historical
     * "one material from db_material + texture-sampled orientations" path.
     */
    static PhaseAssignment phaseAssignment;

    /**
     * @brief Full anisotropic stiffness of a named database material, in Pa.
     *
     * The generalisation of cubicConstantsPa(): reads all 21 columns, so
     * transversely isotropic rows (carbon fiber) survive instead of being
     * flattened onto a cubic triple. Returns false and leaves C untouched when
     * the material is unknown or has no usable constants.
     */
    static bool materialStiffnessPa(const QString& name, double C[6][6]);

signals:
    void sizeChanged();
    void pointsChanged();
    void algorithmChanged();
    void seedChanged();
    void filenameChanged();
    void numThreadsChanged();
    void workingDirectoryChanged();
    void waveCoefficientChanged();

    void probPresetChanged();
    void halfAxisAChanged();
    void halfAxisBChanged();
    void halfAxisCChanged();

    void orientationAngleAChanged();
    void orientationAngleBChanged();
    void orientationAngleCChanged();

    void pointsModeChanged();
    void isAnimationChanged();
    void isGifRecordingChanged();
    void initialConditionSelectionChanged();

    void hasProbParametersChanged();
    void ellipseOrderChanged();

    void materialChanged();
    void material1Changed();
    void material2Changed();

    void waveSpreadChanged();
    void stefanNumberChanged();
    void initialNucleiCountChanged();

    void minkowskiPChanged();
    void isPeriodicChanged();
    void dbMaterialChanged();
    void textureSettingsChanged();
    void compositeSettingsChanged();

    void numRndLoadsChanged();
    void stressSolverChanged();
    void stressModeChanged();
    void stressEpsChanged();

private:
    static int size;
    static int points;
    static QString algorithm;

    // static unsigned int seed;
    // static QString filename;
    // static int num_threads;
    // static QString working_directory;
    // static float wave_coefficient;
    // static float halfaxis_a;
    // static float halfaxis_b;
    // static float halfaxis_c;
    // static float orientation_angle_a;
    // static float orientation_angle_b;
    // static float orientation_angle_c;

    static QString points_mode; // "count" / "density"
    static bool isAnimation;
    static bool isGifRecording;
    static bool hasProbParameters;
    static unsigned int num_rnd_loads;

    static QString stressSolver;      // "ansys" | "fft"
    static QString stressMode;        // "single" | "dataset"
    static double  stressEps[6];      // exx,eyy,ezz,exy,eyz,exz

    static QString m_material;
    static QString m_material1;
    static QString m_material2;

    static double  minkowski_p;
    static bool    is_periodic;

    static QString db_material;
    // Cubic constants of db_material in GPa, the unit material_properties.db
    // stores; cubicConstantsPa() converts. Defaults are the Cu values the
    // solvers used to hardcode, so an unset material changes nothing.
    static double  mat_c11, mat_c12, mat_c44;
    static QString mat_type;

    static QString texture_preset;
    static double  texture_scatter;
    static QString lattice_override;

    static QString composite_dim;        // "1d" | "2d" | "3d"
    static QString composite_packing;    // "square" | "hexagonal"
    static double  fiber_volume_fraction;
    static int     fibers_per_row;
    static double  fiber_aspect_ratio;
    static double  fiber_angle_scatter;
    static double  fiber_center_jitter;
    static bool    fiber_allow_overlap;
    static QString matrix_material;
    static QString fiber_material;
};

#endif // PARAMETERS_H

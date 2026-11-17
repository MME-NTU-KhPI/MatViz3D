#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <array>
#include <functional>
#include <vector>
#include "texturelibrary.h"
#include "phasematerial.h"
#include "printability.h"
#include "mycelium.h"

class Parameters : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int size READ getSize WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(int points READ getPoints WRITE setPoints NOTIFY pointsChanged)
    Q_PROPERTY(QString algorithm READ getAlgorithm WRITE setAlgorithm NOTIFY algorithmChanged)
    Q_PROPERTY(unsigned int seed READ getSeed WRITE setSeed NOTIFY seedChanged)
    // How the initial crystallization seeds are placed: Random = uniform
    // rejection sampling (historical behaviour), Sobol = scrambled Sobol
    // sequence with Gaussian jitter of seed_sigma (see lowdiscrepancy.hpp).
    Q_PROPERTY(SeedMethod seed_method READ getSeedMethod WRITE setSeedMethod NOTIFY seedMethodChanged)
    Q_PROPERTY(double seed_sigma READ getSeedSigma WRITE setSeedSigma NOTIFY seedSigmaChanged)
    Q_PROPERTY(QString filename READ getFilename WRITE setFilename NOTIFY filenameChanged)
    Q_PROPERTY(int num_threads READ getNumThreads WRITE setNumThreads NOTIFY numThreadsChanged)
    Q_PROPERTY(QString working_directory READ getWorkingDirectory WRITE setWorkingDirectory NOTIFY workingDirectoryChanged)
    Q_PROPERTY(float wave_coefficient READ getWaveCoefficient WRITE setWaveCoefficient NOTIFY waveCoefficientChanged)

    Q_PROPERTY(QString prob_preset READ getProbPreset WRITE setProbPreset NOTIFY probPresetChanged)
    Q_PROPERTY(QString prob_matrix_mode READ getProbMatrixMode WRITE setProbMatrixMode NOTIFY probMatrixModeChanged)
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

    // Generate all initial grain seeds on the same plane (thin layer mode).
    Q_PROPERTY(bool is_thin_layer READ getIsThinLayer WRITE setIsThinLayer NOTIFY isThinLayerChanged)

    // Starting direction of the thin layer film ('+Z', '-Z', '+X', '-X', '+Y', '-Y').
    Q_PROPERTY(QString layer_direction READ getLayerDirection WRITE setLayerDirection NOTIFY layerDirectionChanged)

    // Metric tensor components for anisotropic Voronoi tessellation:
    // M = [mxx mxy mxz; mxy myy myz; mxz myz mzz], default I (isotropic)
    Q_PROPERTY(QString voronoi_metric_preset READ getVoronoiMetricPreset WRITE setVoronoiMetricPreset NOTIFY voronoiMetricPresetChanged)
    Q_PROPERTY(double voronoi_mxx READ getVoronoiMxx WRITE setVoronoiMxx NOTIFY voronoiMetricChanged)
    Q_PROPERTY(double voronoi_myy READ getVoronoiMyy WRITE setVoronoiMyy NOTIFY voronoiMetricChanged)
    Q_PROPERTY(double voronoi_mzz READ getVoronoiMzz WRITE setVoronoiMzz NOTIFY voronoiMetricChanged)
    Q_PROPERTY(double voronoi_mxy READ getVoronoiMxy WRITE setVoronoiMxy NOTIFY voronoiMetricChanged)
    Q_PROPERTY(double voronoi_myz READ getVoronoiMyz WRITE setVoronoiMyz NOTIFY voronoiMetricChanged)
    Q_PROPERTY(double voronoi_mxz READ getVoronoiMxz WRITE setVoronoiMxz NOTIFY voronoiMetricChanged)

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

    // ── Lattice (cellular metamaterial RVE) ───────────────────────────────
    // Cell family: stored as the CLI key (see latticefamily.h), read back as
    // the combo-box label so the panel can match its own option list.
    Q_PROPERTY(QString lattice_family   READ getLatticeFamily   WRITE setLatticeFamily   NOTIFY latticeSettingsChanged)
    Q_PROPERTY(int     cells_per_edge   READ getCellsPerEdge    WRITE setCellsPerEdge    NOTIFY latticeSettingsChanged)
    // Target relative density (solid fraction) of the cell. This is the input;
    // the level/threshold that produces it is solved by the algorithm.
    Q_PROPERTY(double  relative_density READ getRelativeDensity WRITE setRelativeDensity NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  lattice_grading  READ getLatticeGrading  WRITE setLatticeGrading  NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  lattice_jitter   READ getLatticeJitter   WRITE setLatticeJitter   NOTIFY latticeSettingsChanged)
    // Spinodoid: half-angles of the cones of admissible wave directions about
    // X, Y, Z in degrees (0 = that cone closed, 90 = whole sphere).
    Q_PROPERTY(double  spinodoid_theta_x READ getSpinodoidThetaX WRITE setSpinodoidThetaX NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  spinodoid_theta_y READ getSpinodoidThetaY WRITE setSpinodoidThetaY NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  spinodoid_theta_z READ getSpinodoidThetaZ WRITE setSpinodoidThetaZ NOTIFY latticeSettingsChanged)
    // Fourier-series families: random draw controls, or a coefficient file
    // (kx ky kz amplitude phase per line) that overrides the draw.
    Q_PROPERTY(int     fourier_terms READ getFourierTerms WRITE setFourierTerms NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  fourier_kmin  READ getFourierKmin  WRITE setFourierKmin  NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  fourier_kmax  READ getFourierKmax  WRITE setFourierKmax  NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  fourier_decay READ getFourierDecay WRITE setFourierDecay NOTIFY latticeSettingsChanged)
    Q_PROPERTY(QString fourier_file  READ getFourierFile  WRITE setFourierFile  NOTIFY latticeSettingsChanged)
    // Smoothed-noise families: Gaussian sigma as a fraction of the cube edge.
    Q_PROPERTY(double  noise_smooth  READ getNoiseSmooth  WRITE setNoiseSmooth  NOTIFY latticeSettingsChanged)
    // Fourier families: "none" or "cubic" (waves expanded to their orbits under the cubic group).
    Q_PROPERTY(QString fourier_symmetry READ getFourierSymmetry WRITE setFourierSymmetry NOTIFY latticeSettingsChanged)
    // Plate lattices: hole diameter through every panel centre, in cell edges (0 = none).
    Q_PROPERTY(double  plate_hole    READ getPlateHole    WRITE setPlateHole    NOTIFY latticeSettingsChanged)
    // Second cell family united with the first; stored as the key, read back as the label ("none" = off).
    Q_PROPERTY(QString lattice_union READ getLatticeUnion WRITE setLatticeUnion NOTIFY latticeSettingsChanged)
    // Third family carved out of the solid up to its own level (cell edges), and a Gaussian blur of the field (cell edges).
    Q_PROPERTY(QString lattice_subtract      READ getLatticeSubtract     WRITE setLatticeSubtract     NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  lattice_subtract_size READ getLatticeSubtractSize WRITE setLatticeSubtractSize NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  lattice_smooth        READ getLatticeSmooth       WRITE setLatticeSmooth       NOTIFY latticeSettingsChanged)
    // Honeycomb shape: custom on/off, inclined wall angle (deg, negative = re-entrant), rib / wall length.
    Q_PROPERTY(bool    honeycomb_custom READ getHoneycombCustom WRITE setHoneycombCustom NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  honeycomb_angle  READ getHoneycombAngle  WRITE setHoneycombAngle  NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  honeycomb_rib    READ getHoneycombRib    WRITE setHoneycombRib    NOTIFY latticeSettingsChanged)
    // Cubic assembly: members drawn in the asymmetric unit, thickness spread, optional member file.
    Q_PROPERTY(int     assembly_struts  READ getAssemblyStruts  WRITE setAssemblyStruts  NOTIFY latticeSettingsChanged)
    Q_PROPERTY(int     assembly_plates  READ getAssemblyPlates  WRITE setAssemblyPlates  NOTIFY latticeSettingsChanged)
    Q_PROPERTY(int     assembly_shells  READ getAssemblyShells  WRITE setAssemblyShells  NOTIFY latticeSettingsChanged)
    Q_PROPERTY(int     assembly_holes   READ getAssemblyHoles   WRITE setAssemblyHoles   NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  assembly_spread  READ getAssemblySpread  WRITE setAssemblySpread  NOTIFY latticeSettingsChanged)
    Q_PROPERTY(QString assembly_file    READ getAssemblyFile    WRITE setAssemblyFile    NOTIFY latticeSettingsChanged)
    // 3D honeycombs (foam families): walls | windows | edges, window frame width
    // (fraction of the bubble size), bubbles per cell of the random foam, site file.
    Q_PROPERTY(QString foam_mode        READ getFoamMode        WRITE setFoamMode        NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  foam_frame       READ getFoamFrame       WRITE setFoamFrame       NOTIFY latticeSettingsChanged)
    Q_PROPERTY(int     foam_bubbles     READ getFoamBubbles     WRITE setFoamBubbles     NOTIFY latticeSettingsChanged)
    Q_PROPERTY(QString foam_file        READ getFoamFile        WRITE setFoamFile        NOTIFY latticeSettingsChanged)
    // Grown mycelium (mycelium.h): mode, total hypha length per cell, spores, hyphal
    // growth unit, branch angle, tip wiggle, fusion distance, branch taper, and the
    // mode-specific shares (polypore binding hyphae, mat anchors, layered substrate).
    Q_PROPERTY(QString mycelium_mode    READ getMyceliumMode    WRITE setMyceliumMode    NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  mycelium_length  READ getMyceliumLength  WRITE setMyceliumLength  NOTIFY latticeSettingsChanged)
    Q_PROPERTY(int     mycelium_spores  READ getMyceliumSpores  WRITE setMyceliumSpores  NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  mycelium_unit    READ getMyceliumUnit    WRITE setMyceliumUnit    NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  mycelium_angle   READ getMyceliumAngle   WRITE setMyceliumAngle   NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  mycelium_wiggle  READ getMyceliumWiggle  WRITE setMyceliumWiggle  NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  mycelium_fuse    READ getMyceliumFuse    WRITE setMyceliumFuse    NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  mycelium_taper   READ getMyceliumTaper   WRITE setMyceliumTaper   NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  mycelium_binding READ getMyceliumBinding WRITE setMyceliumBinding NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  mycelium_align   READ getMyceliumAlign   WRITE setMyceliumAlign   NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  mycelium_anchors READ getMyceliumAnchors WRITE setMyceliumAnchors NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  mycelium_poor    READ getMyceliumPoor    WRITE setMyceliumPoor    NOTIFY latticeSettingsChanged)
    Q_PROPERTY(QString mycelium_file    READ getMyceliumFile    WRITE setMyceliumFile    NOTIFY latticeSettingsChanged)
    // Bio-inspired / architected forms (cellforms.h): three shape knobs in [0, 1]
    // whose meaning depends on the form (cellforms::knobLabel()).
    Q_PROPERTY(double  form_a           READ getFormA           WRITE setFormA           NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  form_b           READ getFormB           WRITE setFormB           NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  form_c           READ getFormC           WRITE setFormC           NOTIFY latticeSettingsChanged)
    // Material given as numbers instead of a database row (dataset runs): E in
    // GPa (0 = use db_material), Poisson's ratio, and the ratio of the modulus
    // across the print layers (Z) to the in-plane one (1 = isotropic).
    Q_PROPERTY(double  mat_e_gpa  READ getMatEGpa  WRITE setMatEGpa  NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  mat_nu     READ getMatNu    WRITE setMatNu    NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  mat_aniso  READ getMatAniso WRITE setMatAniso NOTIFY latticeSettingsChanged)
    // Write per-voxel stress/strain fields to HDF5 (off for dataset runs: the
    // six load cases of a 64^3 cell are ~40 MB of fields next to <1 MB of
    // matrices and metadata).
    Q_PROPERTY(bool    hdf5_fields READ getHdf5Fields WRITE setHdf5Fields NOTIFY latticeSettingsChanged)
    // Physical size of one lattice cell, mm: converts voxels into millimetres
    // for the printability check and the wall-thickness report.
    Q_PROPERTY(double  lattice_cell_mm READ getLatticeCellMm WRITE setLatticeCellMm NOTIFY latticeSettingsChanged)
    // Sheet families: thicken by the first-order distance |f|/|grad f| instead
    // of |f|, so the wall thickness is (nearly) uniform and printable.
    Q_PROPERTY(bool    lattice_uniform_wall READ getLatticeUniformWall WRITE setLatticeUniformWall NOTIFY latticeSettingsChanged)
    // Printability thresholds (see printability.h) and whether a headless run
    // must stop before the stress solve when the structure fails them.
    Q_PROPERTY(double  print_min_wall_mm              READ getPrintMinWallMm            WRITE setPrintMinWallMm            NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  print_max_overhang_deg         READ getPrintMaxOverhangDeg       WRITE setPrintMaxOverhangDeg       NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  print_max_thin_fraction        READ getPrintMaxThinFraction      WRITE setPrintMaxThinFraction      NOTIFY latticeSettingsChanged)
    Q_PROPERTY(int     print_max_island_vox           READ getPrintMaxIslandVox         WRITE setPrintMaxIslandVox         NOTIFY latticeSettingsChanged)
    Q_PROPERTY(int     print_max_components           READ getPrintMaxComponents        WRITE setPrintMaxComponents        NOTIFY latticeSettingsChanged)
    Q_PROPERTY(int     print_min_directions           READ getPrintMinDirections        WRITE setPrintMinDirections        NOTIFY latticeSettingsChanged)
    Q_PROPERTY(double  print_max_unsupported_fraction READ getPrintMaxUnsupportedFraction WRITE setPrintMaxUnsupportedFraction NOTIFY latticeSettingsChanged)
    Q_PROPERTY(bool    require_printable              READ getRequirePrintable          WRITE setRequirePrintable          NOTIFY latticeSettingsChanged)

    // FFT solver scheme: "auto" (CG for porous or high-contrast cells, basic
    // otherwise), "basic" (Moulinec-Suquet fixed point) or "cg" (conjugate
    // gradients on the Galerkin form). See FFTSolverSession::Scheme.
    Q_PROPERTY(QString fft_scheme READ getFftScheme WRITE setFftScheme NOTIFY fftSchemeChanged)
    Q_PROPERTY(double  fft_tol      READ getFftTol     WRITE setFftTol     NOTIFY fftSchemeChanged)
    Q_PROPERTY(int     fft_max_iter READ getFftMaxIter WRITE setFftMaxIter NOTIFY fftSchemeChanged)

    Q_PROPERTY(QString texture_preset  READ getTexturePreset  WRITE setTexturePreset  NOTIFY textureSettingsChanged)
    Q_PROPERTY(double  texture_scatter READ getTextureScatter WRITE setTextureScatter NOTIFY textureSettingsChanged)

    Q_PROPERTY(float wave_spread          READ getWaveSpread         WRITE setWaveSpread         NOTIFY waveSpreadChanged)
    Q_PROPERTY(float stefan_number        READ getStefanNumber       WRITE setStefanNumber       NOTIFY stefanNumberChanged)
    Q_PROPERTY(int   initial_nuclei_count READ getInitialNucleiCount WRITE setInitialNucleiCount NOTIFY initialNucleiCountChanged)
    Q_PROPERTY(bool   is_wave_generation  READ getIsWaveGeneration   WRITE setIsWaveGeneration   NOTIFY isWaveGenerationChanged)
    Q_PROPERTY(double wave_peak_fraction  READ getWavePeakFraction   WRITE setWavePeakFraction   NOTIFY wavePeakFractionChanged)
    Q_PROPERTY(double wave_end_fraction   READ getWaveEndFraction    WRITE setWaveEndFraction    NOTIFY waveEndFractionChanged)
    Q_PROPERTY(unsigned int num_rnd_loads READ getNumRndLoads        WRITE setNumRndLoads        NOTIFY numRndLoadsChanged)
    Q_PROPERTY(QString polycrystall_neighborhood READ getPolycrystallNeighborhood WRITE setPolycrystallNeighborhood NOTIFY polycrystallNeighborhoodChanged)

public:
    enum SeedMethod { Random = 0, Sobol = 1 };
    Q_ENUM(SeedMethod)

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

    SeedMethod getSeedMethod() const { return seed_method; }
    Q_INVOKABLE void setSeedMethod(SeedMethod value);

    double getSeedSigma() const { return seed_sigma; }
    Q_INVOKABLE void setSeedSigma(double value);

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

    QString getPolycrystallNeighborhood() const { return polycrystall_neighborhood; }
    Q_INVOKABLE void setPolycrystallNeighborhood(const QString& value);

    QString getProbMatrixMode() const { return prob_matrix_mode; }
    Q_INVOKABLE void setProbMatrixMode(const QString& value);

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

    bool getIsThinLayer() const { return is_thin_layer; }
    Q_INVOKABLE void setIsThinLayer(bool value);

    QString getLayerDirection() const { return layer_direction; }
    Q_INVOKABLE void setLayerDirection(const QString& value);

    QString getVoronoiMetricPreset() const { return voronoi_metric_preset; }
    Q_INVOKABLE void setVoronoiMetricPreset(const QString& value);

    double getVoronoiMxx() const { return voronoi_mxx; }
    Q_INVOKABLE void setVoronoiMxx(double value);

    double getVoronoiMyy() const { return voronoi_myy; }
    Q_INVOKABLE void setVoronoiMyy(double value);

    double getVoronoiMzz() const { return voronoi_mzz; }
    Q_INVOKABLE void setVoronoiMzz(double value);

    double getVoronoiMxy() const { return voronoi_mxy; }
    Q_INVOKABLE void setVoronoiMxy(double value);

    double getVoronoiMyz() const { return voronoi_myz; }
    Q_INVOKABLE void setVoronoiMyz(double value);

    double getVoronoiMxz() const { return voronoi_mxz; }
    Q_INVOKABLE void setVoronoiMxz(double value);

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

    // ── Lattice ───────────────────────────────────────────────────────────
    QString getLatticeFamily()   const { return latticeFamilyLabelOf(lattice_family); }
    int     getCellsPerEdge()    const { return cells_per_edge; }
    double  getRelativeDensity() const { return relative_density; }
    double  getLatticeGrading()  const { return lattice_grading; }
    double  getLatticeJitter()   const { return lattice_jitter; }

    /// Accepts the CLI key, the panel label or a loose spelling of either.
    Q_INVOKABLE void setLatticeFamily(const QString& value);
    Q_INVOKABLE void setCellsPerEdge(int value);
    Q_INVOKABLE void setRelativeDensity(double value);
    Q_INVOKABLE void setLatticeGrading(double value);
    Q_INVOKABLE void setLatticeJitter(double value);

    double  getSpinodoidThetaX() const { return spinodoid_theta_x; }
    double  getSpinodoidThetaY() const { return spinodoid_theta_y; }
    double  getSpinodoidThetaZ() const { return spinodoid_theta_z; }
    int     getFourierTerms()    const { return fourier_terms; }
    double  getFourierKmin()     const { return fourier_kmin; }
    double  getFourierKmax()     const { return fourier_kmax; }
    double  getFourierDecay()    const { return fourier_decay; }
    QString getFourierFile()     const { return fourier_file; }
    Q_INVOKABLE void setSpinodoidThetaX(double value);
    Q_INVOKABLE void setSpinodoidThetaY(double value);
    Q_INVOKABLE void setSpinodoidThetaZ(double value);
    Q_INVOKABLE void setFourierTerms(int value);
    Q_INVOKABLE void setFourierKmin(double value);
    Q_INVOKABLE void setFourierKmax(double value);
    Q_INVOKABLE void setFourierDecay(double value);
    Q_INVOKABLE void setFourierFile(const QString& value);
    double  getNoiseSmooth()     const { return noise_smooth; }
    Q_INVOKABLE void setNoiseSmooth(double value);
    QString getFourierSymmetry() const { return fourier_symmetry; }
    Q_INVOKABLE void setFourierSymmetry(const QString& value);
    double  getPlateHole()       const { return plate_hole; }
    Q_INVOKABLE void setPlateHole(double value);
    QString getLatticeUnion()    const { return lattice_union.isEmpty() ? QStringLiteral("none") : latticeFamilyLabelOf(lattice_union); }
    Q_INVOKABLE void setLatticeUnion(const QString& value);
    static QString latticeUnionKeyStored() { return lattice_union; }
    QString getLatticeSubtract() const { return lattice_subtract.isEmpty() ? QStringLiteral("none") : latticeFamilyLabelOf(lattice_subtract); }
    Q_INVOKABLE void setLatticeSubtract(const QString& value);
    static QString latticeSubtractKeyStored() { return lattice_subtract; }
    double  getLatticeSubtractSize() const { return lattice_subtract_size; }
    Q_INVOKABLE void setLatticeSubtractSize(double value);
    double  getLatticeSmooth()       const { return lattice_smooth; }
    Q_INVOKABLE void setLatticeSmooth(double value);
    bool    getHoneycombCustom()     const { return honeycomb_custom; }
    Q_INVOKABLE void setHoneycombCustom(bool value);
    double  getHoneycombAngle()      const { return honeycomb_angle; }
    Q_INVOKABLE void setHoneycombAngle(double value);
    double  getHoneycombRib()        const { return honeycomb_rib; }
    Q_INVOKABLE void setHoneycombRib(double value);
    int     getAssemblyStruts()      const { return assembly_struts; }
    Q_INVOKABLE void setAssemblyStruts(int value);
    int     getAssemblyPlates()      const { return assembly_plates; }
    Q_INVOKABLE void setAssemblyPlates(int value);
    int     getAssemblyShells()      const { return assembly_shells; }
    Q_INVOKABLE void setAssemblyShells(int value);
    int     getAssemblyHoles()       const { return assembly_holes; }
    Q_INVOKABLE void setAssemblyHoles(int value);
    double  getAssemblySpread()      const { return assembly_spread; }
    Q_INVOKABLE void setAssemblySpread(double value);
    QString getAssemblyFile()        const { return assembly_file; }
    Q_INVOKABLE void setAssemblyFile(const QString& value);
    QString getFoamMode()            const { return foam_mode; }
    Q_INVOKABLE void setFoamMode(const QString& value);
    double  getFoamFrame()           const { return foam_frame; }
    Q_INVOKABLE void setFoamFrame(double value);
    int     getFoamBubbles()         const { return foam_bubbles; }
    Q_INVOKABLE void setFoamBubbles(int value);
    QString getFoamFile()            const { return foam_file; }
    Q_INVOKABLE void setFoamFile(const QString& value);
    QString getMyceliumMode()        const { return mycelium_mode; }
    Q_INVOKABLE void setMyceliumMode(const QString& value);
    double  getMyceliumLength()      const { return mycelium_length; }
    Q_INVOKABLE void setMyceliumLength(double value);
    int     getMyceliumSpores()      const { return mycelium_spores; }
    Q_INVOKABLE void setMyceliumSpores(int value);
    double  getMyceliumUnit()        const { return mycelium_unit; }
    Q_INVOKABLE void setMyceliumUnit(double value);
    double  getMyceliumAngle()       const { return mycelium_angle; }
    Q_INVOKABLE void setMyceliumAngle(double value);
    double  getMyceliumWiggle()      const { return mycelium_wiggle; }
    Q_INVOKABLE void setMyceliumWiggle(double value);
    double  getMyceliumFuse()        const { return mycelium_fuse; }
    Q_INVOKABLE void setMyceliumFuse(double value);
    double  getMyceliumTaper()       const { return mycelium_taper; }
    Q_INVOKABLE void setMyceliumTaper(double value);
    double  getMyceliumBinding()     const { return mycelium_binding; }
    Q_INVOKABLE void setMyceliumBinding(double value);
    double  getMyceliumAlign()       const { return mycelium_align; }
    Q_INVOKABLE void setMyceliumAlign(double value);
    double  getMyceliumAnchors()     const { return mycelium_anchors; }
    Q_INVOKABLE void setMyceliumAnchors(double value);
    double  getMyceliumPoor()        const { return mycelium_poor; }
    Q_INVOKABLE void setMyceliumPoor(double value);
    QString getMyceliumFile()        const { return mycelium_file; }
    Q_INVOKABLE void setMyceliumFile(const QString& value);
    double  getFormA()               const { return form_a; }
    double  getFormB()               const { return form_b; }
    double  getFormC()               const { return form_c; }
    Q_INVOKABLE void setFormA(double value);
    Q_INVOKABLE void setFormB(double value);
    Q_INVOKABLE void setFormC(double value);
    double getMatEGpa()    const { return mat_e_gpa; }
    double getMatNu()      const { return mat_nu; }
    double getMatAniso()   const { return mat_aniso; }
    bool   getHdf5Fields() const { return hdf5_fields; }
    Q_INVOKABLE void setMatEGpa(double value);
    Q_INVOKABLE void setMatNu(double value);
    Q_INVOKABLE void setMatAniso(double value);
    Q_INVOKABLE void setHdf5Fields(bool value);
    /// True when the material comes from mat_e_gpa / mat_nu / mat_aniso rather than the database.
    static bool materialOverrideActive() { return mat_e_gpa > 0.0; }
    /// Voigt 6x6 stiffness in Pa of the override material: isotropic when
    /// mat_aniso == 1, otherwise transversely isotropic about axis 3 (the build
    /// direction) with E3 = aniso*E, G13 = G23 = aniso*G and the in-plane
    /// Poisson's ratio carried over. Returns false if the constants are not
    /// positive definite.
    static bool overrideStiffnessPa(double C[6][6]);
    /// The same law from explicit numbers (E in GPa): what overrideStiffnessPa
    /// builds from mat_e_gpa / mat_nu / mat_aniso.
    static bool lawStiffnessPa(double eGpa, double nu, double aniso, double C[6][6]);
    /// Publish the solid a Lattice cell file was solved with (`parameters` =
    /// its geometry parameters: mat_e_gpa / mat_nu / mat_aniso / mat_source)
    /// as phaseAssignment, so a stress run on an opened cell uses it; false
    /// when the file carries none (other algorithms, old files).
    static bool publishCellMaterial(const QVariantMap& parameters, const QString& algorithm, const QString& logLine = QString());
    static QString overrideMaterialName();

    double getLatticeCellMm()               const { return lattice_cell_mm; }
    bool   getLatticeUniformWall()          const { return lattice_uniform_wall; }
    Q_INVOKABLE void setLatticeUniformWall(bool value);
    double getPrintMinWallMm()              const { return print_min_wall_mm; }
    double getPrintMaxOverhangDeg()         const { return print_max_overhang_deg; }
    double getPrintMaxThinFraction()        const { return print_max_thin_fraction; }
    int    getPrintMaxIslandVox()           const { return print_max_island_vox; }
    int    getPrintMaxComponents()          const { return print_max_components; }
    int    getPrintMinDirections()          const { return print_min_directions; }
    double getPrintMaxUnsupportedFraction() const { return print_max_unsupported_fraction; }
    bool   getRequirePrintable()            const { return require_printable; }
    bool   getLatticeRemoveIslands()        const { return lattice_remove_islands; }
    double getLatticeMetricY()              const { return lattice_metric_y; }
    double getLatticeMetricZ()              const { return lattice_metric_z; }
    Q_INVOKABLE void setLatticeCellMm(double value);
    Q_INVOKABLE void setPrintMinWallMm(double value);
    Q_INVOKABLE void setPrintMaxOverhangDeg(double value);
    Q_INVOKABLE void setPrintMaxThinFraction(double value);
    Q_INVOKABLE void setPrintMaxIslandVox(int value);
    Q_INVOKABLE void setPrintMaxComponents(int value);
    Q_INVOKABLE void setPrintMinDirections(int value);
    Q_INVOKABLE void setPrintMaxUnsupportedFraction(double value);
    Q_INVOKABLE void setRequirePrintable(bool value);
    /// Lattice: remove every floating island (any size) from the rasterized cell before the printability check.
    Q_INVOKABLE void setLatticeRemoveIslands(bool value);
    /// Lattice thickness metric (1 = off): a distance along Y / Z counts this many times (lattice.h). 0.25..4.
    Q_INVOKABLE void setLatticeMetricY(double value);
    Q_INVOKABLE void setLatticeMetricZ(double value);
    /// Thresholds for checkPrintability(), built from the fields above.
    static PrintabilityThresholds printThresholds(double voxel_mm);

    /// The stored CLI key of the current family ("gyroid_sheet", ...).
    static QString latticeFamilyKeyStored() { return lattice_family; }
    static QString latticeFamilyLabelOf(const QString& key);

    QString getFftScheme() const { return fft_scheme; }
    /// Accepts auto | basic | cg (case-insensitive); anything else is ignored with a warning.
    Q_INVOKABLE void setFftScheme(const QString& value);
    double getFftTol()     const { return fft_tol; }
    int    getFftMaxIter() const { return fft_max_iter; }
    Q_INVOKABLE void setFftTol(double value);
    double getFftVoidEta() const { return fft_void_eta; }
    Q_INVOKABLE void setFftVoidEta(double value);
    QString getFftDiscretization() const { return fft_discretization; }
    Q_INVOKABLE void setFftDiscretization(const QString& value);
    Q_INVOKABLE void setFftMaxIter(int value);

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
    bool   getIsWaveGeneration()  const { return is_wave_generation; }
    double getWavePeakFraction()  const { return wave_peak_fraction; }
    double getWaveEndFraction()   const { return wave_end_fraction; }
    unsigned int getNumRndLoads() const { return num_rnd_loads; }

    Q_INVOKABLE void setWaveSpread(float value);
    Q_INVOKABLE void setStefanNumber(float value);
    Q_INVOKABLE void setInitialNucleiCount(int value);
    Q_INVOKABLE void setIsWaveGeneration(bool value);
    Q_INVOKABLE void setWavePeakFraction(double value);
    Q_INVOKABLE void setWaveEndFraction(double value);
    Q_INVOKABLE void setNumRndLoads(unsigned int value);

    static void resetDefaults();

    static Parameters* m_instance;

    static int32_t*** voxels;

    /// The grid above is in use by something that keeps its pointer while the
    /// event loop runs: a structure generation (its own array is the grid, and it
    /// lets queued events run to redraw the 3D view - once before filling, after
    /// every step of an animation) and a stress solve (a worker thread reads it).
    /// Meanwhile nothing may free or replace the grid: a cell opened from a file
    /// or shown by the Neural tab did, and the generator or solver went on in
    /// freed memory (SIGSEGV in Lattice::rasterize / OpenGLWidgetQML::setVoxels).
    /// Main thread only. acquireVoxels() / releaseVoxels() count the users;
    /// whenVoxelsFree() runs fn at once when there is none, else when the last one
    /// is done (later in the event loop, and only while ctx still exists).
    static void acquireVoxels();
    static void releaseVoxels();
    static bool voxelsBusy() { return voxelUsers > 0; }
    static void whenVoxelsFree(QObject* ctx, std::function<void()> fn);
    /// acquireVoxels() for the lifetime of a scope.
    struct VoxelUse {
        VoxelUse() { acquireVoxels(); }
        ~VoxelUse() { releaseVoxels(); }
        VoxelUse(const VoxelUse&) = delete;
        VoxelUse& operator=(const VoxelUse&) = delete;
    };
    static int voxelUsers;

    static unsigned int seed;
    static SeedMethod seed_method;
    static double seed_sigma;
    static QString filename;
    static int num_threads;
    static QString working_directory;
    static QString csv_output;
    static float wave_coefficient;
    static float wave_spread;
    static int initial_nuclei_count;
    static bool is_wave_generation;
    static double wave_peak_fraction;
    static double wave_end_fraction;
    static QString prob_preset;
    static QString polycrystall_neighborhood;
    static QString prob_matrix_mode;
    static QString local_criterion;   // "auto" | "schmid" | "vonmises"
    static QString hill_space;        // "auto" | "deviatoric5d" | "full6d"
    static double  yield_stress;      // Pa, von Mises threshold
    static double  yield_percentile;  // (0,1], or 0 = "auto" (per algorithm)
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

    // Published by the Lattice algorithm after rasterization, for the HDF5
    // geometry metadata: the solid fraction actually reached and the
    // estimated wall / strut thickness in voxels (0 = no estimate).
    static double lattice_achieved_density;
    static double lattice_wall_voxels;
    // The waves a spinodoid or Fourier-series cell was built from, one row
    // per wave: kx, ky, kz, amplitude, phase. Empty for the other families.
    static std::vector<std::array<float,5>> lattice_wave_terms;
    // The members of a cubic-assembly cell, one row per member: kind
    // (0 strut, 1 plate, 2 shell, 3 window), three points (x y z each, in the
    // asymmetric unit 0 <= z <= y <= x <= 1/2), radius, thickness weight.
    static std::vector<std::array<float,12>> lattice_assembly_terms;
    // The sites of a foam cell (x y z in cell units, one row per bubble):
    // its Voronoi partition is the cell. Empty for the other families.
    static std::vector<std::array<float,3>> lattice_foam_sites;
    // The hyphae of a mycelium cell, one row per straight piece: a (x y z in
    // [0, 1) cell units), b = a + the piece's vector, thickness weight. Its
    // growth statistics alongside. Empty for the other families.
    static std::vector<std::array<float,7>> lattice_mycelium_segments;
    static mycelium::Stats lattice_mycelium_stats;
    // Result of the last printability check (Lattice runs it after rasterization).
    static PrintabilityReport printability;
    // Voxel edge in mm of the last Lattice run (cells * cell_mm / size).
    static double lattice_voxel_mm;
    // Floating islands removed from the last Lattice cell (lattice_remove_islands) and their voxels.
    static int lattice_islands_removed;
    static long long lattice_island_voxels_removed;

    // "auto" | "basic" | "cg"; both FFT entry points read it at construction.
    static QString fft_scheme;
    // Convergence tolerance and iteration cap of the FFT solver (both schemes).
    static double  fft_tol;
    static int     fft_max_iter;
    static double  fft_void_eta;      // stiffness of the void phase as a fraction of the solid's C11
    static QString fft_discretization; // "continuous" (Moulinec-Suquet) | "rotated" (Willot)

    /**
     * @brief Full anisotropic stiffness of a named database material, in Pa.
     *
     * The generalisation of cubicConstantsPa(): reads all 21 columns, so
     * transversely isotropic rows (carbon fiber) survive instead of being
     * flattened onto a cubic triple. Returns false and leaves C untouched when
     * the material is unknown or has no usable constants.
     */
    static bool materialStiffnessPa(const QString& name, double C[6][6]);

    /**
     * @brief Cached full 6x6 stiffness of the selected material (mat_C), in Pa.
     *        Thread-safe for solver worker threads: no QSqlDatabase access.
     */
    static void stiffnessPa(double C[6][6]);

    /**
     * @brief Resolves "auto" local yield criterion / Hill fit space from the
     *        generation algorithm name (aliases accepted).
     *
     * criterion and space are in/out: an explicit (non-"auto") value is kept,
     * "auto" is replaced by the per-algorithm default. criterionAuto/spaceAuto
     * report whether the value came from the auto table.
     */
    static void resolveYieldSettings(const QString& algorithm, QString& criterion, QString& space,
                                     bool* criterionAuto = nullptr, bool* spaceAuto = nullptr);

    /**
     * @brief Resolve yield_percentile: an explicit value in (0,1] is kept, 0
     *        ("auto") is replaced by the per-algorithm default (aliases accepted).
     *        isAuto reports whether the value came from the auto table.
     */
    static double resolveYieldPercentile(const QString& algorithm, double percentile, bool* isAuto = nullptr);

signals:
    void sizeChanged();
    void pointsChanged();
    void algorithmChanged();
    void seedChanged();
    void seedMethodChanged();
    void seedSigmaChanged();
    void filenameChanged();
    void numThreadsChanged();
    void workingDirectoryChanged();
    void waveCoefficientChanged();

    void probPresetChanged();
    void polycrystallNeighborhoodChanged();
    void probMatrixModeChanged();
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
    void isWaveGenerationChanged();
    void wavePeakFractionChanged();
    void waveEndFractionChanged();

    void minkowskiPChanged();
    void isPeriodicChanged();
    void isThinLayerChanged();
    void layerDirectionChanged();
    void voronoiMetricPresetChanged();
    void voronoiMetricChanged();
    void dbMaterialChanged();
    void textureSettingsChanged();
    void compositeSettingsChanged();
    void latticeSettingsChanged();
    void fftSchemeChanged();

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
    static bool    is_thin_layer;
    static QString layer_direction;
    static QString normalizeLayerDirection(const QString& value);
    static QString voronoi_metric_preset;
    static double  voronoi_mxx;
    static double  voronoi_myy;
    static double  voronoi_mzz;
    static double  voronoi_mxy;
    static double  voronoi_myz;
    static double  voronoi_mxz;

    static QString db_material;
    // Cubic constants of db_material in GPa, the unit material_properties.db
    // stores; cubicConstantsPa() converts. Defaults are the Cu values the
    // solvers used to hardcode, so an unset material changes nothing.
    static double  mat_c11, mat_c12, mat_c44;
    // Full 6x6 stiffness of db_material in GPa (Voigt), cached by setDbMaterial().
    static double  mat_C[6][6];
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

    static QString lattice_family;       // CLI key, see latticefamily.h
    static int     cells_per_edge;
    static double  relative_density;
    static double  lattice_grading;
    static double  lattice_jitter;
    static double  spinodoid_theta_x;
    static double  spinodoid_theta_y;
    static double  spinodoid_theta_z;
    static int     fourier_terms;
    static double  fourier_kmin;
    static double  fourier_kmax;
    static double  fourier_decay;
    static QString fourier_file;
    static double  noise_smooth;
    static QString fourier_symmetry;     // "none" | "cubic"
    static double  plate_hole;
    static QString lattice_union;        // CLI key of the second family, empty = none
    static QString lattice_subtract;     // CLI key of the carved-out family, empty = none
    static double  lattice_subtract_size;
    static double  lattice_smooth;
    static bool    honeycomb_custom;
    static double  honeycomb_angle;
    static double  honeycomb_rib;
    static int     assembly_struts;
    static int     assembly_plates;
    static int     assembly_shells;
    static int     assembly_holes;
    static double  assembly_spread;
    static QString assembly_file;
    static QString foam_mode;            // "walls" | "windows" | "edges"
    static double  foam_frame;
    static int     foam_bubbles;
    static QString foam_file;
    static QString mycelium_mode;        // "substrate" | "polypore" | "mat" | "layered"
    static double  mycelium_length;
    static int     mycelium_spores;
    static double  mycelium_unit;
    static double  mycelium_angle;
    static double  mycelium_wiggle;
    static double  mycelium_fuse;
    static double  mycelium_taper;
    static double  mycelium_binding;
    static double  mycelium_align;
    static double  mycelium_anchors;
    static double  mycelium_poor;
    static QString mycelium_file;
    static double  form_a;
    static double  form_b;
    static double  form_c;
    static double  mat_e_gpa;
    static double  mat_nu;
    static double  mat_aniso;
    static bool    hdf5_fields;
    static double  lattice_cell_mm;
    static bool    lattice_uniform_wall;
    static double  print_min_wall_mm;
    static double  print_max_overhang_deg;
    static double  print_max_thin_fraction;
    static int     print_max_island_vox;
    static int     print_max_components;
    static int     print_min_directions;
    static double  print_max_unsupported_fraction;
    static bool    require_printable;
    static bool    lattice_remove_islands;
    static double  lattice_metric_y;
    static double  lattice_metric_z;
};

#endif // PARAMETERS_H

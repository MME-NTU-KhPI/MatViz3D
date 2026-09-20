#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>
#include <omp.h>

#include <cmath>
#include <ctime>
#include "algorithmfactory.h"
#include "cpuinfo.hpp"
#include "commandline_parser.h"
#include "config_source.h"
#include "dbmanager.h"
#include "parameters.h"
#include "texturelibrary.h"

Commandline_Parser::Commandline_Parser()
{

}

QString Commandline_Parser::buildApplicationDescription()
{
    QString desc;
    desc += "MatViz3D - Cellular Automata 3D Microstructure Generator & Homogenization Analyzer\n\n";

    desc += "OVERVIEW:\n";
    desc += "  MatViz3D synthesizes 3D cellular microstructures on a regular voxel grid,\n";
    desc += "  assigns crystallographic textures (Euler orientation angles) or multi-phase\n";
    desc += "  constituent stiffnesses, and computes effective elastic response and full field\n";
    desc += "  distributions via an in-memory FFT solver (Moulinec-Suquet) or external ANSYS FEM.\n\n";

    desc += "EXECUTION MODES:\n";
    desc += "  * Interactive GUI Mode (default):\n";
    desc += "      Launch without --nogui to run the Qt/QML graphical desktop interface.\n";
    desc += "  * Headless Batch Mode (for scripts and AI agents):\n";
    desc += "      Pass BOTH --nogui and --autostart to run non-interactively and exit:\n";
    desc += "        MatViz3D --nogui --autostart [options]\n";
    desc += "      To also compute stress/strain homogenization after generation, add --run_stress_calc:\n";
    desc += "        MatViz3D --nogui --autostart --run_stress_calc [options]\n\n";

    desc += "WORKFLOW RECIPES FOR AGENTS & SCRIPTS:\n";
    desc += "  1. Headless Voronoi Polycrystal (periodic 30^3, 50 grains, copper):\n";
    desc += "       MatViz3D --nogui --autostart --algorithm Voronoi --size 30 --points 50 --periodic --material Cu --output voronoi.hdf5\n\n";
    desc += "  2. Composite Fiber RVE (2D orthogonal, hexagonal packing, 40% Vf):\n";
    desc += "       MatViz3D --nogui --autostart --algorithm Composite --size 40 --composite_dim 2d --composite_packing hexagonal --fiber_volume_fraction 0.4 --matrix_material Epoxy --fiber_material C-fiber --output composite.hdf5\n\n";
    desc += "  3. Fast In-Memory FFT Stiffness Tensor (Phase 1, 6 unit strains -> S, C, moduli):\n";
    desc += "       MatViz3D --nogui --autostart --algorithm Voronoi --size 20 --points 30 --material Al --run_stress_calc --solver fft --stress_mode stiffness --output stiffness.hdf5\n\n";
    desc += "  4. Prescribed Strain Single-Load Analysis:\n";
    desc += "       MatViz3D --nogui --autostart --algorithm Voronoi --size 20 --points 20 --run_stress_calc --solver fft --stress_mode single --eps 0.001,0,0,0,0,0 --output single.hdf5\n\n";

    desc += "AVAILABLE GENERATION ALGORITHMS (--algorithm <name>):\n";
    const QStringList algos = AlgorithmFactory::instance().algorithmNames();
    for (const QString& name : algos) {
        const AlgorithmPlugin* plugin = AlgorithmFactory::instance().pluginFor(name);
        QString summary = plugin ? plugin->description : QString();
        if (summary.isEmpty()) {
            desc += QString("  - %1\n").arg(name, -13);
        } else {
            int dotIdx = summary.indexOf('.');
            QString shortSummary = (dotIdx > 0 && dotIdx < 100) ? summary.left(dotIdx + 1) : summary;
            desc += QString("  - %1 : %2\n").arg(name, -13).arg(shortSummary);
        }
    }
    desc += "\n";

    desc += "KEY PRESETS & ENUM VALUES:\n";
    desc += "  --algorithm:             Voronoi | Composite | Probability | Polycrystall | DLCA\n";
    desc += "  --neighborhood:          'Moore (26)' | 'von Neumann (6)' | 'Radial (18)'\n";
    desc += "  --solver:                fft (in-memory Moulinec-Suquet) | ansys (external APDL FEM)\n";
    desc += "  --stress_mode:           stiffness (fast 6 solves -> S, C, moduli) | single (--eps required) | dataset (full 300 loads)\n";
    desc += "  --composite_dim:         1d (fibers along Z) | 2d (along X, Y) | 3d (along X, Y, Z)\n";
    desc += "  --composite_packing:     square | hexagonal\n";
    desc += "  --prob_matrix_mode:      volume ('Volume Sampling') | surface ('Surface Flux')\n";
    desc += "  --texture:               random | extrusion | rolling | recrystallization | shear | scattered_cube\n";
    desc += "  --lattice:               fcc | bcc\n";
    desc += "  --prob_preset:           'Sphere (Circle)', 'Prolate (Needle)', 'Oblate (Disc)', 'Triaxial Ellipsoid', 'Superellipsoid (Cube)', 'Custom'\n";
    desc += "  --voronoi_metric_preset: 'Sphere (Circle)', 'Prolate (Needle)', 'Oblate (Disc)', 'Triaxial Ellipsoid', 'Superellipsoid (Cube)',\n";
    desc += "                           'Columnar (Z-axis)', 'Columnar (X-axis)', 'Rolled (Orthotropic)', 'Sheared (45 deg XY)', 'Custom'\n\n";

    desc += "CONFIGURATION FILES (--config <file>):\n";
    desc += "  Load run parameters from a configuration file (JSON supported; YAML stubbed).\n";
    desc += "  Load order: configuration file parameters are loaded first, and explicit command-line\n";
    desc += "  options are applied on top, overriding file values.\n\n";

    desc += "MACHINE-READABLE SCHEMA:\n";
    desc += "  Pass --help-json to output full CLI schema, algorithms, materials, and options in JSON format.";

    return desc;
}

void Commandline_Parser::setupParser(QCommandLineParser &parser)
{
    parser.setApplicationDescription(buildApplicationDescription());
    parser.addHelpOption();
    parser.addVersionOption();

    // ── Configuration File Import ─────────────────────────────────────────
    parser.addOption(QCommandLineOption(QStringList() << "config" << "c",
        "[Config] Load run parameters from configuration file (JSON supported; explicit CLI options override file values).", "file"));

    // ── Execution & Headless Control ──────────────────────────────────────
    parser.addOption(QCommandLineOption("nogui",
        "[Mode] Run in headless batch mode without GUI (requires --autostart to execute)."));
    parser.addOption(QCommandLineOption(QStringList() << "nologo" << "no-logo",
        "[Mode] Suppress printing the ASCII logo banner on startup."));
    parser.addOption(QCommandLineOption("autostart",
        "[Mode] Automatically start structure generation on launch (required for headless mode)."));
    parser.addOption(QCommandLineOption("animate",
        "[Mode] Grow structure iteration by iteration instead of one-shot generation."));
    parser.addOption(QCommandLineOption("np",
        "[System] Number of OpenMP worker threads (default: physical CPU cores).", "threads"));
    parser.addOption(QCommandLineOption("seed",
        "[System] RNG seed for reproducibility (default: current timestamp).", "uint"));
    parser.addOption(QCommandLineOption("output",
        "[Output] Output HDF5 file path for structure and fields (default: current_ls.hdf5).", "file"));

    // ── Structure Geometry & Nucleation ───────────────────────────────────
    parser.addOption(QCommandLineOption("size",
        "[Grid] Voxel grid dimension N for N x N x N cell (integer > 0, e.g. 30).", "n"));
    parser.addOption(QCommandLineOption("points",
        "[Grid] Number of initial nucleation seeds / grains (integer > 0, e.g. 50).", "count"));
    parser.addOption(QCommandLineOption("concentration",
        "[Grid] Nucleation seed density as volume percentage in (0, 100]; overrides --points (e.g. 0.5).", "pct"));
    parser.addOption(QCommandLineOption("algorithm",
        "[Grid] Generation algorithm name (default: Voronoi). See registered list above.", "name"));
    parser.addOption(QCommandLineOption("periodic",
        "[Grid] Enable periodic boundary conditions (grains/fibers wrap across opposite cell faces)."));
    parser.addOption(QCommandLineOption(QStringList() << "neighborhood" << "polycrystall_neighborhood",
        "[Polycrystall] Neighborhood stencil: 'Moore (26)' | 'von Neumann (6)' | 'Radial (18)' (aliases: Moore, Neumann, Radial). Default: 'Moore (26)'", "stencil"));
    parser.addOption(QCommandLineOption(QStringList() << "thin-layer" << "thin_layer",
        "[Polycrystall] Generate all initial grain seeds on the same plane (thin layer mode)."));
    parser.addOption(QCommandLineOption(QStringList() << "layer-direction" << "layer_direction",
        "[Polycrystall] Starting direction of the thin layer film: '+Z' | '-Z' | '+X' | '-X' | '+Y' | '-Y' (default: '+Z').", "dir"));

    // ── Voronoi Tessellation & Riemannian Metric ──────────────────────────
    parser.addOption(QCommandLineOption("minkowski_p",
        "[Voronoi] Minkowski exponent p: 1 = Manhattan (octahedral), 2 = Euclidean, large = Chebyshev (cuboidal). Default: 2.0", "p"));
    parser.addOption(QCommandLineOption("voronoi_metric_preset",
        "[Voronoi] Preset for shape / metric tensor ('Sphere (Circle)', 'Prolate (Needle)', 'Oblate (Disc)', 'Triaxial Ellipsoid', 'Superellipsoid (Cube)', 'Columnar (Z-axis)', 'Columnar (X-axis)', 'Rolled (Orthotropic)', 'Sheared (45 deg XY)', 'Custom').", "preset"));
    parser.addOption(QCommandLineOption("voronoi_metric",
        "[Voronoi] Metric tensor components as comma-separated list: 'mxx,myy,mzz' or 'mxx,myy,mzz,mxy,myz,mxz'.", "components"));
    parser.addOption(QCommandLineOption("voronoi_mxx",
        "[Voronoi] Metric tensor diagonal M_xx (M11) scaling X-elongation (default: 1.0).", "val"));
    parser.addOption(QCommandLineOption("voronoi_myy",
        "[Voronoi] Metric tensor diagonal M_yy (M22) scaling Y-elongation (default: 1.0).", "val"));
    parser.addOption(QCommandLineOption("voronoi_mzz",
        "[Voronoi] Metric tensor diagonal M_zz (M33) scaling Z-elongation (default: 1.0).", "val"));
    parser.addOption(QCommandLineOption("voronoi_mxy",
        "[Voronoi] Metric tensor off-diagonal M_xy (M12) XY shear coupling (default: 0.0).", "val"));
    parser.addOption(QCommandLineOption("voronoi_myz",
        "[Voronoi] Metric tensor off-diagonal M_yz (M23) YZ shear coupling (default: 0.0).", "val"));
    parser.addOption(QCommandLineOption("voronoi_mxz",
        "[Voronoi] Metric tensor off-diagonal M_xz (M13) XZ shear coupling (default: 0.0).", "val"));

    // ── Composite (Fiber-Reinforced RVE) ──────────────────────────────────
    parser.addOption(QCommandLineOption("composite_dim",
        "[Composite] Reinforcement families: 1d (along Z) | 2d (along X,Y) | 3d (along X,Y,Z). Default: 1d", "dim"));
    parser.addOption(QCommandLineOption("composite_packing",
        "[Composite] Fiber cross-section packing lattice: square | hexagonal. Default: square", "packing"));
    parser.addOption(QCommandLineOption("fiber_volume_fraction",
        "[Composite] Target fiber volume fraction in range (0, 1). Solved via bisection. Default: 0.4", "frac"));
    parser.addOption(QCommandLineOption("fibers_per_row",
        "[Composite] Number of fibers per lattice row in cross-section (default: 3).", "n"));
    parser.addOption(QCommandLineOption("fiber_aspect_ratio",
        "[Composite] Fiber elliptical cross-section a/b (major/minor semi-axis; 1 = circular). Default: 1.0", "ratio"));
    parser.addOption(QCommandLineOption("fiber_angle_scatter",
        "[Composite] Per-fiber in-plane rotation spread in degrees [0, 180] (default: 0.0).", "deg"));
    parser.addOption(QCommandLineOption("fiber_center_jitter",
        "[Composite] Random shift of fiber centers in [0, 1] of half-pitches (0 = perfect lattice). Default: 0.0", "val"));
    parser.addOption(QCommandLineOption("fiber_allow_overlap",
        "[Composite] Let jittered fibers overlap and merge; lifts packing limit on volume fraction."));
    parser.addOption(QCommandLineOption("matrix_material",
        "[Composite] Matrix constituent from material_properties.db (e.g. Epoxy, Al, Cu). Default: Epoxy", "name"));
    parser.addOption(QCommandLineOption("fiber_material",
        "[Composite] Fiber constituent from material_properties.db (e.g. C-fiber, E-glass, SiC, W). Default: C-fiber", "name"));

    // ── Probability Cellular Automaton ────────────────────────────────────
    parser.addOption(QCommandLineOption("prob_preset",
        "[Probability] Shape preset: 'Sphere (Circle)', 'Prolate (Needle)', 'Oblate (Disc)', 'Triaxial Ellipsoid', 'Superellipsoid (Cube)', 'Custom'.", "preset"));
    parser.addOption(QCommandLineOption("prob_matrix_mode",
        "[Probability] Transition matrix calculation: volume ('Volume Sampling') | surface ('Surface Flux'). Default: volume", "mode"));
    parser.addOption(QCommandLineOption("halfaxis_a",
        "[Probability] Kernel semi-axis A length along X.", "val"));
    parser.addOption(QCommandLineOption("halfaxis_b",
        "[Probability] Kernel semi-axis B length along Y.", "val"));
    parser.addOption(QCommandLineOption("halfaxis_c",
        "[Probability] Kernel semi-axis C length along Z.", "val"));
    parser.addOption(QCommandLineOption("orientation_angle_a",
        "[Probability] Kernel Euler rotation angle about X-axis in degrees.", "deg"));
    parser.addOption(QCommandLineOption("orientation_angle_b",
        "[Probability] Kernel Euler rotation angle about Y-axis in degrees.", "deg"));
    parser.addOption(QCommandLineOption("orientation_angle_c",
        "[Probability] Kernel Euler rotation angle about Z-axis in degrees.", "deg"));
    parser.addOption(QCommandLineOption("ellipse_order",
        "[Probability] Superellipse equation degree |x/a|^p + |y/b|^p + |z/c|^p <= 1 (default: 2.0).", "p"));
    parser.addOption(QCommandLineOption("stefan_number",
        "[Probability] Thermodynamic Stefan number (cooling limit) for probabilistic growth.", "val"));
    parser.addOption(QCommandLineOption("wave_generation",
        "[Probability] Enable continuous wave nucleation (transformation-fraction controlled)."));
    parser.addOption(QCommandLineOption("initial_nuclei",
        "[Probability] Number of initial nuclei present at step 0 for wave nucleation.", "count"));
    parser.addOption(QCommandLineOption("wave_peak_fraction",
        "[Probability] Solid volume fraction where nucleation rate peaks in [0, 1] (default: 0.20).", "frac"));
    parser.addOption(QCommandLineOption("wave_end_fraction",
        "[Probability] Solid volume fraction where 100% of nuclei are placed in [0, 1] (default: 0.60).", "frac"));
    parser.addOption(QCommandLineOption("wave_coefficient",
        "[Probability] Rate coefficient for wave nucleation kinetics (default: 0.1).", "val"));

    // ── Material & Crystallographic Texture ───────────────────────────────
    parser.addOption(QCommandLineOption("material",
        "[Material] Material from material_properties.db (e.g. Cu, Fe, Al, W). Supplies cubic constants and default lattice.", "name"));
    parser.addOption(QCommandLineOption("texture",
        "[Texture] Crystallographic texture preset: random | extrusion | rolling | recrystallization | shear | scattered_cube", "preset"));
    parser.addOption(QCommandLineOption("lattice",
        "[Texture] Crystal lattice override: fcc | bcc (default: from material, or fcc).", "lat"));
    parser.addOption(QCommandLineOption("scatter",
        "[Texture] Texture orientation scatter spread in degrees (default: 11.0).", "deg"));

    // ── Stress Analysis & Homogenization ──────────────────────────────────
    parser.addOption(QCommandLineOption("run_stress_calc",
        "[Stress] Run stress/strain homogenization after structure generation."));
    parser.addOption(QCommandLineOption("solver",
        "[Stress] Homogenization solver: fft (in-memory Moulinec-Suquet) | ansys (external FEM). Default: ansys", "solver"));
    parser.addOption(QCommandLineOption("stress_mode",
        "[Stress] Calculation mode: stiffness (fast 6 solves -> S, C, moduli) | single (prescribed --eps) | dataset (full 300 loads). Default: dataset", "mode"));
    parser.addOption(QCommandLineOption("eps",
        "[Stress] Prescribed macroscopic strain tensor for '--stress_mode single': exx,eyy,ezz,exy,eyz,exz", "strains"));
    parser.addOption(QCommandLineOption("num_rnd_loads",
        "[Stress] Number of random loads for Hill yield criterion fit in dataset mode (default: 150).", "count"));
    parser.addOption(QCommandLineOption("working_directory",
        "[Stress] Working directory for ANSYS scratch and APDL files.", "dir"));

    // ── Machine-Readable Agent Metadata ───────────────────────────────────
    parser.addOption(QCommandLineOption(QStringList() << "help-json" << "json-help",
        "[Agent] Output complete CLI schema, algorithms, materials, presets, and options as JSON and exit."));
}

void Commandline_Parser::printJsonHelp()
{
    // Silence debug logs so output is 100% pure JSON even if stderr is combined
    auto noopHandler = [](QtMsgType, const QMessageLogContext&, const QString&) {};
    auto oldHandler = qInstallMessageHandler(noopHandler);

    QJsonObject root;
    root["application"] = "MatViz3D";
    root["version"] = "3.01";
    root["description"] = "Cellular Automata 3D Microstructure Generator & Homogenization Stress Analyzer";

    QJsonObject modes;
    modes["gui"] = "Launch without --nogui for interactive Qt/QML desktop UI";
    modes["headless_generation"] = "MatViz3D --nogui --autostart --algorithm <algo> --size <n> --points <n>";
    modes["headless_stress_analysis"] = "MatViz3D --nogui --autostart --run_stress_calc --solver <fft|ansys> --stress_mode <mode>";
    root["modes"] = modes;

    QJsonArray recipes;
    auto addRecipe = [&](const QString& name, const QString& desc, const QString& cmd) {
        QJsonObject r;
        r["name"] = name;
        r["description"] = desc;
        r["command"] = cmd;
        recipes.append(r);
    };
    addRecipe("Voronoi Polycrystal",
              "Periodic Voronoi tessellation (30^3, 50 grains, copper)",
              "MatViz3D --nogui --autostart --algorithm Voronoi --size 30 --points 50 --periodic --material Cu --output voronoi.hdf5");
    addRecipe("Composite Fiber RVE",
              "2D orthogonal continuous fibers on hexagonal lattice with 40% volume fraction",
              "MatViz3D --nogui --autostart --algorithm Composite --size 40 --composite_dim 2d --composite_packing hexagonal --fiber_volume_fraction 0.4 --matrix_material Epoxy --fiber_material C-fiber --output composite.hdf5");
    addRecipe("Fast In-Memory FFT Stiffness Tensor",
              "Phase 1 homogenization (6 unit strain solves) computing effective S, C, and engineering moduli",
              "MatViz3D --nogui --autostart --algorithm Voronoi --size 20 --points 30 --material Al --run_stress_calc --solver fft --stress_mode stiffness --output stiffness.hdf5");
    addRecipe("Single Load Case Prescribed Strain",
              "Single-shot stress solve under prescribed macroscopic strain tensor",
              "MatViz3D --nogui --autostart --algorithm Voronoi --size 20 --points 20 --run_stress_calc --solver fft --stress_mode single --eps 0.001,0,0,0,0,0 --output single.hdf5");
    root["recipes"] = recipes;

    QJsonArray algos;
    for (const QString& name : AlgorithmFactory::instance().algorithmNames()) {
        const AlgorithmPlugin* plugin = AlgorithmFactory::instance().pluginFor(name);
        QJsonObject a;
        a["name"] = name;
        a["description"] = plugin ? plugin->description : QString();
        algos.append(a);
    }
    root["algorithms"] = algos;

    QJsonArray mats;
    QStringList matList = DBManager::materialNames();
    if (matList.isEmpty())
        matList << "Cu" << "Fe" << "Al" << "W" << "Ti" << "Ni" << "Brass" << "Epoxy" << "C-fiber" << "E-glass" << "SiC" << "Al2O3";
    for (const QString& m : matList) mats.append(m);
    root["materials"] = mats;

    QJsonArray textures;
    textures.append("random");
    textures.append("extrusion");
    textures.append("rolling");
    textures.append("recrystallization");
    textures.append("shear");
    textures.append("scattered_cube");
    root["texture_presets"] = textures;

    QJsonArray options;
    auto addOpt = [&](const QString& name, const QString& cat, const QString& type,
                      const QString& valName, const QString& defVal, const QString& desc,
                      const QStringList& choices = {}) {
        QJsonObject o;
        o["name"] = name;
        o["flag"] = "--" + name;
        o["category"] = cat;
        o["type"] = type;
        if (!valName.isEmpty()) o["value_name"] = valName;
        if (!defVal.isEmpty()) o["default"] = defVal;
        o["description"] = desc;
        if (!choices.isEmpty()) {
            QJsonArray ch;
            for (const QString& c : choices) ch.append(c);
            o["choices"] = ch;
        }
        options.append(o);
    };

    // Configuration File
    addOpt("config", "Config", "string", "file", "", "Load run parameters from configuration file (.json supported; explicit CLI options override file)");

    // Execution & Headless
    addOpt("nogui", "Execution", "bool", "", "false", "Run in headless mode without GUI (requires --autostart to execute)");
    addOpt("nologo", "Execution", "bool", "", "false", "Suppress printing the ASCII logo banner on startup");
    addOpt("autostart", "Execution", "bool", "", "false", "Automatically start structure generation on startup (required for --nogui)");
    addOpt("animate", "Execution", "bool", "", "false", "Grow structure iteration by iteration instead of one-shot generation");
    addOpt("np", "System", "int", "threads", "CPU cores", "Number of OpenMP worker threads for algorithm execution");
    addOpt("seed", "System", "uint", "uint", "timestamp", "RNG seed for reproducible structure generation");
    addOpt("output", "Output", "string", "file", "current_ls.hdf5", "Output HDF5 filepath for structure and fields");

    // Grid Geometry & Nucleation
    addOpt("size", "Grid", "int", "n", "", "Voxel grid dimension N for N x N x N cell (integer > 0)");
    addOpt("points", "Grid", "int", "count", "", "Number of initial nucleation seeds / grains (integer > 0)");
    addOpt("concentration", "Grid", "float", "pct", "", "Nucleation seed density as volume percentage in (0, 100]; overrides --points");
    addOpt("algorithm", "Grid", "string", "name", "Voronoi", "Generation algorithm name", QStringList() << "Voronoi" << "Composite" << "Probability" << "Polycrystall" << "DLCA" << "Moore" << "Neumann" << "Radial" << "Thin Layer");
    addOpt("periodic", "Grid", "bool", "", "false", "Periodic boundary conditions (grains/fibers wrap across opposite cell faces)");

    // Polycrystall
    addOpt("neighborhood", "Polycrystall", "string", "stencil", "Moore (26)", "Cellular automaton neighborhood stencil",
           QStringList() << "Moore (26)" << "von Neumann (6)" << "Radial (18)");
    addOpt("thin-layer", "Polycrystall", "bool", "", "false", "Generate all initial grain seeds on the same plane (thin layer mode)");
    addOpt("layer-direction", "Polycrystall", "string", "dir", "+Z",
           "Starting direction of the thin layer film ('+Z', '-Z', '+X', '-X', '+Y', '-Y')",
           QStringList() << "+Z" << "-Z" << "+X" << "-X" << "+Y" << "-Y");

    // Voronoi
    addOpt("minkowski_p", "Voronoi", "double", "p", "2.0", "Minkowski exponent: 1 = Manhattan, 2 = Euclidean, large = Chebyshev");
    addOpt("voronoi_metric_preset", "Voronoi", "string", "preset", "", "Preset for Voronoi shape / metric tensor",
           QStringList() << "Sphere (Circle)" << "Prolate (Needle)" << "Oblate (Disc)" << "Triaxial Ellipsoid"
                         << "Superellipsoid (Cube)" << "Columnar (Z-axis)" << "Columnar (X-axis)"
                         << "Rolled (Orthotropic)" << "Sheared (45 deg XY)" << "Custom");
    addOpt("voronoi_metric", "Voronoi", "string", "components", "", "Metric tensor components: 'mxx,myy,mzz' or 'mxx,myy,mzz,mxy,myz,mxz'");
    addOpt("voronoi_mxx", "Voronoi", "double", "val", "1.0", "Metric tensor diagonal M_xx (M11) scaling X-elongation");
    addOpt("voronoi_myy", "Voronoi", "double", "val", "1.0", "Metric tensor diagonal M_yy (M22) scaling Y-elongation");
    addOpt("voronoi_mzz", "Voronoi", "double", "val", "1.0", "Metric tensor diagonal M_zz (M33) scaling Z-elongation");
    addOpt("voronoi_mxy", "Voronoi", "double", "val", "0.0", "Metric tensor off-diagonal M_xy (M12) XY shear coupling");
    addOpt("voronoi_myz", "Voronoi", "double", "val", "0.0", "Metric tensor off-diagonal M_yz (M23) YZ shear coupling");
    addOpt("voronoi_mxz", "Voronoi", "double", "val", "0.0", "Metric tensor off-diagonal M_xz (M13) XZ shear coupling");

    // Composite
    addOpt("composite_dim", "Composite", "string", "dim", "1d", "Reinforcement dimensionality: 1d (along Z) | 2d (along X,Y) | 3d (along X,Y,Z)", QStringList() << "1d" << "2d" << "3d");
    addOpt("composite_packing", "Composite", "string", "packing", "square", "Fiber cross-section packing: square | hexagonal", QStringList() << "square" << "hexagonal");
    addOpt("fiber_volume_fraction", "Composite", "double", "frac", "0.4", "Target fiber volume fraction in (0, 1)");
    addOpt("fibers_per_row", "Composite", "int", "n", "3", "Fibers per row in cross-section lattice");
    addOpt("fiber_aspect_ratio", "Composite", "double", "ratio", "1.0", "Fiber elliptical cross-section a/b (major/minor semi-axis; 1 = circular)");
    addOpt("fiber_angle_scatter", "Composite", "double", "deg", "0.0", "Per-fiber in-plane rotation spread in degrees [0, 180]");
    addOpt("fiber_center_jitter", "Composite", "double", "val", "0.0", "Random shift of fiber centers in [0, 1] of half-pitches");
    addOpt("fiber_allow_overlap", "Composite", "bool", "", "false", "Let jittered fibers overlap and merge; lifts packing limit on volume fraction");
    addOpt("matrix_material", "Composite", "string", "name", "Epoxy", "Matrix constituent from material database");
    addOpt("fiber_material", "Composite", "string", "name", "C-fiber", "Fiber constituent from material database");

    // Probability
    addOpt("prob_preset", "Probability", "string", "preset", "", "Kernel shape preset",
           QStringList() << "Sphere (Circle)" << "Prolate (Needle)" << "Oblate (Disc)" << "Triaxial Ellipsoid" << "Superellipsoid (Cube)" << "Custom");
    addOpt("prob_matrix_mode", "Probability", "string", "mode", "volume", "Transition matrix calculation method: volume | surface", QStringList() << "volume" << "surface");
    addOpt("halfaxis_a", "Probability", "float", "val", "", "Kernel semi-axis A length along X");
    addOpt("halfaxis_b", "Probability", "float", "val", "", "Kernel semi-axis B length along Y");
    addOpt("halfaxis_c", "Probability", "float", "val", "", "Kernel semi-axis C length along Z");
    addOpt("orientation_angle_a", "Probability", "float", "deg", "", "Kernel Euler rotation angle about X-axis in degrees");
    addOpt("orientation_angle_b", "Probability", "float", "deg", "", "Kernel Euler rotation angle about Y-axis in degrees");
    addOpt("orientation_angle_c", "Probability", "float", "deg", "", "Kernel Euler rotation angle about Z-axis in degrees");
    addOpt("ellipse_order", "Probability", "double", "p", "2.0", "Superellipse equation exponent in |x/a|^p + |y/b|^p + |z/c|^p <= 1");
    addOpt("stefan_number", "Probability", "float", "val", "", "Thermodynamic Stefan cooling limit number for probabilistic growth");
    addOpt("wave_generation", "Probability", "bool", "", "false", "Enable continuous wave nucleation (transformation-fraction controlled)");
    addOpt("initial_nuclei", "Probability", "int", "count", "", "Number of initial nuclei present at step 0 for wave nucleation");
    addOpt("wave_peak_fraction", "Probability", "float", "frac", "0.20", "Solid volume fraction where nucleation rate peaks in [0, 1]");
    addOpt("wave_end_fraction", "Probability", "float", "frac", "0.60", "Solid volume fraction where 100% of nuclei are placed in [0, 1]");
    addOpt("wave_coefficient", "Probability", "float", "val", "0.1", "Rate coefficient for wave nucleation kinetics");

    // Material & Texture
    addOpt("material", "Material", "string", "name", "Cu", "Material from database supplying cubic constants and default lattice");
    addOpt("texture", "Texture", "string", "preset", "", "Crystallographic texture preset",
           QStringList() << "random" << "extrusion" << "rolling" << "recrystallization" << "shear" << "scattered_cube");
    addOpt("lattice", "Texture", "string", "lat", "fcc", "Crystal lattice override: fcc | bcc", QStringList() << "fcc" << "bcc");
    addOpt("scatter", "Texture", "double", "deg", "11.0", "Texture orientation scatter spread in degrees");

    // Stress Analysis
    addOpt("run_stress_calc", "Stress", "bool", "", "false", "Run stress/strain homogenization after structure generation");
    addOpt("solver", "Stress", "string", "solver", "ansys", "Homogenization solver: fft | ansys", QStringList() << "ansys" << "fft");
    addOpt("stress_mode", "Stress", "string", "mode", "dataset", "Calculation mode: stiffness (fast 6 solves) | single | dataset (full 300 loads)", QStringList() << "dataset" << "stiffness" << "single");
    addOpt("eps", "Stress", "string", "strains", "", "Applied strain tensor for --stress_mode single: exx,eyy,ezz,exy,eyz,exz");
    addOpt("num_rnd_loads", "Stress", "uint", "count", "150", "Number of random loads for Hill yield criterion fit in dataset mode");
    addOpt("working_directory", "Stress", "string", "dir", "", "Working directory for ANSYS scratch and APDL files");

    // Agent Metadata
    addOpt("help-json", "Agent", "bool", "", "false", "Output complete CLI schema, algorithms, materials, and options as JSON and exit");

    root["options"] = options;

    QJsonDocument doc(root);
    QTextStream out(stdout);
    out << doc.toJson(QJsonDocument::Indented);
    out.flush();

    qInstallMessageHandler(oldHandler);
}

// Maps --texture to TextureLibrary::Process. Returns false on an unknown name.
bool Commandline_Parser::parseProcess(const QString& name, TextureLibrary::Process& out)
{
    const QString n = name.trimmed().toLower();
    if (n == "random")                                     out = TextureLibrary::Process::Random;
    else if (n == "extrusion")                             out = TextureLibrary::Process::Extrusion;
    else if (n == "rolling")                               out = TextureLibrary::Process::Rolling;
    else if (n == "recrystallization" || n == "recryst")   out = TextureLibrary::Process::Recrystallization;
    else if (n == "shear")                                 out = TextureLibrary::Process::Shear;
    else if (n == "scattered_cube" || n == "scatteredcube" ||
             n == "scattered-cube")                        out = TextureLibrary::Process::ScatteredCube;
    else return false;
    return true;
}

bool Commandline_Parser::parseLattice(const QString& name, TextureLibrary::Lattice& out)
{
    const QString n = name.trimmed().toLower();
    if (n == "fcc")      out = TextureLibrary::Lattice::FCC;
    else if (n == "bcc") out = TextureLibrary::Lattice::BCC;
    else return false;
    return true;
}

bool Commandline_Parser::isValidCompositeDim(const QString& v)
{
    const QString n = v.trimmed().toLower();
    return n.startsWith('1') || n.startsWith('2') || n.startsWith('3');
}

bool Commandline_Parser::isValidCompositePacking(const QString& v)
{
    const QString n = v.trimmed().toLower();
    return n == "square" || n == "hexagonal" || n == "hex";
}

bool Commandline_Parser::isValidSolver(const QString& v)
{
    const QString s = v.trimmed().toLower();
    return s == "ansys" || s == "fft";
}

bool Commandline_Parser::isValidStressMode(const QString& v)
{
    const QString m = v.trimmed().toLower();
    return m == "single" || m == "dataset" || m == "stiffness";
}

bool Commandline_Parser::applyParameter(const QString& key, const QString& value, QString* error)
{
    Parameters* params = Parameters::instance();

    QString normKey = key.trimmed().toLower();
    normKey.replace('-', '_');
    normKey.replace('.', '_');

    auto setErr = [&](const QString& msg) {
        if (error) *error = msg;
        return false;
    };

    auto parseInt = [&](int& out, bool positive = false) -> bool {
        bool ok = false;
        int v = value.trimmed().toInt(&ok);
        if (!ok) {
            return setErr(QString("expects an integer; got \"%1\"").arg(value));
        }
        if (positive && v <= 0) {
            return setErr(QString("expects a positive integer; got %1").arg(v));
        }
        out = v;
        return true;
    };

    auto parseUInt = [&](unsigned int& out) -> bool {
        bool ok = false;
        unsigned int v = value.trimmed().toUInt(&ok);
        if (!ok) {
            return setErr(QString("expects a non-negative integer; got \"%1\"").arg(value));
        }
        out = v;
        return true;
    };

    auto parseFloat = [&](float& out, bool positive = false) -> bool {
        bool ok = false;
        float v = value.trimmed().toFloat(&ok);
        if (!ok) {
            return setErr(QString("expects a float; got \"%1\"").arg(value));
        }
        if (positive && v <= 0.0f) {
            return setErr(QString("expects a positive value; got %1").arg(value));
        }
        out = v;
        return true;
    };

    auto parseDouble = [&](double& out, bool positive = false) -> bool {
        bool ok = false;
        double v = value.trimmed().toDouble(&ok);
        if (!ok) {
            return setErr(QString("expects a double; got \"%1\"").arg(value));
        }
        if (positive && v <= 0.0) {
            return setErr(QString("expects a positive value; got %1").arg(value));
        }
        out = v;
        return true;
    };

    auto parseBool = [&](bool& out) -> bool {
        const QString v = value.trimmed().toLower();
        if (v == "true" || v == "1" || v == "yes" || v == "on") {
            out = true;
            return true;
        }
        if (v == "false" || v == "0" || v == "no" || v == "off") {
            out = false;
            return true;
        }
        return setErr(QString("expects a boolean ('true' or 'false'); got \"%1\"").arg(value));
    };

    if (normKey == "size") {
        int v = 0;
        if (!parseInt(v, true)) return false;
        params->setSize(v);
        return true;
    }

    if (normKey == "points") {
        int v = 0;
        if (!parseInt(v, true)) return false;
        if (params->getSize() > 0 && v > std::pow(params->getSize(), 3)) {
            return setErr(QString("Initial points (%1) exceed the cube volume (%2); lower points or raise size")
                              .arg(QString::number(v), QString::number(std::pow(params->getSize(), 3))));
        }
        params->setPoints(v);
        return true;
    }

    if (normKey == "concentration") {
        bool ok = false;
        float pct = value.trimmed().toFloat(&ok);
        if (!ok) {
            return setErr(QString("Option concentration expects a float; got \"%1\"").arg(value));
        }
        if (pct <= 0.0f || pct > 100.0f) {
            return setErr(QString("Option concentration expects a value in (0, 100]; got %1").arg(value));
        }
        const double volume = std::pow(static_cast<double>(params->getSize()), 3);
        const int derived = static_cast<int>(std::lround(static_cast<double>(pct) / 100.0 * volume));
        if (derived <= 0) {
            return setErr(QString("Option concentration resolved to %1 points; raise size or concentration").arg(derived));
        }
        params->setPoints(derived);
        return true;
    }

    if (normKey == "algorithm") {
        params->setAlgorithm(value.trimmed());
        return true;
    }

    if (normKey == "periodic" || normKey == "is_periodic") {
        bool b = false;
        if (!parseBool(b)) return false;
        params->setIsPeriodic(b);
        return true;
    }

    if (normKey == "neighborhood" || normKey == "polycrystall_neighborhood") {
        const QString v = value.trimmed();
        const QString vl = v.toLower();
        if (vl == "moore" || vl == "moore (26)") {
            params->setPolycrystallNeighborhood("Moore (26)");
        } else if (vl == "neumann" || vl == "von neumann" || vl == "von neumann (6)") {
            params->setPolycrystallNeighborhood("von Neumann (6)");
        } else if (vl == "radial" || vl == "radial (18)") {
            params->setPolycrystallNeighborhood("Radial (18)");
        } else {
            params->setPolycrystallNeighborhood(v);
        }
        return true;
    }

    if (normKey == "thin_layer" || normKey == "is_thin_layer") {
        bool b = false;
        if (!parseBool(b)) return false;
        params->setIsThinLayer(b);
        return true;
    }

    if (normKey == "layer_direction") {
        const QString v = value.trimmed().toUpper();
        if (v != "+Z" && v != "-Z" && v != "+X" && v != "-X" && v != "+Y" && v != "-Y") {
            return setErr(QString("Option layer_direction expects one of: +Z, -Z, +X, -X, +Y, -Y; got \"%1\"").arg(value));
        }
        params->setLayerDirection(v);
        params->setIsThinLayer(true);
        return true;
    }

    if (normKey == "minkowski_p") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        params->setMinkowskiP(v);
        return true;
    }

    if (normKey == "voronoi_metric_preset") {
        params->setVoronoiMetricPreset(value.trimmed());
        return true;
    }

    if (normKey == "voronoi_metric") {
        const QStringList parts = value.split(',');
        if (parts.size() != 3 && parts.size() != 6) {
            return setErr(QString("voronoi_metric expects 3 or 6 comma-separated values, got %1").arg(parts.size()));
        }
        bool ok1 = false, ok2 = false, ok3 = false;
        double mxx = parts[0].trimmed().toDouble(&ok1);
        double myy = parts[1].trimmed().toDouble(&ok2);
        double mzz = parts[2].trimmed().toDouble(&ok3);
        if (!ok1 || !ok2 || !ok3) {
            return setErr(QString("voronoi_metric diagonal components must be valid numbers"));
        }
        params->setVoronoiMxx(mxx);
        params->setVoronoiMyy(myy);
        params->setVoronoiMzz(mzz);
        if (parts.size() == 6) {
            bool ok4 = false, ok5 = false, ok6 = false;
            double mxy = parts[3].trimmed().toDouble(&ok4);
            double myz = parts[4].trimmed().toDouble(&ok5);
            double mxz = parts[5].trimmed().toDouble(&ok6);
            if (!ok4 || !ok5 || !ok6) {
                return setErr(QString("voronoi_metric off-diagonal components must be valid numbers"));
            }
            params->setVoronoiMxy(mxy);
            params->setVoronoiMyz(myz);
            params->setVoronoiMxz(mxz);
        }
        return true;
    }

    if (normKey == "voronoi_mxx") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        params->setVoronoiMxx(v);
        return true;
    }
    if (normKey == "voronoi_myy") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        params->setVoronoiMyy(v);
        return true;
    }
    if (normKey == "voronoi_mzz") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        params->setVoronoiMzz(v);
        return true;
    }
    if (normKey == "voronoi_mxy") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        params->setVoronoiMxy(v);
        return true;
    }
    if (normKey == "voronoi_myz") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        params->setVoronoiMyz(v);
        return true;
    }
    if (normKey == "voronoi_mxz") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        params->setVoronoiMxz(v);
        return true;
    }

    if (normKey == "composite_dim") {
        if (!isValidCompositeDim(value)) {
            return setErr(QString("Option composite_dim expects 1d, 2d or 3d; got \"%1\"").arg(value));
        }
        params->setCompositeDim(value);
        return true;
    }

    if (normKey == "composite_packing") {
        if (!isValidCompositePacking(value)) {
            return setErr(QString("Option composite_packing expects square or hexagonal; got \"%1\"").arg(value));
        }
        params->setCompositePacking(value);
        return true;
    }

    if (normKey == "fiber_volume_fraction" || normKey == "composite_fiber_volume_fraction") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v <= 0.0 || v >= 1.0) {
            return setErr(QString("Option fiber_volume_fraction expects a value in (0, 1); got %1").arg(value));
        }
        params->setFiberVolumeFraction(v);
        return true;
    }

    if (normKey == "fibers_per_row" || normKey == "composite_fibers_per_row") {
        int v = 0;
        if (!parseInt(v, true)) return false;
        params->setFibersPerRow(v);
        return true;
    }

    if (normKey == "fiber_aspect_ratio" || normKey == "composite_fiber_aspect_ratio") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        params->setFiberAspectRatio(v);
        return true;
    }

    if (normKey == "fiber_angle_scatter" || normKey == "composite_fiber_angle_scatter") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0.0 || v > 180.0) {
            return setErr(QString("Option fiber_angle_scatter expects degrees in [0, 180]; got %1").arg(value));
        }
        params->setFiberAngleScatter(v);
        return true;
    }

    if (normKey == "fiber_center_jitter" || normKey == "composite_fiber_center_jitter") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0.0 || v > 1.0) {
            return setErr(QString("Option fiber_center_jitter expects a value in [0, 1]; got %1").arg(value));
        }
        params->setFiberCenterJitter(v);
        return true;
    }

    if (normKey == "fiber_allow_overlap" || normKey == "composite_fiber_allow_overlap") {
        bool b = false;
        if (!parseBool(b)) return false;
        params->setFiberAllowOverlap(b);
        return true;
    }

    if (normKey == "matrix_material" || normKey == "composite_matrix_material") {
        params->setMatrixMaterial(value.trimmed());
        return true;
    }

    if (normKey == "fiber_material" || normKey == "composite_fiber_material") {
        params->setFiberMaterial(value.trimmed());
        return true;
    }

    if (normKey == "prob_preset") {
        params->setProbPreset(value.trimmed());
        return true;
    }

    if (normKey == "prob_matrix_mode") {
        const QString m = value.trimmed().toLower();
        if (m == "surface" || m == "surface flux" || m == "surface_flux") {
            params->setProbMatrixMode("Surface Flux");
        } else {
            params->setProbMatrixMode("Volume Sampling");
        }
        return true;
    }

    if (normKey == "halfaxis_a") {
        float v = 0.0f;
        if (!parseFloat(v)) return false;
        params->setHalfAxisA(v);
        params->setHasProbParameters(true);
        return true;
    }
    if (normKey == "halfaxis_b") {
        float v = 0.0f;
        if (!parseFloat(v)) return false;
        params->setHalfAxisB(v);
        params->setHasProbParameters(true);
        return true;
    }
    if (normKey == "halfaxis_c") {
        float v = 0.0f;
        if (!parseFloat(v)) return false;
        params->setHalfAxisC(v);
        params->setHasProbParameters(true);
        return true;
    }

    if (normKey == "orientation_angle_a") {
        float v = 0.0f;
        if (!parseFloat(v)) return false;
        params->setOrientationAngleA(v);
        params->setHasProbParameters(true);
        return true;
    }
    if (normKey == "orientation_angle_b") {
        float v = 0.0f;
        if (!parseFloat(v)) return false;
        params->setOrientationAngleB(v);
        params->setHasProbParameters(true);
        return true;
    }
    if (normKey == "orientation_angle_c") {
        float v = 0.0f;
        if (!parseFloat(v)) return false;
        params->setOrientationAngleC(v);
        params->setHasProbParameters(true);
        return true;
    }

    if (normKey == "ellipse_order") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        params->setEllipseOrder(v);
        return true;
    }

    if (normKey == "stefan_number") {
        float v = 0.0f;
        if (!parseFloat(v)) return false;
        params->setStefanNumber(v);
        return true;
    }

    if (normKey == "animate" || normKey == "is_animation") {
        bool b = false;
        if (!parseBool(b)) return false;
        params->setIsAnimation(b);
        return true;
    }

    if (normKey == "wave_generation" || normKey == "is_wave_generation") {
        bool b = false;
        if (!parseBool(b)) return false;
        params->setIsWaveGeneration(b);
        return true;
    }

    if (normKey == "initial_nuclei" || normKey == "initial_nuclei_count") {
        int v = 0;
        if (!parseInt(v)) return false;
        params->setInitialNucleiCount(v);
        return true;
    }

    if (normKey == "wave_peak_fraction") {
        float v = 0.0f;
        if (!parseFloat(v)) return false;
        params->setWavePeakFraction(v);
        return true;
    }

    if (normKey == "wave_end_fraction") {
        float v = 0.0f;
        if (!parseFloat(v)) return false;
        params->setWaveEndFraction(v);
        return true;
    }

    if (normKey == "wave_coefficient") {
        float v = 0.0f;
        if (!parseFloat(v)) return false;
        params->setWaveCoefficient(v);
        return true;
    }

    if (normKey == "material" || normKey == "db_material") {
        params->setDbMaterial(value.trimmed());
        return true;
    }

    if (normKey == "lattice" || normKey == "lattice_override") {
        TextureLibrary::Lattice lat;
        if (!parseLattice(value, lat)) {
            return setErr(QString("Option lattice expects fcc or bcc; got \"%1\"").arg(value));
        }
        params->setLatticeOverride(value.trimmed().toLower());
        return true;
    }

    if (normKey == "texture" || normKey == "texture_preset") {
        TextureLibrary::Process proc;
        if (!parseProcess(value, proc)) {
            return setErr(QString("Option texture expects one of: random, extrusion, rolling, "
                                  "recrystallization, shear, scattered_cube; got \"%1\"").arg(value));
        }
        params->setTexturePreset(value.trimmed());
        return true;
    }

    if (normKey == "scatter" || normKey == "texture_scatter") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0.0) {
            return setErr(QString("Option scatter expects a non-negative number of degrees; got \"%1\"").arg(value));
        }
        params->setTextureScatter(v);
        return true;
    }

    if (normKey == "num_rnd_loads") {
        unsigned int v = 0;
        if (!parseUInt(v)) return false;
        params->setNumRndLoads(v);
        return true;
    }

    if (normKey == "solver" || normKey == "stress_solver") {
        if (!isValidSolver(value)) {
            return setErr(QString("Option solver expects ansys or fft; got \"%1\"").arg(value));
        }
        params->setStressSolver(value.trimmed().toLower());
        return true;
    }

    if (normKey == "stress_mode") {
        if (!isValidStressMode(value)) {
            return setErr(QString("Option stress_mode expects single, dataset or stiffness; got \"%1\"").arg(value));
        }
        params->setStressMode(value.trimmed().toLower());
        return true;
    }

    if (normKey == "eps" || normKey == "stress_eps") {
        const QStringList parts = value.split(',');
        if (parts.size() != 6) {
            return setErr(QString("Option eps expects 6 comma-separated values, got %1").arg(parts.size()));
        }
        double e[6];
        for (int i = 0; i < 6; ++i) {
            bool ok = false;
            e[i] = parts[i].trimmed().toDouble(&ok);
            if (!ok) {
                return setErr(QString("Option eps: component %1 is not a number: \"%2\"")
                                  .arg(QString::number(i + 1), parts[i].trimmed()));
            }
        }
        params->setStressEps(e);
        return true;
    }

    if (normKey == "output" || normKey == "filename") {
        params->setFilename(value.trimmed());
        return true;
    }

    if (normKey == "working_directory") {
        params->setWorkingDirectory(value.trimmed());
        return true;
    }

    if (normKey == "seed") {
        unsigned int v = 0;
        if (!parseUInt(v)) return false;
        params->setSeed(v);
        return true;
    }

    if (normKey == "np" || normKey == "num_threads") {
        int v = 0;
        if (!parseInt(v, true)) return false;
        params->setNumThreads(v);
        return true;
    }

    return setErr(QString("Unknown parameter key: '%1'").arg(key));
}

bool applyParameter(const QString& key, const QString& value, QString* error)
{
    return Commandline_Parser::applyParameter(key, value, error);
}

bool Commandline_Parser::processOptions(const QCommandLineParser& parser, QString* error)
{
    Parameters* params = Parameters::instance();
    QSet<QString> appliedKeys;

    auto setErr = [&](const QString& msg) -> bool {
        if (error) *error = msg;
        qCritical().noquote() << msg;
        return false;
    };

    auto normalizeKey = [](const QString& k) -> QString {
        QString s = k.trimmed().toLower();
        s.replace('-', '_');
        s.replace('.', '_');
        return s;
    };

    auto applyCliOption = [&](const QString& opt, const QString& val) -> bool {
        QString err;
        if (!applyParameter(opt, val, &err)) {
            return setErr(QString("Option --%1: %2").arg(opt, err));
        }
        appliedKeys.insert(normalizeKey(opt));
        qInfo() << opt << ":" << val;
        return true;
    };

    // ── Load order Step 1: Config file (if specified) ─────────────────────
    if (parser.isSet("config")) {
        const QString configPath = parser.value("config");
        QString configErr;
        if (!ConfigDispatcher::loadAndApply(configPath, &configErr, &appliedKeys)) {
            return setErr(configErr);
        }
    }

    // ── Load order Step 2: Explicit CLI options (isSet overrides file) ─────
    if (parser.isSet("size"))               if (!applyCliOption("size", parser.value("size"))) return false;
    if (parser.isSet("points"))             if (!applyCliOption("points", parser.value("points"))) return false;
    if (parser.isSet("concentration")) {
        if (parser.isSet("points")) {
            qWarning() << "Both --points and --concentration were given;"
                       << "--concentration wins and --points is ignored";
        }
        if (!applyCliOption("concentration", parser.value("concentration"))) return false;
    }
    if (parser.isSet("algorithm"))          if (!applyCliOption("algorithm", parser.value("algorithm"))) return false;
    if (parser.isSet("periodic"))           if (!applyCliOption("periodic", "true")) return false;

    if (parser.isSet("neighborhood")) {
        if (!applyCliOption("neighborhood", parser.value("neighborhood"))) return false;
    } else if (parser.isSet("polycrystall_neighborhood")) {
        if (!applyCliOption("polycrystall_neighborhood", parser.value("polycrystall_neighborhood"))) return false;
    }

    if (parser.isSet("thin-layer") || parser.isSet("thin_layer")) {
        if (!applyCliOption("thin_layer", "true")) return false;
    }
    if (parser.isSet("layer-direction")) {
        if (!applyCliOption("layer_direction", parser.value("layer-direction"))) return false;
    } else if (parser.isSet("layer_direction")) {
        if (!applyCliOption("layer_direction", parser.value("layer_direction"))) return false;
    }

    if (parser.isSet("minkowski_p"))        if (!applyCliOption("minkowski_p", parser.value("minkowski_p"))) return false;
    if (parser.isSet("voronoi_metric_preset")) if (!applyCliOption("voronoi_metric_preset", parser.value("voronoi_metric_preset"))) return false;
    if (parser.isSet("voronoi_metric"))     if (!applyCliOption("voronoi_metric", parser.value("voronoi_metric"))) return false;
    if (parser.isSet("voronoi_mxx"))        if (!applyCliOption("voronoi_mxx", parser.value("voronoi_mxx"))) return false;
    if (parser.isSet("voronoi_myy"))        if (!applyCliOption("voronoi_myy", parser.value("voronoi_myy"))) return false;
    if (parser.isSet("voronoi_mzz"))        if (!applyCliOption("voronoi_mzz", parser.value("voronoi_mzz"))) return false;
    if (parser.isSet("voronoi_mxy"))        if (!applyCliOption("voronoi_mxy", parser.value("voronoi_mxy"))) return false;
    if (parser.isSet("voronoi_myz"))        if (!applyCliOption("voronoi_myz", parser.value("voronoi_myz"))) return false;
    if (parser.isSet("voronoi_mxz"))        if (!applyCliOption("voronoi_mxz", parser.value("voronoi_mxz"))) return false;

    if (parser.isSet("composite_dim"))           if (!applyCliOption("composite_dim", parser.value("composite_dim"))) return false;
    if (parser.isSet("composite_packing"))       if (!applyCliOption("composite_packing", parser.value("composite_packing"))) return false;
    if (parser.isSet("fiber_volume_fraction"))   if (!applyCliOption("fiber_volume_fraction", parser.value("fiber_volume_fraction"))) return false;
    if (parser.isSet("fibers_per_row"))          if (!applyCliOption("fibers_per_row", parser.value("fibers_per_row"))) return false;
    if (parser.isSet("fiber_aspect_ratio"))      if (!applyCliOption("fiber_aspect_ratio", parser.value("fiber_aspect_ratio"))) return false;
    if (parser.isSet("fiber_angle_scatter"))     if (!applyCliOption("fiber_angle_scatter", parser.value("fiber_angle_scatter"))) return false;
    if (parser.isSet("fiber_center_jitter"))     if (!applyCliOption("fiber_center_jitter", parser.value("fiber_center_jitter"))) return false;
    if (parser.isSet("fiber_allow_overlap"))     if (!applyCliOption("fiber_allow_overlap", "true")) return false;
    if (parser.isSet("matrix_material"))         if (!applyCliOption("matrix_material", parser.value("matrix_material"))) return false;
    if (parser.isSet("fiber_material"))          if (!applyCliOption("fiber_material", parser.value("fiber_material"))) return false;

    if (parser.isSet("prob_preset"))             if (!applyCliOption("prob_preset", parser.value("prob_preset"))) return false;
    if (parser.isSet("prob_matrix_mode"))        if (!applyCliOption("prob_matrix_mode", parser.value("prob_matrix_mode"))) return false;
    if (parser.isSet("halfaxis_a"))              if (!applyCliOption("halfaxis_a", parser.value("halfaxis_a"))) return false;
    if (parser.isSet("halfaxis_b"))              if (!applyCliOption("halfaxis_b", parser.value("halfaxis_b"))) return false;
    if (parser.isSet("halfaxis_c"))              if (!applyCliOption("halfaxis_c", parser.value("halfaxis_c"))) return false;
    if (parser.isSet("orientation_angle_a"))     if (!applyCliOption("orientation_angle_a", parser.value("orientation_angle_a"))) return false;
    if (parser.isSet("orientation_angle_b"))     if (!applyCliOption("orientation_angle_b", parser.value("orientation_angle_b"))) return false;
    if (parser.isSet("orientation_angle_c"))     if (!applyCliOption("orientation_angle_c", parser.value("orientation_angle_c"))) return false;
    if (parser.isSet("ellipse_order"))           if (!applyCliOption("ellipse_order", parser.value("ellipse_order"))) return false;
    if (parser.isSet("stefan_number"))           if (!applyCliOption("stefan_number", parser.value("stefan_number"))) return false;
    if (parser.isSet("animate"))                 if (!applyCliOption("animate", "true")) return false;
    if (parser.isSet("wave_generation"))         if (!applyCliOption("wave_generation", "true")) return false;
    if (parser.isSet("initial_nuclei"))          if (!applyCliOption("initial_nuclei", parser.value("initial_nuclei"))) return false;
    if (parser.isSet("wave_peak_fraction"))      if (!applyCliOption("wave_peak_fraction", parser.value("wave_peak_fraction"))) return false;
    if (parser.isSet("wave_end_fraction"))       if (!applyCliOption("wave_end_fraction", parser.value("wave_end_fraction"))) return false;
    if (parser.isSet("wave_coefficient"))        if (!applyCliOption("wave_coefficient", parser.value("wave_coefficient"))) return false;

    if (parser.isSet("material"))                if (!applyCliOption("material", parser.value("material"))) return false;
    if (parser.isSet("lattice"))                 if (!applyCliOption("lattice", parser.value("lattice"))) return false;

    if (parser.isSet("texture")) {
        if (!applyCliOption("texture", parser.value("texture"))) return false;
        if (parser.isSet("scatter")) {
            if (!applyCliOption("scatter", parser.value("scatter"))) return false;
        }
        const bool bcc = Parameters::materialLattice() == TextureLibrary::Lattice::BCC;
        qInfo() << "texture:" << parser.value("texture")
                << " lattice:" << (bcc ? "bcc" : "fcc")
                << " scatter:" << params->getTextureScatter() << "deg"
                << " components:" << Parameters::textureComponents.size();
    } else if (appliedKeys.contains("texture")) {
        if (parser.isSet("scatter")) {
            if (!applyCliOption("scatter", parser.value("scatter"))) return false;
        }
        const bool bcc = Parameters::materialLattice() == TextureLibrary::Lattice::BCC;
        qInfo() << "texture:" << params->getTexturePreset()
                << " lattice:" << (bcc ? "bcc" : "fcc")
                << " scatter:" << params->getTextureScatter() << "deg"
                << " components:" << Parameters::textureComponents.size();
    } else {
        if (parser.isSet("lattice") || parser.isSet("scatter")) {
            qWarning() << "--lattice and --scatter have no effect without --texture";
        }
        Parameters::textureComponents.clear();
    }

    if (parser.isSet("num_rnd_loads"))           if (!applyCliOption("num_rnd_loads", parser.value("num_rnd_loads"))) return false;
    if (parser.isSet("solver"))                  if (!applyCliOption("solver", parser.value("solver"))) return false;
    if (parser.isSet("stress_mode"))             if (!applyCliOption("stress_mode", parser.value("stress_mode"))) return false;
    if (parser.isSet("eps"))                     if (!applyCliOption("eps", parser.value("eps"))) return false;
    if (parser.isSet("output"))                  if (!applyCliOption("output", parser.value("output"))) return false;
    if (parser.isSet("working_directory"))       if (!applyCliOption("working_directory", parser.value("working_directory"))) return false;

    if (parser.isSet("seed")) {
        if (!applyCliOption("seed", parser.value("seed"))) return false;
    }

    if (parser.isSet("np")) {
        if (!applyCliOption("np", parser.value("np"))) return false;
    }

    // ── Load order Step 3: Default values only if not set in file or CLI ──
    if (!appliedKeys.contains("voronoi_metric_preset") &&
        appliedKeys.contains("prob_preset") &&
        params->getAlgorithm().compare("Voronoi", Qt::CaseInsensitive) == 0) {
        params->setVoronoiMetricPreset(params->getProbPreset());
    }

    if (!appliedKeys.contains("wave_coefficient")) {
        params->setWaveCoefficient(0.1f);
    }

    if (!appliedKeys.contains("seed")) {
        params->setSeed(static_cast<unsigned int>(std::time(nullptr)));
        qInfo() << "Random seed:" << params->getSeed();
    }

    if (!appliedKeys.contains("np") && !appliedKeys.contains("num_threads")) {
        int cores = CpuInfo::getPhysicalCores();
        params->setNumThreads(cores > 0 ? cores : omp_get_max_threads());
        qDebug() << "Physical CPU cores:" << params->getNumThreads();
        qInfo() << "Number of threads:" << params->getNumThreads();
    }

    return true;
}

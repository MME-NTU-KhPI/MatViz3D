#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <omp.h>

#include <cmath>
#include <ctime>
#include "algorithmfactory.h"
#include "cpuinfo.hpp"
#include "commandline_parser.h"
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

    desc += "MACHINE-READABLE SCHEMA:\n";
    desc += "  Pass --help-json to output full CLI schema, algorithms, materials, and options in JSON format.";

    return desc;
}

void Commandline_Parser::setupParser(QCommandLineParser &parser)
{
    parser.setApplicationDescription(buildApplicationDescription());
    parser.addHelpOption();
    parser.addVersionOption();

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
    addOpt("algorithm", "Grid", "string", "name", "Voronoi", "Generation algorithm name", QStringList() << "Voronoi" << "Composite" << "Probability" << "Polycrystall" << "DLCA" << "Moore" << "Neumann" << "Radial");
    addOpt("periodic", "Grid", "bool", "", "false", "Periodic boundary conditions (grains/fibers wrap across opposite cell faces)");

    // Polycrystall
    addOpt("neighborhood", "Polycrystall", "string", "stencil", "Moore (26)", "Cellular automaton neighborhood stencil",
           QStringList() << "Moore (26)" << "von Neumann (6)" << "Radial (18)");

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

namespace {

// Maps --texture to TextureLibrary::Process. Returns false on an unknown name.
bool parseProcess(const QString& name, TextureLibrary::Process& out)
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

bool parseLattice(const QString& name, TextureLibrary::Lattice& out)
{
    const QString n = name.trimmed().toLower();
    if (n == "fcc")      out = TextureLibrary::Lattice::FCC;
    else if (n == "bcc") out = TextureLibrary::Lattice::BCC;
    else return false;
    return true;
}

} // namespace

void Commandline_Parser::processOptions(const QCommandLineParser& parser)
{
    Parameters* params = Parameters::instance();

    // ── Typed parse helpers ───────────────────────────────────────────────
    // Each helper returns true on success so callers can chain or ignore.

    auto parseInt = [&](const QString& opt, auto setter) -> bool {
        if (!parser.isSet(opt)) return false;
        const QString str = parser.value(opt);
        bool ok = false;
        const int value = str.toInt(&ok);
        if (!ok) {
            qFatal("Option --%s expects an integer; got \"%s\"",
                   qPrintable(opt), qPrintable(str));
        }
        setter(value);
        qInfo() << opt << ":" << value;
        return true;
    };

    auto parseFloat = [&](const QString& opt, auto setter) -> bool {
        if (!parser.isSet(opt)) return false;
        const QString str = parser.value(opt);
        bool ok = false;
        const float value = str.toFloat(&ok);
        if (!ok) {
            qFatal("Option --%s expects a float; got \"%s\"",
                   qPrintable(opt), qPrintable(str));
        }
        setter(value);
        qInfo() << opt << ":" << value;
        return true;
    };

    auto parseDouble = [&](const QString& opt, auto setter) -> bool {
        if (!parser.isSet(opt)) return false;
        const QString str = parser.value(opt);
        bool ok = false;
        const double value = str.toDouble(&ok);
        if (!ok) {
            qFatal("Option --%s expects a double; got \"%s\"",
                   qPrintable(opt), qPrintable(str));
        }
        setter(value);
        qInfo() << opt << ":" << value;
        return true;
    };

    auto parseString = [&](const QString& opt, auto setter) -> bool {
        if (!parser.isSet(opt)) return false;
        const QString value = parser.value(opt);
        setter(value);
        qInfo() << opt << ":" << value;
        return true;
    };

    // Positive-value guard: catches --size -5 and --np 0, which used to pass
    // parsing and blow up much later.
    auto requirePositive = [&](const QString& opt, double value) {
        if (value <= 0.0)
            qFatal("Option --%s expects a positive value; got %s",
                   qPrintable(opt), qPrintable(QString::number(value)));
    };

    // ── Cube geometry ─────────────────────────────────────────────────────
    parseInt("size",   [&](int v) { requirePositive("size", v);   params->setSize(v); });
    parseInt("points", [&](int v) { requirePositive("points", v); params->setPoints(v); });

    if (parser.isSet("concentration")) {
        if (parser.isSet("points")) {
            qWarning() << "Both --points and --concentration were given;"
                       << "--concentration wins and --points is ignored";
        }
        const QString str = parser.value("concentration");
        bool ok = false;
        const float pct = str.toFloat(&ok);
        if (!ok) {
            qFatal("Option --concentration expects a float; got \"%s\"",
                   qPrintable(str));
        }
        if (pct <= 0.0f || pct > 100.0f)
            qFatal("Option --concentration expects a value in (0, 100]; got %s",
                   qPrintable(str));

        // Double, and rounded rather than truncated, to match
        // Parameters::processPointInput(): computed in float, 0.375% of 20^3
        // lands on 29.999... and truncated to 29 instead of the exact 30.
        const double volume = std::pow(static_cast<double>(params->getSize()), 3);
        const int derived = static_cast<int>(
            std::lround(static_cast<double>(pct) / 100.0 * volume));
        if (derived <= 0)
            qFatal("Option --concentration resolved to %d points; raise --size or --concentration",
                   derived);

        params->setPoints(derived);
        qInfo() << "concentration:" << pct << "% -> points:" << derived;
    }

    // points must fit inside the cube, otherwise generation misbehaves silently
    if (params->getSize() > 0 && params->getPoints() > std::pow(params->getSize(), 3)) {
        qFatal("Initial points (%d) exceed the cube volume (%.0f); lower --points or raise --size",
               params->getPoints(), std::pow(params->getSize(), 3));
    }

    // ── Probability preset & half-axes ───────────────────────────────────
    parseString("prob_preset",      [&](const QString& v) { params->setProbPreset(v); });

    // ── Ellipsoid half-axes ───────────────────────────────────────────────
    parseFloat("halfaxis_a", [&](float v) { params->setHalfAxisA(v); });
    parseFloat("halfaxis_b", [&](float v) { params->setHalfAxisB(v); });
    parseFloat("halfaxis_c", [&](float v) { params->setHalfAxisC(v); });

    params->setHasProbParameters(
        parser.isSet("halfaxis_a") ||
        parser.isSet("halfaxis_b") ||
        parser.isSet("halfaxis_c") ||
        parser.isSet("orientation_angle_a") ||
        parser.isSet("orientation_angle_b") ||
        parser.isSet("orientation_angle_c"));

    // ── Orientation angles ────────────────────────────────────────────────
    parseFloat("orientation_angle_a", [&](float v) { params->setOrientationAngleA(v); });
    parseFloat("orientation_angle_b", [&](float v) { params->setOrientationAngleB(v); });
    parseFloat("orientation_angle_c", [&](float v) { params->setOrientationAngleC(v); });

    // ── Algorithm options ─────────────────────────────────────────────────
    parseDouble("ellipse_order",    [&](double v) { params->setEllipseOrder(v); });
    parseFloat("stefan_number",     [&](float v)  { params->setStefanNumber(v); });
    parseDouble("minkowski_p",      [&](double v) {
        if (v <= 0.0)
            qFatal("Option --minkowski_p expects a positive value; got %s",
                   qPrintable(QString::number(v)));
        params->setMinkowskiP(v);
    });
    params->setIsPeriodic(parser.isSet("periodic"));

    parseString("voronoi_metric_preset", [&](const QString& v) { params->setVoronoiMetricPreset(v); });
    if (!parser.isSet("voronoi_metric_preset") && parser.isSet("prob_preset") &&
        parser.value("algorithm").compare("Voronoi", Qt::CaseInsensitive) == 0) {
        params->setVoronoiMetricPreset(parser.value("prob_preset"));
    }

    parseDouble("voronoi_mxx", [&](double v) { params->setVoronoiMxx(v); });
    parseDouble("voronoi_myy", [&](double v) { params->setVoronoiMyy(v); });
    parseDouble("voronoi_mzz", [&](double v) { params->setVoronoiMzz(v); });
    parseDouble("voronoi_mxy", [&](double v) { params->setVoronoiMxy(v); });
    parseDouble("voronoi_myz", [&](double v) { params->setVoronoiMyz(v); });
    parseDouble("voronoi_mxz", [&](double v) { params->setVoronoiMxz(v); });

    if (parser.isSet("voronoi_metric")) {
        const QStringList parts = parser.value("voronoi_metric").split(',');
        if (parts.size() >= 3) {
            bool ok1 = false, ok2 = false, ok3 = false;
            double mxx = parts[0].trimmed().toDouble(&ok1);
            double myy = parts[1].trimmed().toDouble(&ok2);
            double mzz = parts[2].trimmed().toDouble(&ok3);
            if (ok1 && ok2 && ok3) {
                params->setVoronoiMxx(mxx);
                params->setVoronoiMyy(myy);
                params->setVoronoiMzz(mzz);
            }
            if (parts.size() >= 6) {
                bool ok4 = false, ok5 = false, ok6 = false;
                double mxy = parts[3].trimmed().toDouble(&ok4);
                double myz = parts[4].trimmed().toDouble(&ok5);
                double mxz = parts[5].trimmed().toDouble(&ok6);
                if (ok4 && ok5 && ok6) {
                    params->setVoronoiMxy(mxy);
                    params->setVoronoiMyz(myz);
                    params->setVoronoiMxz(mxz);
                }
            }
        }
    }

    // ── Composite (fiber-reinforced RVE) ──────────────────────────────────
    parseString("composite_dim", [&](const QString& v) {
        const QString n = v.trimmed().toLower();
        if (!n.startsWith('1') && !n.startsWith('2') && !n.startsWith('3'))
            qFatal("Option --composite_dim expects 1d, 2d or 3d; got \"%s\"", qPrintable(v));
        params->setCompositeDim(v);
    });
    parseString("composite_packing", [&](const QString& v) {
        const QString n = v.trimmed().toLower();
        if (n != "square" && n != "hexagonal" && n != "hex")
            qFatal("Option --composite_packing expects square or hexagonal; got \"%s\"",
                   qPrintable(v));
        params->setCompositePacking(v);
    });
    parseDouble("fiber_volume_fraction", [&](double v) {
        if (v <= 0.0 || v >= 1.0)
            qFatal("Option --fiber_volume_fraction expects a value in (0, 1); got %s",
                   qPrintable(QString::number(v)));
        params->setFiberVolumeFraction(v);
    });
    parseInt("fibers_per_row", [&](int v) {
        requirePositive("fibers_per_row", v);
        params->setFibersPerRow(v);
    });
    parseDouble("fiber_aspect_ratio", [&](double v) {
        if (v <= 0.0)
            qFatal("Option --fiber_aspect_ratio expects a positive value; got %s",
                   qPrintable(QString::number(v)));
        params->setFiberAspectRatio(v);
    });
    parseDouble("fiber_angle_scatter", [&](double v) {
        if (v < 0.0 || v > 180.0)
            qFatal("Option --fiber_angle_scatter expects degrees in [0, 180]; got %s",
                   qPrintable(QString::number(v)));
        params->setFiberAngleScatter(v);
    });
    parseDouble("fiber_center_jitter", [&](double v) {
        if (v < 0.0 || v > 1.0)
            qFatal("Option --fiber_center_jitter expects a value in [0, 1]; got %s",
                   qPrintable(QString::number(v)));
        params->setFiberCenterJitter(v);
    });
    params->setFiberAllowOverlap(parser.isSet("fiber_allow_overlap"));
    parseString("matrix_material", [&](const QString& v) { params->setMatrixMaterial(v); });
    parseString("fiber_material",  [&](const QString& v) { params->setFiberMaterial(v); });

    if (parser.isSet("animate"))
        params->setIsAnimation(true);
    if (parser.isSet("wave_generation"))
        params->setIsWaveGeneration(true);
    parseInt   ("initial_nuclei",      [&](int    v) { params->setInitialNucleiCount(v); });
    parseFloat ("wave_peak_fraction",  [&](float  v) { params->setWavePeakFraction(v); });
    parseFloat ("wave_end_fraction",   [&](float  v) { params->setWaveEndFraction(v); });
    parseFloat ("wave_coefficient",    [&](float  v) { params->setWaveCoefficient(v); });

    if (!parser.isSet("wave_coefficient"))
        params->setWaveCoefficient(0.1f);

    if (parser.isSet("prob_matrix_mode")) {
        const QString m = parser.value("prob_matrix_mode").trimmed().toLower();
        if (m == "surface" || m == "surface flux" || m == "surface_flux")
            params->setProbMatrixMode("Surface Flux");
        else
            params->setProbMatrixMode("Volume Sampling");
    }

    parseString("neighborhood",              [&](const QString& v) { params->setPolycrystallNeighborhood(v); });
    parseString("polycrystall_neighborhood", [&](const QString& v) { params->setPolycrystallNeighborhood(v); });
    parseString("algorithm",                 [&](const QString& v) { params->setAlgorithm(v); });

    // ── RNG seed ──────────────────────────────────────────────────────────
    // toUInt, not toInt: the seed is unsigned and values above 2^31-1 are legal.
    if (parser.isSet("seed")) {
        bool ok = false;
        const unsigned int seed = parser.value("seed").toUInt(&ok);
        if (!ok)
            qFatal("Option --seed expects a non-negative integer; got \"%s\"",
                   qPrintable(parser.value("seed")));
        params->setSeed(seed);
    } else {
        params->setSeed(static_cast<unsigned int>(std::time(nullptr)));
    }
    qInfo() << "Random seed:" << params->getSeed();

    // ── Threading ─────────────────────────────────────────────────────────
    if (parser.isSet("np")) {
        bool ok = false;
        const int np = parser.value("np").toInt(&ok);
        if (!ok) qFatal("Option --np expects an integer");
        requirePositive("np", np);
        params->setNumThreads(np);
    } else {
        int cores = CpuInfo::getPhysicalCores();
        params->setNumThreads(cores > 0 ? cores : omp_get_max_threads());
        qDebug() << "Physical CPU cores:" << params->getNumThreads();
    }
    qInfo() << "Number of threads:" << params->getNumThreads();

    // ── Material ──────────────────────────────────────────────────────────
    // Before the texture block: the material's Type column decides which
    // lattice the presets are built for, unless --lattice overrides it.
    parseString("material", [&](const QString& v) { params->setDbMaterial(v); });

    if (parser.isSet("lattice")) {
        TextureLibrary::Lattice lat;
        if (!parseLattice(parser.value("lattice"), lat))
            qFatal("Option --lattice expects fcc or bcc; got \"%s\"",
                   qPrintable(parser.value("lattice")));
        params->setLatticeOverride(parser.value("lattice").trimmed().toLower());
    }

    // ── Crystallographic texture ──────────────────────────────────────────
    // Fills the same Parameters::textureComponents that the Texture Editor writes,
    // so ansysWrapper and the viewport pick it up through the usual path.
    if (parser.isSet("texture")) {
        TextureLibrary::Process proc;
        if (!parseProcess(parser.value("texture"), proc)) {
            qFatal("Option --texture expects one of: random, extrusion, rolling, "
                   "recrystallization, shear, scattered_cube; got \"%s\"",
                   qPrintable(parser.value("texture")));
        }

        if (parser.isSet("scatter")) {
            bool ok = false;
            const double scatter = parser.value("scatter").toDouble(&ok);
            if (!ok || scatter < 0.0)
                qFatal("Option --scatter expects a non-negative number of degrees; got \"%s\"",
                       qPrintable(parser.value("scatter")));
            params->setTextureScatter(scatter);
        }

        // Routed through Parameters (rather than writing textureComponents
        // directly) so the GUI panel shows the preset the CLI selected.
        params->setTexturePreset(parser.value("texture"));

        const bool bcc = Parameters::materialLattice() == TextureLibrary::Lattice::BCC;
        qInfo() << "texture:" << parser.value("texture")
                << " lattice:" << (bcc ? "bcc" : "fcc")
                << " scatter:" << params->getTextureScatter() << "deg"
                << " components:" << Parameters::textureComponents.size();
    } else {
        if (parser.isSet("lattice") || parser.isSet("scatter"))
            qWarning() << "--lattice and --scatter have no effect without --texture";
        Parameters::textureComponents.clear();
    }

    if (parser.isSet("num_rnd_loads")) {
        bool ok = false;
        const unsigned int n = parser.value("num_rnd_loads").toUInt(&ok);
        if (!ok)
            qFatal("Option --num_rnd_loads expects a non-negative integer; got \"%s\"",
                   qPrintable(parser.value("num_rnd_loads")));
        params->setNumRndLoads(n);
        qInfo() << "num_rnd_loads :" << n;
    }

    if (parser.isSet("solver")) {
        const QString s = parser.value("solver").trimmed().toLower();
        if (s != "ansys" && s != "fft")
            qFatal("Option --solver expects ansys or fft; got \"%s\"",
                   qPrintable(parser.value("solver")));
        params->setStressSolver(s);
        qInfo() << "solver :" << s;
    }

    if (parser.isSet("stress_mode")) {
        const QString m = parser.value("stress_mode").trimmed().toLower();
        if (m != "single" && m != "dataset" && m != "stiffness")
            qFatal("Option --stress_mode expects single, dataset or stiffness; got \"%s\"",
                   qPrintable(parser.value("stress_mode")));
        params->setStressMode(m);
        qInfo() << "stress_mode :" << m;
    }

    if (parser.isSet("eps")) {
        const QStringList parts = parser.value("eps").split(',');
        if (parts.size() != 6)
            qFatal("Option --eps expects 6 comma-separated values, got %d", static_cast<int>(parts.size()));
        double e[6];
        for (int i = 0; i < 6; ++i) {
            bool ok = false;
            e[i] = parts[i].trimmed().toDouble(&ok);
            if (!ok)
                qFatal("Option --eps: component %d is not a number: \"%s\"",
                       i + 1, qPrintable(parts[i]));
        }
        params->setStressEps(e);
    }

    parseString("output", [&](const QString& v) {
        params->setFilename(v);
    });

    parseString("working_directory", [&](const QString& v) {
        params->setWorkingDirectory(v);
    });
}

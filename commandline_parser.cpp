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
#include "latticefamily.h"
#include <QRegularExpression>
#include "cpuinfo.hpp"
#include "commandline_parser.h"
#include "mycelium.h"
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
    desc += "  5. Lattice Metamaterial Cell (periodic sheet gyroid, effective stiffness):\n";
    desc += "       MatViz3D --nogui --autostart --algorithm Lattice --lattice_family gyroid_sheet --size 48 --cells_per_edge 2 --relative_density 0.3 --material PETG --run_stress_calc --solver fft --stress_mode stiffness --output gyroid.hdf5\n\n";
    desc += "  6. Lattice Dataset Cell (one unit cell, material as numbers, printability gate, Willot scheme, no per-voxel fields):\n";
    desc += "       MatViz3D --nogui --autostart --algorithm Lattice --lattice_family gyroid_sheet --size 64 --cells_per_edge 1 --relative_density 0.3 --lattice_cell_mm 12 --material_iso 2.0,0.38 --require_printable --no_fields --run_stress_calc --solver fft --stress_mode stiffness --fft_discretization rotated --fft_tol 1e-5 --output cell.hdf5\n\n";

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
    desc += "  --algorithm:             Voronoi | Composite | Probability | Polycrystall | DLCA | Lattice\n";
    desc += "  --neighborhood:          'Moore (26)' | 'von Neumann (6)' | 'Radial (18)'\n";
    desc += "  --solver:                fft (in-memory Moulinec-Suquet) | ansys (external APDL FEM)\n";
    desc += "  --stress_mode:           stiffness (fast 6 solves -> S, C, moduli) | single (--eps required) | dataset (full 300 loads)\n";
    desc += "  --fft_scheme:            auto | basic | cg  (FFT iterative scheme; auto = CG for porous or high-contrast cells)\n";
    desc += "  --fft_tol:               FFT convergence tolerance (default: 1e-5); --fft_max_iter: iteration cap per load (default: 1000)\n";
    desc += "  --fft_discretization:    continuous (Moulinec-Suquet, default) | rotated (Willot 2015: converges for true pores, cleaner fields at walls)\n";
    desc += "  --fft_void_eta:          stiffness of the void phase as a fraction of the solid C11 (default: 1e-4). Very soft cells need 1e-6..1e-8:\n"
            "                           the fictitious void stiffness otherwise carries part of the result (see solid_energy_share in the output)\n";
    desc += "  --composite_dim:         1d (fibers along Z) | 2d (along X, Y) | 3d (along X, Y, Z)\n";
    desc += "  --composite_packing:     square | hexagonal\n";
    {
        // Generated from the family table so the list can never fall behind it;
        // wrapped under the option column.
        const QString indent(27, QLatin1Char(' '));
        QString line = QStringLiteral("  --lattice_family:        ");
        const QStringList keys = latticeFamilyKeys();
        for (int i = 0; i < keys.size(); ++i) {
            const QString item = keys[i] + (i + 1 < keys.size() ? QStringLiteral(" |") : QString());
            if (line.size() + 1 + item.size() > 110 && line.size() > indent.size()) {
                desc += line + QLatin1Char('\n');
                line = indent + item;
            } else {
                line += (line.endsWith(QLatin1Char(' ')) ? QString() : QStringLiteral(" ")) + item;
            }
        }
        desc += line + QLatin1Char('\n');
    }
    desc += "  --cells_per_edge:        unit cells along each cube edge (default: 2)\n";
    desc += "  --relative_density:      target solid fraction of the lattice in (0, 1) (default: 0.3)\n";
    desc += "  --lattice_grading:       linear wall-thickness gradient along Z, 0..0.9 (default: 0)\n";
    desc += "  --lattice_jitter:        random wall-thickness modulation, 0..1 (default: 0)\n";
    desc += "  --spinodoid_theta_x/y/z: spinodoid cone half-angles about the axes, 0 (off) or 5..90 deg (default: 90)\n";
    desc += "  --fourier_terms/kmin/kmax/decay: random Fourier-series draw (default: 24 waves, |k| in [1, 4] per edge, decay 1)\n";
    desc += "  --fourier_file:          text file with one wave per line (kx ky kz amplitude phase); overrides the random draw\n";
    desc += "  --noise_smooth:          smoothed-noise families: Gaussian sigma as a fraction of the cube edge, 0.005..0.5 (default: 0.06)\n";
    desc += "  --fourier_symmetry:      none | cubic -- expand every Fourier wave to its orbit under the cubic group (default: none)\n";
    desc += "  --plate_hole:            plate lattices: hole diameter through every panel centre, in cell edges, 0..0.9 (default: 0)\n";
    desc += "  --lattice_union:         second cell family united with the first (plates + struts, ...); none = off\n";
    desc += "  --lattice_subtract / --lattice_subtract_size: family carved out of the solid up to that level in cell edges (default: none / 0.3)\n";
    desc += "  --lattice_smooth:        Gaussian blur of the field before the level is solved, sigma in cell edges, 0..0.3 (default: 0 = off)\n";
    desc += "  --honeycomb_angle / --honeycomb_rib: honeycomb shape, wall angle in degrees (-50..50, < 0 auxetic) and rib / wall length (0.2..5); setting either overrides the family preset\n";
    desc += "  --assembly_struts/plates/shells/holes: cubic_assembly members drawn in the asymmetric unit (0..12 / 0..12 / 0..4 / 0..8;\n"
            "                           default 3 / 2 / 0 / 1), repeated by the 48 cube symmetries; --assembly_spread: thickness\n"
            "                           spread between members 0..0.8 (default 0.3); --assembly_file: one member per line\n"
            "                           (kind x0 y0 z0 x1 y1 z1 x2 y2 z2 radius weight), overrides the draw\n";
    desc += "  --foam_mode:             foam families (3D honeycombs): walls | windows | edges -- closed cells, a window in every\n"
            "                           face, or the edges only (open cells) (default: walls); --foam_frame: frame around a window\n"
            "                           beyond half the wall thickness, bubble sizes 0.01..0.5 (default 0.1); --foam_bubbles: foam_random sites per cell 1..64\n"
            "                           (default 8); --foam_file: foam_random sites, one x y z per line in cell units\n";
    desc += "  --mycelium_mode:         mycelium: substrate | polypore | mat | layered -- foraging hyphae, skeletal + binding hyphae\n"
            "                           of a bracket fungus, two layers joined by anchors, or growth on a substrate rich in the\n"
            "                           mid plane (default: substrate); --mycelium_length, _spores, _unit, _angle, _wiggle, _fuse,\n"
            "                           _taper, _binding, _align, _anchors, _poor: growth settings; --mycelium_file: one piece per line\n"
            "                           (ax ay az bx by bz weight, cell units), overrides the growth\n";
    desc += "  --form_a/b/c:            bio-inspired / architected forms (neovius_sheet ... hierarchical_gyroid): three shape\n"
            "                           knobs 0..1 whose meaning depends on the form, e.g. glass_sponge diagonal offset /\n"
            "                           thickness / horizontal bracing (default 0.5 each)\n";
    desc += "  --screenshot <file>:     with --nogui --autostart: a picture of the structure, rendered without a window (.png | .pdf);\n"
            "                           --dpi (300), --background theme | white | transparent (white), --width_cm (0 = view size),\n"
            "                           --view_size WxH (1200x900), --outlines off | boundaries | voxels, --pdf_vector,\n"
            "                           --cell_frame (the cell's edges as thin grey lines)\n";
    desc += "  --browse <path>:         GUI: open the Dataset Browser on a dataset folder (manifest.csv + cells/), its manifest or a cell file\n";
    desc += "  --select <targets>:      GUI: open the Selector tab and search the dataset of --browse, e.g. E=150,nu=0.3,mat_e=2.0 (moduli in MPa)\n";
    desc += "  --neural <seed>:         GUI: open the Neural tab and generate the cell of this seed (0 = a new seed): the diffusion network\n"
            "                           when models/cell_diffusion.bin is found, else a random code\n";
    desc += "  --neural_compare:        GUI: with --neural, solve the generated cell the way the dataset was solved and list the asked\n"
            "                           properties next to the obtained ones (takes minutes; --np sets the threads)\n";
    desc += "  --material_iso E,nu:     solid material as numbers instead of a database row: Young's modulus in GPa and Poisson's ratio\n";
    desc += "  --material_ti E,nu,r:    the same, transversely isotropic about the build axis Z with E_z = r*E, G_xz = G_yz = r*G\n";
    desc += "  --no_fields:             do not store per-voxel stress/strain fields in HDF5 (matrices and metadata only; dataset runs)\n";
    desc += "  --lattice_cell_mm:       physical size of one cell in mm (default: 10); sets the voxel size for the printability check\n";
    desc += "  --lattice_uniform_wall:  sheet families: thicken by |f|/|grad f| so the wall thickness is uniform (printable random sheets)\n";
    desc += "  --print_min_wall_mm / --print_max_overhang_deg: printability thresholds (default: 0.8 mm, 45 deg)\n";
    desc += "  --print_max_thin_fraction / --print_max_island_vox / --print_max_unsupported_fraction: tolerated thin fraction, largest floating island in voxels, unsupported fraction (default: 0.02 / 8 / 0.05)\n";
    desc += "  --print_max_components:  separate solid bodies tolerated, 0 = no limit (default: 1 — a stack of slabs is not one part)\n";
    desc += "  --print_min_directions:  independent directions the solid must join its periodic neighbours in, 0 = no check (default: 3 — a slab joins in 2, a rod in 1)\n";
    desc += "  --require_printable:     headless: stop before the stress solve and exit with code 3 if the structure fails the check\n";
    desc += "  --remove_islands:        Lattice: remove every floating island (any size) from the cell before the printability check and the solve\n";
    desc += "  --lattice_metric_y/z:    Lattice thickness metric, 0.25..4 (default 1): a distance along Y / Z counts this many times, so members\n"
            "                           measured along that axis come out thinner (struts, plates, cubic assembly, uniform-wall sheets)\n";
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
    parser.addOption(QCommandLineOption(QStringList() << "seed-method" << "seed_method",
        "[Grid] Placement of the initial crystallization seeds: 'random' | 'sobol' (default: random).", "method"));
    parser.addOption(QCommandLineOption(QStringList() << "seed-sigma" << "seed_sigma",
        "[Grid] Sobol seed dispersion sigma >= 0: 0 = most uniform grains, 1 ~ random (default: 0.3).", "sigma"));
    parser.addOption(QCommandLineOption("output",
        "[Output] Output HDF5 file path for structure and fields (default: current_ls.hdf5).", "file"));
    parser.addOption(QCommandLineOption("csv",
                                        "Write the generated voxel grid to a CSV file (X;Y;Z;Color).", "file"));
    parser.addOption(QCommandLineOption("screenshot",
        "[Output] With --nogui --autostart: write a picture of the structure, rendered without a window: .png or .pdf.", "file"));
    parser.addOption(QCommandLineOption("dpi",
        "[Output] Resolution of --screenshot in DPI (default: 300).", "dpi"));
    parser.addOption(QCommandLineOption("background",
        "[Output] Background of --screenshot: theme | white | transparent (default: white).", "kind"));
    parser.addOption(QCommandLineOption("width_cm",
        "[Output] Width of --screenshot in cm at its DPI; 0 = the view size (default: 0).", "cm"));
    parser.addOption(QCommandLineOption("view_size",
        "[Output] Size of the virtual 3D view of --screenshot in logical pixels, WxH (default: 1200x900).", "WxH"));
    parser.addOption(QCommandLineOption("outlines",
        "[Output] Outlines in --screenshot: off | boundaries | voxels (default: off).", "kind"));
    parser.addOption(QCommandLineOption("pdf_vector",
        "[Output] --screenshot to .pdf: the model as vector polygons instead of an image."));
    parser.addOption(QCommandLineOption("cell_frame",
        "[Output] --screenshot: draw the twelve edges of the cell as thin grey lines."));

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

    // ── Lattice (cellular metamaterial RVE) ───────────────────────────────
    parser.addOption(QCommandLineOption("lattice_family",
        "[Lattice] Unit-cell family: gyroid_sheet | gyroid_skeletal | schwarz_p_sheet | schwarz_p_skeletal | "
        "schwarz_d_sheet | schwarz_d_skeletal | iwp_sheet | iwp_skeletal | simple_cubic | bcc | fcc | octet | "
        "honeycomb | reentrant | spinodoid | fourier_sheet | fourier_skeletal | noise_sheet | noise_skeletal. Default: gyroid_sheet", "family"));
    parser.addOption(QCommandLineOption("cells_per_edge",
        "[Lattice] Unit cells along each cube edge, 1..64 (default: 2).", "n"));
    parser.addOption(QCommandLineOption("relative_density",
        "[Lattice] Target solid fraction in (0, 1); the level that produces it is solved. Default: 0.3", "frac"));
    parser.addOption(QCommandLineOption("lattice_grading",
        "[Lattice] Linear wall-thickness gradient along Z in [0, 0.9): (1-g) at the bottom, (1+g) at the top. Default: 0", "g"));
    parser.addOption(QCommandLineOption("lattice_jitter",
        "[Lattice] Smooth random wall-thickness modulation of relative amplitude [0, 1], seeded. Default: 0", "j"));
    parser.addOption(QCommandLineOption("spinodoid_theta_x",
        "[Lattice] Spinodoid: half-angle of the cone of wave directions about X, 0 (closed) or 5..90 deg. Default: 90", "deg"));
    parser.addOption(QCommandLineOption("spinodoid_theta_y",
        "[Lattice] Spinodoid: half-angle of the cone of wave directions about Y, 0 (closed) or 5..90 deg. Default: 90", "deg"));
    parser.addOption(QCommandLineOption("spinodoid_theta_z",
        "[Lattice] Spinodoid: half-angle of the cone of wave directions about Z, 0 (closed) or 5..90 deg. Default: 90", "deg"));
    parser.addOption(QCommandLineOption("fourier_terms",
        "[Lattice] Fourier series: number of random waves, 2..256 (default: 24).", "n"));
    parser.addOption(QCommandLineOption("fourier_kmin",
        "[Lattice] Fourier series: smallest wave number in waves per cube edge (default: 1).", "k"));
    parser.addOption(QCommandLineOption("fourier_kmax",
        "[Lattice] Fourier series: largest wave number in waves per cube edge (default: 4).", "k"));
    parser.addOption(QCommandLineOption("fourier_decay",
        "[Lattice] Fourier series: amplitude ~ |k|^-decay, 0..4 (default: 1).", "p"));
    parser.addOption(QCommandLineOption("browse",
        "[GUI] Open the Dataset Browser on a dataset folder (manifest.csv + cells/), its manifest or one cell file.", "path"));
    parser.addOption(QCommandLineOption("select",
        "[GUI] Open the Selector tab and search the dataset of --browse: comma-separated key=value, keys are the targets "
        "(E, G, nu, K, nuMin, Ex, ..., EzEx, EmaxEmin, AU, zener; moduli in MPa), mat_e (GPa), mat_nu, mat_r, "
        "rho_min, rho_max, axes=1 (per-axis targets) and result (row to show, from 0).", "targets"));
    parser.addOption(QCommandLineOption("neural",
        "[GUI] Open the Neural tab and generate the cell of this seed (0 = a new seed): the diffusion network when "
        "models/cell_diffusion.bin is found, else a random code.", "seed"));
    parser.addOption(QCommandLineOption("neural_compare",
        "[GUI] With --neural: solve the generated cell the way the dataset was solved and list the asked properties "
        "next to the obtained ones (takes minutes; --np sets the threads)."));
    parser.addOption(QCommandLineOption("fourier_file",
        "[Lattice] Fourier series: text file with one wave per line (kx ky kz amplitude phase); overrides the random draw.", "path"));
    parser.addOption(QCommandLineOption("material_iso",
        "[Material] Solid material as numbers instead of a database row: 'E_GPa,nu' (isotropic).", "E,nu"));
    parser.addOption(QCommandLineOption("material_ti",
        "[Material] Solid material as numbers, transversely isotropic about the build axis Z: 'E_GPa,nu,r' with E_z = r*E, G_xz = G_yz = r*G.", "E,nu,r"));
    parser.addOption(QCommandLineOption("no_fields",
        "[Output] Do not store per-voxel stress/strain fields in HDF5; keep matrices, voxels and metadata (dataset runs)."));
    parser.addOption(QCommandLineOption("noise_smooth",
        "[Lattice] Smoothed noise: Gaussian sigma of the blur as a fraction of the cube edge, 0.005..0.5 (default: 0.06).", "frac"));
    parser.addOption(QCommandLineOption("lattice_uniform_wall",
        "[Lattice] Sheet families: thicken the surface by the first-order distance |f|/|grad f| instead of |f|, "
        "so the wall thickness is nearly uniform. Recommended for random Fourier sheets."));
    parser.addOption(QCommandLineOption("fourier_symmetry",
        "[Lattice] Fourier families: none | cubic. cubic replaces every wave by its orbit under the 48 symmetries "
        "of the cube, so the random cell has cubic symmetry (default: none).", "mode"));
    parser.addOption(QCommandLineOption("plate_hole",
        "[Lattice] Plate lattices: diameter of the circular hole through every panel centre, in cell edges, 0..0.9 (default: 0).", "frac"));
    parser.addOption(QCommandLineOption("lattice_union",
        "[Lattice] Second cell family united with the first, one shared level (e.g. plate_sc_fcc + octet); "
        "not a random family, not two strut/honeycomb families (default: none).", "family"));
    parser.addOption(QCommandLineOption("lattice_subtract",
        "[Lattice] Family carved out of the solid (e.g. spheres_bcc for spherical windows, cylinders_axes for channels); "
        "not a random family (default: none).", "family"));
    parser.addOption(QCommandLineOption("lattice_subtract_size",
        "[Lattice] Level of the carved-out family in cell edges: sphere / cylinder radius, plate half-thickness (default: 0.3).", "frac"));
    parser.addOption(QCommandLineOption("lattice_smooth",
        "[Lattice] Gaussian blur of the field before the level is solved, sigma in cell edges, 0..0.3: rounded corners and "
        "blended junctions (default: 0 = off).", "frac"));
    parser.addOption(QCommandLineOption("honeycomb_angle",
        "[Lattice] Honeycombs: inclined wall angle to the horizontal in degrees, -50..50; 30 = regular hexagon, 0 = rectangles, "
        "negative = re-entrant (auxetic). Overrides the family preset.", "deg"));
    parser.addOption(QCommandLineOption("assembly_struts",
        "[Lattice] Cubic assembly: random struts drawn in the asymmetric unit of the cell, 0..12 (default: 3).", "n"));
    parser.addOption(QCommandLineOption("assembly_plates",
        "[Lattice] Cubic assembly: random triangular plates, 0..12 (default: 2).", "n"));
    parser.addOption(QCommandLineOption("assembly_shells",
        "[Lattice] Cubic assembly: spherical shells about special positions, 0..4 (default: 0).", "n"));
    parser.addOption(QCommandLineOption("assembly_holes",
        "[Lattice] Cubic assembly: spherical windows carved out of the plates and shells, 0..8 (default: 1).", "n"));
    parser.addOption(QCommandLineOption("assembly_spread",
        "[Lattice] Cubic assembly: thickness weights of the members drawn in [1 - s, 1 + s], 0..0.8 (default: 0.3).", "s"));
    parser.addOption(QCommandLineOption("assembly_file",
        "[Lattice] Cubic assembly: text file with one member per line, kind (strut|plate|shell|hole) x0 y0 z0 x1 y1 z1 "
        "x2 y2 z2 radius weight, points in the asymmetric unit 0 <= z <= y <= x <= 1/2; overrides the draw.", "path"));
    parser.addOption(QCommandLineOption("foam_mode",
        "[Lattice] Foam families: walls (closed cells), windows (a window in every face) or edges (open cells) (default: walls).", "mode"));
    parser.addOption(QCommandLineOption("foam_frame",
        "[Lattice] Foam windows: frame left around every window beyond half the wall thickness, in bubble sizes, 0.01..0.5 (default: 0.1).", "frac"));
    parser.addOption(QCommandLineOption("foam_bubbles",
        "[Lattice] foam_random: Voronoi sites (bubbles) per cell, drawn from the seed with a hard core, 1..64 (default: 8).", "n"));
    parser.addOption(QCommandLineOption("foam_file",
        "[Lattice] foam_random: text file with one site per line, x y z in cell units; overrides the draw.", "path"));
    parser.addOption(QCommandLineOption("mycelium_mode",
        "[Lattice] Mycelium: substrate (foraging hyphae), polypore (skeletal + binding hyphae), mat (layers + anchors) or "
        "layered (substrate rich in the mid plane) (default: substrate).", "mode"));
    parser.addOption(QCommandLineOption("mycelium_length",
        "[Lattice] Mycelium: total hypha length per cell, cell edges, 2..80 (default: 22).", "value"));
    parser.addOption(QCommandLineOption("mycelium_spores",
        "[Lattice] Mycelium: spores (polypore: skeletal hyphae, mat: spores per layer), 0..40; 0 = the mode's default 4 / 10 / 3 / 4.", "n"));
    parser.addOption(QCommandLineOption("mycelium_unit",
        "[Lattice] Mycelium: hyphal growth unit, hypha length per branch on rich substrate, cell edges, 0.05..2 (default: 0.25).", "value"));
    parser.addOption(QCommandLineOption("mycelium_angle",
        "[Lattice] Mycelium: branch angle to the parent hypha, degrees, 10..90 (default: 60).", "value"));
    parser.addOption(QCommandLineOption("mycelium_wiggle",
        "[Lattice] Mycelium: random turning of the tips, 0 (straight) .. 1 (default: 0.25).", "value"));
    parser.addOption(QCommandLineOption("mycelium_fuse",
        "[Lattice] Mycelium: a tip this close to another hypha fuses with it, cell edges, 0..0.2; 0 grows a tree (default: 0.05).", "value"));
    parser.addOption(QCommandLineOption("mycelium_taper",
        "[Lattice] Mycelium: thickness of a branch over its parent's, 0.3..1 (default: 0.75).", "value"));
    parser.addOption(QCommandLineOption("mycelium_binding",
        "[Lattice] Mycelium polypore: share of the length in binding hyphae, 0..0.8 (default: 0.36).", "value"));
    parser.addOption(QCommandLineOption("mycelium_align",
        "[Lattice] Mycelium polypore: lean of the skeletal hyphae to Z, 0 (random) .. 1 (all along Z) (default: 0).", "value"));
    parser.addOption(QCommandLineOption("mycelium_anchors",
        "[Lattice] Mycelium mat: share of the branches that grow into vertical anchors, 0..1 (default: 0.25).", "value"));
    parser.addOption(QCommandLineOption("mycelium_poor",
        "[Lattice] Mycelium layered: substrate level at z = 0 (1 in the mid plane), 0.05..1 (default: 0.25).", "value"));
    parser.addOption(QCommandLineOption("mycelium_file",
        "[Lattice] Mycelium: text file with one piece of hypha per line, ax ay az bx by bz [weight] in cell units; overrides the growth.", "path"));
    for (const char* k : { "form_a", "form_b", "form_c" })
        parser.addOption(QCommandLineOption(QString::fromLatin1(k),
            "[Lattice] Bio-inspired / architected forms: shape knob 0..1, its meaning depends on the form (default: 0.5).", "value"));
    parser.addOption(QCommandLineOption("honeycomb_rib",
        "[Lattice] Honeycombs: vertical rib length over inclined wall length, 0.2..5 (1 regular, 2 the re-entrant preset). "
        "Overrides the family preset.", "ratio"));
    parser.addOption(QCommandLineOption("honeycomb_custom",
        "[Lattice] Honeycombs: use honeycomb_angle / honeycomb_rib instead of the family preset (true|false).", "bool"));
    parser.addOption(QCommandLineOption("lattice_cell_mm",
        "[Lattice] Physical size of one unit cell in mm (default: 10). Voxel size = cells_per_edge * cell_mm / size.", "mm"));
    parser.addOption(QCommandLineOption("print_min_wall_mm",
        "[Printability] Thinnest printable wall in mm, typically two nozzle diameters (default: 0.8).", "mm"));
    parser.addOption(QCommandLineOption("print_max_overhang_deg",
        "[Printability] Steepest overhang the printer handles without support, degrees from vertical (default: 45).", "deg"));
    parser.addOption(QCommandLineOption("print_max_thin_fraction",
        "[Printability] Tolerated fraction of solid thinner than the minimum wall (default: 0.02).", "frac"));
    parser.addOption(QCommandLineOption("print_max_island_vox",
        "[Printability] Largest floating island tolerated, in voxels; anything bigger fails (default: 8, a 2x2x2 speck).", "n"));
    parser.addOption(QCommandLineOption("print_max_components",
        "[Printability] Separate solid bodies tolerated; 0 = no limit (default: 1, one connected part).", "n"));
    parser.addOption(QCommandLineOption("print_min_directions",
        "[Printability] Independent directions in which the solid must join its periodic neighbours, 0..3; 0 = no check (default: 3; a slab joins in 2, a rod in 1).", "n"));
    parser.addOption(QCommandLineOption("print_max_unsupported_fraction",
        "[Printability] Tolerated fraction of unsupported solid along the best build direction (default: 0.05).", "frac"));
    parser.addOption(QCommandLineOption("require_printable",
        "[Printability] Headless: skip the stress solve and exit with code 3 when the structure fails the check."));
    parser.addOption(QCommandLineOption("remove_islands",
        "[Lattice] Remove every floating island (a solid piece that does not join the body, any size) from the rasterized cell "
        "before the printability check and the stress solve; the achieved density is that of the cleaned cell."));
    parser.addOption(QCommandLineOption("lattice_metric_y",
        "[Lattice] Thickness metric along Y, 0.25..4 (default 1): a distance along Y counts this many times (struts, plates, cubic assembly, "
        "uniform-wall sheets), so members measured along Y come out thinner and the cell stiffer along Y.", "factor"));
    parser.addOption(QCommandLineOption("lattice_metric_z",
        "[Lattice] Thickness metric along Z, 0.25..4 (default 1): as lattice_metric_y for Z (a cubic cell becomes tetragonal about Z).", "factor"));

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
    parser.addOption(QCommandLineOption("fft_scheme",
        "[Stress] FFT iterative scheme: auto (CG when the cell has void or a stiffness contrast above 100, "
        "basic otherwise) | basic (Moulinec-Suquet fixed point) | cg (conjugate gradients, Galerkin FFT). Default: auto", "scheme"));
    parser.addOption(QCommandLineOption("fft_tol",
        "[Stress] FFT convergence tolerance: equilibrium error (basic) or relative residual (cg). Default: 1e-5", "tol"));
    parser.addOption(QCommandLineOption("fft_max_iter",
        "[Stress] FFT iteration cap per load case (default: 1000).", "n"));
    parser.addOption(QCommandLineOption("fft_discretization",
        "[Stress] Green operator discretization: continuous (Moulinec-Suquet, default) | rotated (Willot 2015, robust for pores). "
        "Aliases: ms, willot.", "disc"));
    parser.addOption(QCommandLineOption("fft_void_eta",
        "[Stress] Void phase stiffness as a fraction of the solid C11 (default: 1e-4; 1e-6..1e-8 for very soft cells).", "eta"));
    parser.addOption(QCommandLineOption("eps",
        "[Stress] Prescribed macroscopic strain tensor for '--stress_mode single': exx,eyy,ezz,exy,eyz,exz", "strains"));
    parser.addOption(QCommandLineOption("num_rnd_loads",
        "[Stress] Number of random loads for Hill yield criterion fit in dataset mode (default: 150).", "count"));
    parser.addOption(QCommandLineOption("local_criterion",
        "[Stress] Local yield criterion for dataset yield points: auto | schmid | vonmises. Default: auto (per algorithm)", "criterion"));
    parser.addOption(QCommandLineOption("hill_space",
        "[Stress] Hill fit space: auto | deviatoric5d | full6d (keeps the hydrostatic axis). Default: auto (per algorithm)", "space"));
    parser.addOption(QCommandLineOption("yield_stress",
        "[Stress] Von Mises yield threshold in Pa for --local_criterion vonmises (default: 120e6).", "Pa"));
    parser.addOption(QCommandLineOption("yield_percentile",
        "[Stress] Percentile of the local stress taken as critical at yield: auto | (0,1], 1 = maximum. Default: auto (DLCA 1.0, others 0.98)", "fraction"));
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
    modes["lattice_homogenization"] = "MatViz3D --nogui --autostart --algorithm Lattice --lattice_family gyroid_sheet --size 48 --cells_per_edge 2 --relative_density 0.3 --material PETG --run_stress_calc --solver fft --stress_mode stiffness --output gyroid.hdf5";
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
    addRecipe("Lattice Metamaterial Cell",
              "Periodic sheet gyroid at 30 % density in PETG, effective stiffness by FFT",
              "MatViz3D --nogui --autostart --algorithm Lattice --lattice_family gyroid_sheet --size 48 --cells_per_edge 2 --relative_density 0.3 --material PETG --run_stress_calc --solver fft --stress_mode stiffness --output gyroid.hdf5");
    addRecipe("Lattice Dataset Cell",
              "One 12 mm unit cell (sheet gyroid, wall 6 voxels at 64^3), material as numbers, exit code 3 when not printable, Willot discretization, no per-voxel fields",
              "MatViz3D --nogui --autostart --algorithm Lattice --lattice_family gyroid_sheet --size 64 --cells_per_edge 1 --relative_density 0.3 --lattice_cell_mm 12 --material_iso 2.0,0.38 --require_printable --no_fields --run_stress_calc --solver fft --stress_mode stiffness --fft_discretization rotated --fft_tol 1e-5 --output cell.hdf5");
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
    addOpt("seed-method", "Grid", "string", "method", "random", "Placement of the initial crystallization seeds",
           QStringList() << "random" << "sobol");
    addOpt("seed-sigma", "Grid", "double", "sigma", "0.3", "Sobol seed dispersion: 0 = most uniform grains, 1 ~ random");
    addOpt("output", "Output", "string", "file", "current_ls.hdf5", "Output HDF5 filepath for structure and fields");
    addOpt("csv", "Output", "string", "file", "", "CSV export of the voxel grid (X;Y;Z;Color)");
    addOpt("screenshot", "Output", "string", "file", "", "With --nogui --autostart: picture of the structure rendered without a window (.png or .pdf)");
    addOpt("dpi", "Output", "int", "dpi", "300", "Resolution of --screenshot");
    addOpt("background", "Output", "string", "kind", "white", "Background of --screenshot",
           QStringList() << "theme" << "white" << "transparent");
    addOpt("width_cm", "Output", "double", "cm", "0", "Width of --screenshot in cm at its DPI (0 = the view size)");
    addOpt("view_size", "Output", "string", "WxH", "1200x900", "Size of the virtual 3D view of --screenshot, logical pixels");
    addOpt("outlines", "Output", "string", "kind", "off", "Outlines in --screenshot",
           QStringList() << "off" << "boundaries" << "voxels");
    addOpt("pdf_vector", "Output", "bool", "", "false", "--screenshot to .pdf: the model as vector polygons");
    addOpt("cell_frame", "Output", "bool", "", "false", "--screenshot: the twelve edges of the cell as thin grey lines");

    // Grid Geometry & Nucleation
    addOpt("size", "Grid", "int", "n", "", "Voxel grid dimension N for N x N x N cell (integer > 0)");
    addOpt("points", "Grid", "int", "count", "", "Number of initial nucleation seeds / grains (integer > 0)");
    addOpt("concentration", "Grid", "float", "pct", "", "Nucleation seed density as volume percentage in (0, 100]; overrides --points");
    addOpt("algorithm", "Grid", "string", "name", "Voronoi", "Generation algorithm name", QStringList() << "Voronoi" << "Composite" << "Probability" << "Polycrystall" << "DLCA" << "Lattice" << "Moore" << "Neumann" << "Radial" << "Thin Layer");
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

    // Lattice
    addOpt("lattice_family", "Lattice", "string", "family", "gyroid_sheet", "Unit-cell family of the cellular metamaterial RVE",
           latticeFamilyKeys());
    addOpt("cells_per_edge", "Lattice", "int", "n", "2", "Unit cells along each cube edge (1..64)");
    addOpt("relative_density", "Lattice", "double", "frac", "0.3", "Target solid fraction in (0, 1)");
    addOpt("lattice_grading", "Lattice", "double", "g", "0.0", "Linear wall-thickness gradient along Z in [0, 0.9)");
    addOpt("lattice_jitter", "Lattice", "double", "j", "0.0", "Random wall-thickness modulation, relative amplitude in [0, 1]");
    addOpt("spinodoid_theta_x", "Lattice", "double", "deg", "90", "Spinodoid: cone half-angle about X, 0 (closed) or 5..90 deg");
    addOpt("spinodoid_theta_y", "Lattice", "double", "deg", "90", "Spinodoid: cone half-angle about Y, 0 (closed) or 5..90 deg");
    addOpt("spinodoid_theta_z", "Lattice", "double", "deg", "90", "Spinodoid: cone half-angle about Z, 0 (closed) or 5..90 deg");
    addOpt("fourier_terms", "Lattice", "int", "n", "24", "Fourier series: number of random waves (2..256)");
    addOpt("fourier_kmin", "Lattice", "double", "k", "1", "Fourier series: smallest wave number, waves per cube edge");
    addOpt("fourier_kmax", "Lattice", "double", "k", "4", "Fourier series: largest wave number, waves per cube edge");
    addOpt("fourier_decay", "Lattice", "double", "p", "1", "Fourier series: amplitude ~ |k|^-decay (0..4)");
    addOpt("fourier_file", "Lattice", "string", "path", "", "Fourier series: text file with one wave per line (kx ky kz amplitude phase)");
    addOpt("noise_smooth", "Lattice", "double", "frac", "0.06", "Smoothed noise: Gaussian sigma as a fraction of the cube edge (0.005..0.5)");
    addOpt("fourier_symmetry", "Lattice", "string", "mode", "none", "Fourier families: none | cubic (waves expanded to their cubic orbits)");
    addOpt("plate_hole", "Lattice", "double", "frac", "0", "Plate lattices: hole diameter through every panel centre, cell edges (0..0.9)");
    addOpt("lattice_union", "Lattice", "string", "family", "none", "Second cell family united with the first (none = off)");
    addOpt("lattice_subtract", "Lattice", "string", "family", "none", "Family carved out of the solid (none = off)");
    addOpt("lattice_subtract_size", "Lattice", "double", "frac", "0.3", "Level of the carved-out family, cell edges (0.01..1)");
    addOpt("lattice_smooth", "Lattice", "double", "frac", "0", "Gaussian blur of the field, sigma in cell edges (0..0.3)");
    addOpt("honeycomb_custom", "Lattice", "bool", "", "false", "Honeycombs: use honeycomb_angle / honeycomb_rib instead of the preset");
    addOpt("honeycomb_angle", "Lattice", "double", "deg", "30", "Honeycombs: inclined wall angle, degrees (-50..50, < 0 auxetic)");
    addOpt("honeycomb_rib", "Lattice", "double", "ratio", "1", "Honeycombs: rib length / wall length (0.2..5)");
    addOpt("assembly_struts", "Lattice", "int", "n", "3", "Cubic assembly: random struts in the asymmetric unit (0..12)");
    addOpt("assembly_plates", "Lattice", "int", "n", "2", "Cubic assembly: random triangular plates (0..12)");
    addOpt("assembly_shells", "Lattice", "int", "n", "0", "Cubic assembly: spherical shells about special positions (0..4)");
    addOpt("assembly_holes", "Lattice", "int", "n", "1", "Cubic assembly: spherical windows carved out of plates and shells (0..8)");
    addOpt("assembly_spread", "Lattice", "double", "s", "0.3", "Cubic assembly: member thickness weights in [1 - s, 1 + s] (0..0.8)");
    addOpt("assembly_file", "Lattice", "string", "path", "", "Cubic assembly: one member per line (kind x0 y0 z0 x1 y1 z1 x2 y2 z2 radius weight)");
    addOpt("foam_mode", "Lattice", "string", "mode", "walls", "Foam families: walls | windows | edges (closed, windowed or open cells)");
    addOpt("foam_frame", "Lattice", "double", "frac", "0.1", "Foam windows: frame around every window beyond half the wall, bubble sizes (0.01..0.5)");
    addOpt("foam_bubbles", "Lattice", "int", "n", "8", "foam_random: Voronoi sites per cell (1..64)");
    addOpt("foam_file", "Lattice", "string", "path", "", "foam_random: one site per line, x y z in cell units");
    addOpt("mycelium_mode", "Lattice", "string", "mode", "substrate", "Mycelium: substrate | polypore | mat | layered");
    addOpt("mycelium_length", "Lattice", "double", "value", "22", "Mycelium: total hypha length per cell, cell edges (2..80)");
    addOpt("mycelium_spores", "Lattice", "int", "n", "0", "Mycelium: spores / skeletal hyphae / spores per layer, 0 = mode default (0..40)");
    addOpt("mycelium_unit", "Lattice", "double", "value", "0.25", "Mycelium: hypha length per branch on rich substrate, cell edges (0.05..2)");
    addOpt("mycelium_angle", "Lattice", "double", "value", "60", "Mycelium: branch angle, degrees (10..90)");
    addOpt("mycelium_wiggle", "Lattice", "double", "value", "0.25", "Mycelium: random turning of the tips (0..1)");
    addOpt("mycelium_fuse", "Lattice", "double", "value", "0.05", "Mycelium: fusion distance, cell edges, 0 = a tree (0..0.2)");
    addOpt("mycelium_taper", "Lattice", "double", "value", "0.75", "Mycelium: branch / parent thickness (0.3..1)");
    addOpt("mycelium_binding", "Lattice", "double", "value", "0.36", "Mycelium polypore: share of the length in binding hyphae (0..0.8)");
    addOpt("mycelium_align", "Lattice", "double", "value", "0", "Mycelium polypore: lean of the skeletal hyphae to Z, 1 = all along Z (0..1)");
    addOpt("mycelium_anchors", "Lattice", "double", "value", "0.25", "Mycelium mat: share of the branches that become anchors (0..1)");
    addOpt("mycelium_poor", "Lattice", "double", "value", "0.25", "Mycelium layered: substrate at z = 0, 1 in the mid plane (0.05..1)");
    addOpt("mycelium_file", "Lattice", "string", "path", "", "Mycelium: one piece per line (ax ay az bx by bz weight, cell units)");
    addOpt("form_a", "Lattice", "double", "value", "0.5", "Forms (glass_sponge, radiolarian, ...): first shape knob, meaning per form (0..1)");
    addOpt("form_b", "Lattice", "double", "value", "0.5", "Forms: second shape knob, meaning per form (0..1)");
    addOpt("form_c", "Lattice", "double", "value", "0.5", "Forms: third shape knob, meaning per form (0..1)");
    addOpt("browse", "GUI", "string", "path", "", "Open the Dataset Browser on a dataset folder, its manifest.csv or a cell file");
    addOpt("select", "GUI", "string", "targets", "", "Open the Selector tab and search the dataset of --browse for these targets, e.g. E=150,nu=0.3,mat_e=2.0 (moduli in MPa)");
    addOpt("neural", "GUI", "int", "seed", "", "Open the Neural tab and generate the cell of this seed (0 = a new seed): the diffusion network when models/cell_diffusion.bin is found, else a random code");
    addOpt("neural_compare", "GUI", "bool", "", "false", "With --neural: solve the generated cell the way the dataset was solved and list the asked properties next to the obtained ones (takes minutes; --np sets the threads)");
    addOpt("material_iso", "Material", "string", "E,nu", "", "Solid material as numbers: Young's modulus in GPa and Poisson's ratio (isotropic)");
    addOpt("material_ti", "Material", "string", "E,nu,r", "", "Solid material as numbers, transversely isotropic about Z: E_z = r*E, G_xz = G_yz = r*G");
    addOpt("no_fields", "Output", "bool", "", "false", "Do not store per-voxel stress/strain fields in HDF5");
    addOpt("lattice_cell_mm", "Lattice", "double", "mm", "10", "Physical size of one unit cell in mm");
    addOpt("lattice_uniform_wall", "Lattice", "bool", "", "false", "Sheet families: uniform wall thickness via |f|/|grad f|");
    addOpt("print_min_wall_mm", "Printability", "double", "mm", "0.8", "Thinnest printable wall in mm");
    addOpt("print_max_overhang_deg", "Printability", "double", "deg", "45", "Steepest unsupported overhang in degrees");
    addOpt("print_max_thin_fraction", "Printability", "double", "frac", "0.02", "Tolerated fraction of solid thinner than the minimum wall");
    addOpt("print_max_island_vox", "Printability", "int", "n", "8", "Largest floating island tolerated, voxels");
    addOpt("print_max_components", "Printability", "int", "n", "1", "Separate solid bodies tolerated, 0 = no limit");
    addOpt("print_min_directions", "Printability", "int", "n", "3", "Independent directions the solid must join its periodic neighbours in (slab 2, rod 1), 0 = no check");
    addOpt("print_max_unsupported_fraction", "Printability", "double", "frac", "0.05", "Tolerated unsupported fraction along the best build direction");
    addOpt("require_printable", "Printability", "bool", "", "false", "Headless: skip the stress solve and exit 3 when the structure is not printable");
    addOpt("remove_islands", "Lattice", "bool", "", "false", "Remove every floating island (any size) before the printability check and the solve");
    addOpt("lattice_metric_y", "Lattice", "double", "factor", "1", "Thickness metric along Y (0.25..4): a distance along Y counts this many times; struts, plates, cubic assembly, uniform-wall sheets");
    addOpt("lattice_metric_z", "Lattice", "double", "factor", "1", "Thickness metric along Z (0.25..4): a distance along Z counts this many times; struts, plates, cubic assembly, uniform-wall sheets");

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
    addOpt("fft_scheme", "Stress", "string", "scheme", "auto", "FFT iterative scheme: auto (CG for porous / high-contrast cells) | basic (Moulinec-Suquet fixed point) | cg (conjugate gradients)", QStringList() << "auto" << "basic" << "cg");
    addOpt("fft_tol", "Stress", "double", "tol", "1e-5", "FFT convergence tolerance (equilibrium error for basic, relative residual for cg)");
    addOpt("fft_void_eta", "Stress", "double", "eta", "1e-4", "Void phase stiffness as a fraction of the solid C11");
    addOpt("fft_discretization", "Stress", "string", "disc", "continuous", "Green operator discretization: continuous (Moulinec-Suquet) | rotated (Willot 2015, robust for pores)", QStringList() << "continuous" << "rotated");
    addOpt("fft_max_iter", "Stress", "int", "n", "1000", "FFT iteration cap per load case");
    addOpt("eps", "Stress", "string", "strains", "", "Applied strain tensor for --stress_mode single: exx,eyy,ezz,exy,eyz,exz");
    addOpt("num_rnd_loads", "Stress", "uint", "count", "150", "Number of random loads for Hill yield criterion fit in dataset mode");
    addOpt("local_criterion", "Stress", "string", "criterion", "auto", "Local yield criterion for dataset yield points (auto: per algorithm)", QStringList() << "auto" << "schmid" << "vonmises");
    addOpt("hill_space", "Stress", "string", "space", "auto", "Hill fit space; full6d keeps the hydrostatic axis (auto: per algorithm)", QStringList() << "auto" << "deviatoric5d" << "full6d");
    addOpt("yield_stress", "Stress", "double", "Pa", "120e6", "Von Mises yield threshold in Pa for local_criterion vonmises");
    addOpt("yield_percentile", "Stress", "string", "fraction", "auto", "Percentile of the local stress taken as critical at yield: auto or (0,1], 1 = maximum (auto: DLCA 1.0, others 0.98)");
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

bool Commandline_Parser::isValidLatticeFamily(const QString& v)
{
    LatticeFamily f;
    return parseLatticeFamily(v, f);
}

bool Commandline_Parser::isValidFftScheme(const QString& v)
{
    const QString m = v.trimmed().toLower();
    return m == "auto" || m == "basic" || m == "cg";
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

    if (normKey == "local_criterion") {
        const QString v = value.trimmed().toLower();
        if (v != "auto" && v != "schmid" && v != "vonmises") {
            return setErr(QString("Option local_criterion expects auto, schmid or vonmises; got \"%1\"").arg(value));
        }
        Parameters::local_criterion = v;
        return true;
    }

    if (normKey == "hill_space") {
        const QString v = value.trimmed().toLower();
        if (v != "auto" && v != "deviatoric5d" && v != "full6d") {
            return setErr(QString("Option hill_space expects auto, deviatoric5d or full6d; got \"%1\"").arg(value));
        }
        Parameters::hill_space = v;
        return true;
    }

    if (normKey == "yield_stress") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        Parameters::yield_stress = v;
        return true;
    }

    if (normKey == "yield_percentile") {
        if (value.trimmed().toLower() == "auto") {
            Parameters::yield_percentile = 0.0;
            return true;
        }
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (!(v > 0.0 && v <= 1.0)) {
            return setErr(QString("Option yield_percentile expects auto or a value in (0, 1]; got \"%1\"").arg(value));
        }
        Parameters::yield_percentile = v;
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

    if (normKey == "csv") {
        Parameters::csv_output = value.trimmed();
        return true;
    }

    if (normKey == "seed") {
        unsigned int v = 0;
        if (!parseUInt(v)) return false;
        params->setSeed(v);
        return true;
    }

    if (normKey == "seed_method") {
        const QString v = value.trimmed().toLower();
        if (v == "random") {
            params->setSeedMethod(Parameters::Random);
        } else if (v == "sobol") {
            params->setSeedMethod(Parameters::Sobol);
        } else {
            return setErr(QString("Option seed_method expects 'random' or 'sobol'; got \"%1\"").arg(value));
        }
        return true;
    }

    if (normKey == "seed_sigma") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0.0) {
            return setErr(QString("Option seed_sigma expects a value >= 0; got %1").arg(value));
        }
        params->setSeedSigma(v);
        return true;
    }

    if (normKey == "np" || normKey == "num_threads") {
        int v = 0;
        if (!parseInt(v, true)) return false;
        params->setNumThreads(v);
        return true;
    }

    if (normKey == "fft_scheme") {
        if (!isValidFftScheme(value)) {
            return setErr(QString("Option fft_scheme expects auto, basic or cg; got \"%1\"").arg(value));
        }
        params->setFftScheme(value);
        return true;
    }

    if (normKey == "fft_tol") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        if (v >= 1.0) {
            return setErr(QString("Option fft_tol expects a value in (0, 1); got %1").arg(value));
        }
        params->setFftTol(v);
        return true;
    }
    if (normKey == "fft_discretization") {
        const QString v = value.trimmed().toLower();
        if (v != "continuous" && v != "rotated" && v != "ms" && v != "willot" && v != "moulinec-suquet") {
            return setErr(QString("Option fft_discretization expects continuous (ms) or rotated (willot); got \"%1\"").arg(value));
        }
        params->setFftDiscretization(v);
        return true;
    }
    if (normKey == "fft_void_eta") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        if (v >= 1.0) {
            return setErr(QString("Option fft_void_eta expects a value in (0, 1); got %1").arg(value));
        }
        params->setFftVoidEta(v);
        return true;
    }

    if (normKey == "fft_max_iter") {
        int v = 0;
        if (!parseInt(v, true)) return false;
        params->setFftMaxIter(v);
        return true;
    }

    if (normKey == "lattice_family") {
        if (!isValidLatticeFamily(value)) {
            return setErr(QString("Option lattice_family expects one of %1; got \"%2\"")
                              .arg(latticeFamilyKeys().join(", "), value));
        }
        params->setLatticeFamily(value);
        return true;
    }

    if (normKey == "cells_per_edge") {
        int v = 0;
        if (!parseInt(v, true)) return false;
        if (v > 64) {
            return setErr(QString("Option cells_per_edge expects 1..64; got %1").arg(v));
        }
        params->setCellsPerEdge(v);
        return true;
    }

    if (normKey == "relative_density") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v <= 0.0 || v >= 1.0) {
            return setErr(QString("Option relative_density expects a value in (0, 1); got %1").arg(value));
        }
        params->setRelativeDensity(v);
        return true;
    }

    if (normKey == "lattice_grading") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0.0 || v >= 0.9) {
            return setErr(QString("Option lattice_grading expects a value in [0, 0.9); got %1").arg(value));
        }
        params->setLatticeGrading(v);
        return true;
    }

    if (normKey == "lattice_jitter") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0.0 || v > 1.0) {
            return setErr(QString("Option lattice_jitter expects a value in [0, 1]; got %1").arg(value));
        }
        params->setLatticeJitter(v);
        return true;
    }

    if (normKey == "spinodoid_theta_x" || normKey == "spinodoid_theta_y" || normKey == "spinodoid_theta_z") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (!(v == 0.0 || (v >= 5.0 && v <= 90.0))) {
            return setErr(QString("Option %1 expects 0 or a value in [5, 90] degrees; got %2").arg(normKey, value));
        }
        if (normKey.endsWith('x'))      params->setSpinodoidThetaX(v);
        else if (normKey.endsWith('y')) params->setSpinodoidThetaY(v);
        else                            params->setSpinodoidThetaZ(v);
        return true;
    }

    if (normKey == "fourier_terms") {
        int v = 0;
        if (!parseInt(v, true)) return false;
        if (v < 2 || v > 256) {
            return setErr(QString("Option fourier_terms expects 2..256; got %1").arg(v));
        }
        params->setFourierTerms(v);
        return true;
    }

    if (normKey == "fourier_kmin" || normKey == "fourier_kmax") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        if (v < 1.0) {
            return setErr(QString("Option %1 expects a value >= 1; got %2").arg(normKey, value));
        }
        if (normKey == "fourier_kmin") params->setFourierKmin(v); else params->setFourierKmax(v);
        return true;
    }

    if (normKey == "fourier_decay") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0.0 || v > 4.0) {
            return setErr(QString("Option fourier_decay expects a value in [0, 4]; got %1").arg(value));
        }
        params->setFourierDecay(v);
        return true;
    }

    if (normKey == "fourier_file") {
        if (value.trimmed().isEmpty()) {
            return setErr(QStringLiteral("Option fourier_file expects a path"));
        }
        params->setFourierFile(value);
        return true;
    }

    if (normKey == "lattice_cell_mm" || normKey == "print_min_wall_mm") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        if (normKey == "lattice_cell_mm") params->setLatticeCellMm(v); else params->setPrintMinWallMm(v);
        return true;
    }

    if (normKey == "print_max_overhang_deg") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0.0 || v > 89.0) {
            return setErr(QString("Option print_max_overhang_deg expects 0..89; got %1").arg(value));
        }
        params->setPrintMaxOverhangDeg(v);
        return true;
    }

    if (normKey == "print_max_thin_fraction" || normKey == "print_max_unsupported_fraction") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0.0 || v > 1.0) {
            return setErr(QString("Option %1 expects a value in [0, 1]; got %2").arg(normKey, value));
        }
        if (normKey == "print_max_thin_fraction") params->setPrintMaxThinFraction(v);
        else                                       params->setPrintMaxUnsupportedFraction(v);
        return true;
    }

    if (normKey == "print_max_island_vox") {
        int v = 0;
        if (!parseInt(v)) return false;
        if (v < 0) {
            return setErr(QString("Option print_max_island_vox expects a non-negative voxel count; got %1").arg(v));
        }
        params->setPrintMaxIslandVox(v);
        return true;
    }

    if (normKey == "print_max_components") {
        int v = 0;
        if (!parseInt(v)) return false;
        if (v < 0) {
            return setErr(QString("Option print_max_components expects a non-negative count; got %1").arg(v));
        }
        params->setPrintMaxComponents(v);
        return true;
    }

    if (normKey == "print_min_directions") {
        int v = 0;
        if (!parseInt(v)) return false;
        if (v < 0 || v > 3) {
            return setErr(QString("Option print_min_directions expects 0..3; got %1").arg(v));
        }
        params->setPrintMinDirections(v);
        return true;
    }

    if (normKey == "material_iso" || normKey == "material_ti") {
        const QStringList parts = value.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
        const int need = normKey == "material_iso" ? 2 : 3;
        if (parts.size() != need) {
            return setErr(QString("Option %1 expects %2 comma-separated numbers; got \"%3\"").arg(normKey).arg(need).arg(value));
        }
        bool okE = false, okN = false, okR = true;
        const double E  = parts[0].toDouble(&okE);
        const double nu = parts[1].toDouble(&okN);
        const double r  = need == 3 ? parts[2].toDouble(&okR) : 1.0;
        if (!okE || !okN || !okR || !(E > 0.0) || !(nu > -1.0 && nu < 0.5) || !(r >= 0.1 && r <= 3.0)) {
            return setErr(QString("Option %1: E must be > 0 GPa, nu in (-1, 0.5), r in [0.1, 3]; got \"%2\"").arg(normKey, value));
        }
        params->setMatEGpa(E);
        params->setMatNu(nu);
        params->setMatAniso(r);
        return true;
    }

    if (normKey == "mat_e_gpa" || normKey == "mat_nu" || normKey == "mat_aniso") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (normKey == "mat_e_gpa")      params->setMatEGpa(v);
        else if (normKey == "mat_nu")    params->setMatNu(v);
        else                              params->setMatAniso(v);
        return true;
    }

    if (normKey == "no_fields" || normKey == "hdf5_fields") {
        bool b = false;
        if (!parseBool(b)) return false;
        params->setHdf5Fields(normKey == "no_fields" ? !b : b);
        return true;
    }

    if (normKey == "fourier_symmetry") {
        const QString v = value.trimmed().toLower();
        if (v != "none" && v != "cubic") {
            return setErr(QString("Option fourier_symmetry expects none or cubic; got \"%1\"").arg(value));
        }
        params->setFourierSymmetry(v);
        return true;
    }
    if (normKey == "plate_hole") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        if (v < 0.0 || v > 0.9) {
            return setErr(QString("Option plate_hole expects a value in [0, 0.9]; got %1").arg(value));
        }
        params->setPlateHole(v);
        return true;
    }
    if (normKey == "lattice_union") {
        const QString v = value.trimmed();
        if (!v.isEmpty() && v.compare("none", Qt::CaseInsensitive) != 0 && !isValidLatticeFamily(v)) {
            return setErr(QString("Option lattice_union expects none or one of %1; got \"%2\"")
                              .arg(latticeFamilyKeys().join(", "), value));
        }
        params->setLatticeUnion(v);
        return true;
    }
    if (normKey == "lattice_subtract") {
        const QString v = value.trimmed();
        if (!v.isEmpty() && v.compare("none", Qt::CaseInsensitive) != 0 && !isValidLatticeFamily(v)) {
            return setErr(QString("Option lattice_subtract expects none or one of %1; got \"%2\"")
                              .arg(latticeFamilyKeys().join(", "), value));
        }
        params->setLatticeSubtract(v);
        return true;
    }
    if (normKey == "lattice_subtract_size") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        if (v < 0.01 || v > 1.0) {
            return setErr(QString("Option lattice_subtract_size expects a value in [0.01, 1]; got %1").arg(value));
        }
        params->setLatticeSubtractSize(v);
        return true;
    }
    if (normKey == "honeycomb_custom") {
        bool v = false;
        if (!parseBool(v)) return false;
        params->setHoneycombCustom(v);
        return true;
    }
    if (normKey == "honeycomb_angle") {
        double v = 0.0;
        if (!parseDouble(v, false)) return false;          // zero and negative angles are valid
        if (v < -50.0 || v > 50.0) {
            return setErr(QString("Option honeycomb_angle expects a value in [-50, 50] degrees; got %1").arg(value));
        }
        params->setHoneycombAngle(v);
        params->setHoneycombCustom(true);
        return true;
    }
    if (normKey == "assembly_struts" || normKey == "assembly_plates" || normKey == "assembly_shells" || normKey == "assembly_holes") {
        int v = 0;
        if (!parseInt(v)) return false;
        const int hi = normKey == "assembly_shells" ? 4 : (normKey == "assembly_holes" ? 8 : 12);
        if (v < 0 || v > hi) {
            return setErr(QString("Option %1 expects 0..%2; got %3").arg(normKey).arg(hi).arg(v));
        }
        if (normKey == "assembly_struts")      params->setAssemblyStruts(v);
        else if (normKey == "assembly_plates") params->setAssemblyPlates(v);
        else if (normKey == "assembly_shells") params->setAssemblyShells(v);
        else                                   params->setAssemblyHoles(v);
        return true;
    }
    if (normKey == "assembly_spread") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        if (v < 0.0 || v > 0.8) {
            return setErr(QString("Option assembly_spread expects a value in [0, 0.8]; got %1").arg(value));
        }
        params->setAssemblySpread(v);
        return true;
    }
    if (normKey == "assembly_file") {
        if (value.trimmed().isEmpty()) {
            return setErr(QStringLiteral("Option assembly_file expects a path"));
        }
        params->setAssemblyFile(value);
        return true;
    }
    if (normKey == "foam_mode") {
        const QString v = value.trimmed().toLower();
        if (v != QLatin1String("walls") && v != QLatin1String("windows") && v != QLatin1String("edges")) {
            return setErr(QString("Option foam_mode expects walls, windows or edges; got \"%1\"").arg(value));
        }
        params->setFoamMode(v);
        return true;
    }
    if (normKey == "foam_frame") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        if (v < 0.01 || v > 0.5) {
            return setErr(QString("Option foam_frame expects a value in [0.01, 0.5]; got %1").arg(value));
        }
        params->setFoamFrame(v);
        return true;
    }
    if (normKey == "foam_bubbles") {
        int v = 0;
        if (!parseInt(v)) return false;
        if (v < 1 || v > 64) {
            return setErr(QString("Option foam_bubbles expects 1..64; got %1").arg(v));
        }
        params->setFoamBubbles(v);
        return true;
    }
    if (normKey == "foam_file") {
        if (value.trimmed().isEmpty()) {
            return setErr(QStringLiteral("Option foam_file expects a path"));
        }
        params->setFoamFile(value);
        return true;
    }
    if (normKey == "mycelium_mode") {
        const QString v = value.trimmed().toLower();
        mycelium::Mode m;
        if (!mycelium::parseMode(v.toLatin1().constData(), m)) {
            return setErr(QString("Option mycelium_mode expects substrate, polypore, mat or layered; got \"%1\"").arg(value));
        }
        params->setMyceliumMode(v);
        return true;
    }
    if (normKey == "mycelium_length") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 2 || v > 80) {
            return setErr(QString("Option mycelium_length expects a value in [2, 80]; got %1").arg(value));
        }
        params->setMyceliumLength(v);
        return true;
    }
    if (normKey == "mycelium_spores") {
        int v = 0;
        if (!parseInt(v)) return false;
        if (v < 0 || v > 40) {
            return setErr(QString("Option mycelium_spores expects 0..40; got %1").arg(v));
        }
        params->setMyceliumSpores(v);
        return true;
    }
    if (normKey == "mycelium_unit") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0.05 || v > 2) {
            return setErr(QString("Option mycelium_unit expects a value in [0.05, 2]; got %1").arg(value));
        }
        params->setMyceliumUnit(v);
        return true;
    }
    if (normKey == "mycelium_angle") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 10 || v > 90) {
            return setErr(QString("Option mycelium_angle expects a value in [10, 90]; got %1").arg(value));
        }
        params->setMyceliumAngle(v);
        return true;
    }
    if (normKey == "mycelium_wiggle") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0 || v > 1) {
            return setErr(QString("Option mycelium_wiggle expects a value in [0, 1]; got %1").arg(value));
        }
        params->setMyceliumWiggle(v);
        return true;
    }
    if (normKey == "mycelium_fuse") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0 || v > 0.2) {
            return setErr(QString("Option mycelium_fuse expects a value in [0, 0.2]; got %1").arg(value));
        }
        params->setMyceliumFuse(v);
        return true;
    }
    if (normKey == "mycelium_taper") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0.3 || v > 1) {
            return setErr(QString("Option mycelium_taper expects a value in [0.3, 1]; got %1").arg(value));
        }
        params->setMyceliumTaper(v);
        return true;
    }
    if (normKey == "mycelium_binding") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0 || v > 0.8) {
            return setErr(QString("Option mycelium_binding expects a value in [0, 0.8]; got %1").arg(value));
        }
        params->setMyceliumBinding(v);
        return true;
    }
    if (normKey == "mycelium_align") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0 || v > 1) {
            return setErr(QString("Option mycelium_align expects a value in [0, 1]; got %1").arg(value));
        }
        params->setMyceliumAlign(v);
        return true;
    }
    if (normKey == "mycelium_anchors") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0 || v > 1) {
            return setErr(QString("Option mycelium_anchors expects a value in [0, 1]; got %1").arg(value));
        }
        params->setMyceliumAnchors(v);
        return true;
    }
    if (normKey == "mycelium_poor") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0.05 || v > 1) {
            return setErr(QString("Option mycelium_poor expects a value in [0.05, 1]; got %1").arg(value));
        }
        params->setMyceliumPoor(v);
        return true;
    }
    if (normKey == "mycelium_file") {
        if (value.trimmed().isEmpty()) {
            return setErr(QStringLiteral("Option mycelium_file expects a path"));
        }
        params->setMyceliumFile(value);
        return true;
    }
    if (normKey == "form_a" || normKey == "form_b" || normKey == "form_c") {
        double v = 0.0;
        if (!parseDouble(v)) return false;
        if (v < 0 || v > 1) {
            return setErr(QString("Option %1 expects a value in [0, 1]; got %2").arg(normKey, value));
        }
        if (normKey == "form_a") params->setFormA(v);
        else if (normKey == "form_b") params->setFormB(v);
        else params->setFormC(v);
        return true;
    }
    if (normKey == "honeycomb_rib") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        if (v < 0.2 || v > 5.0) {
            return setErr(QString("Option honeycomb_rib expects a value in [0.2, 5]; got %1").arg(value));
        }
        params->setHoneycombRib(v);
        params->setHoneycombCustom(true);
        return true;
    }
    if (normKey == "lattice_smooth") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        if (v < 0.0 || v > 0.3) {
            return setErr(QString("Option lattice_smooth expects a value in [0, 0.3]; got %1").arg(value));
        }
        params->setLatticeSmooth(v);
        return true;
    }
    if (normKey == "noise_smooth") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        if (v < 0.005 || v > 0.5) {
            return setErr(QString("Option noise_smooth expects a value in [0.005, 0.5]; got %1").arg(value));
        }
        params->setNoiseSmooth(v);
        return true;
    }

    if (normKey == "lattice_uniform_wall") {
        bool b = false;
        if (!parseBool(b)) return false;
        params->setLatticeUniformWall(b);
        return true;
    }

    if (normKey == "require_printable") {
        bool b = false;
        if (!parseBool(b)) return false;
        params->setRequirePrintable(b);
        return true;
    }

    if (normKey == "remove_islands") {
        bool b = false;
        if (!parseBool(b)) return false;
        params->setLatticeRemoveIslands(b);
        return true;
    }
    if (normKey == "lattice_metric_y" || normKey == "lattice_metric_z") {
        double v = 0.0;
        if (!parseDouble(v, true)) return false;
        if (v < 0.25 || v > 4.0) return setErr(QString("Option %1 expects a value in [0.25, 4]; got %2").arg(normKey, value));
        if (normKey == "lattice_metric_y") params->setLatticeMetricY(v); else params->setLatticeMetricZ(v);
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

    if (parser.isSet("lattice_family"))          if (!applyCliOption("lattice_family", parser.value("lattice_family"))) return false;
    if (parser.isSet("cells_per_edge"))          if (!applyCliOption("cells_per_edge", parser.value("cells_per_edge"))) return false;
    if (parser.isSet("relative_density"))        if (!applyCliOption("relative_density", parser.value("relative_density"))) return false;
    if (parser.isSet("lattice_grading"))         if (!applyCliOption("lattice_grading", parser.value("lattice_grading"))) return false;
    if (parser.isSet("lattice_jitter"))          if (!applyCliOption("lattice_jitter", parser.value("lattice_jitter"))) return false;
    if (parser.isSet("spinodoid_theta_x"))       if (!applyCliOption("spinodoid_theta_x", parser.value("spinodoid_theta_x"))) return false;
    if (parser.isSet("spinodoid_theta_y"))       if (!applyCliOption("spinodoid_theta_y", parser.value("spinodoid_theta_y"))) return false;
    if (parser.isSet("spinodoid_theta_z"))       if (!applyCliOption("spinodoid_theta_z", parser.value("spinodoid_theta_z"))) return false;
    if (parser.isSet("fourier_terms"))           if (!applyCliOption("fourier_terms", parser.value("fourier_terms"))) return false;
    if (parser.isSet("fourier_kmin"))            if (!applyCliOption("fourier_kmin", parser.value("fourier_kmin"))) return false;
    if (parser.isSet("fourier_kmax"))            if (!applyCliOption("fourier_kmax", parser.value("fourier_kmax"))) return false;
    if (parser.isSet("fourier_decay"))           if (!applyCliOption("fourier_decay", parser.value("fourier_decay"))) return false;
    if (parser.isSet("fourier_file"))            if (!applyCliOption("fourier_file", parser.value("fourier_file"))) return false;
    if (parser.isSet("lattice_cell_mm"))         if (!applyCliOption("lattice_cell_mm", parser.value("lattice_cell_mm"))) return false;
    if (parser.isSet("noise_smooth"))            if (!applyCliOption("noise_smooth", parser.value("noise_smooth"))) return false;
    if (parser.isSet("fourier_symmetry"))        if (!applyCliOption("fourier_symmetry", parser.value("fourier_symmetry"))) return false;
    if (parser.isSet("plate_hole"))              if (!applyCliOption("plate_hole", parser.value("plate_hole"))) return false;
    if (parser.isSet("lattice_union"))           if (!applyCliOption("lattice_union", parser.value("lattice_union"))) return false;
    if (parser.isSet("lattice_subtract"))        if (!applyCliOption("lattice_subtract", parser.value("lattice_subtract"))) return false;
    if (parser.isSet("lattice_subtract_size"))   if (!applyCliOption("lattice_subtract_size", parser.value("lattice_subtract_size"))) return false;
    if (parser.isSet("lattice_smooth"))          if (!applyCliOption("lattice_smooth", parser.value("lattice_smooth"))) return false;
    if (parser.isSet("honeycomb_angle"))         if (!applyCliOption("honeycomb_angle", parser.value("honeycomb_angle"))) return false;
    if (parser.isSet("honeycomb_rib"))           if (!applyCliOption("honeycomb_rib", parser.value("honeycomb_rib"))) return false;
    if (parser.isSet("honeycomb_custom"))        if (!applyCliOption("honeycomb_custom", parser.value("honeycomb_custom"))) return false;
    for (const char* k : { "assembly_struts", "assembly_plates", "assembly_shells", "assembly_holes", "assembly_spread", "assembly_file",
                           "foam_mode", "foam_frame", "foam_bubbles", "foam_file",
                           "mycelium_mode", "mycelium_length", "mycelium_spores", "mycelium_unit", "mycelium_angle", "mycelium_wiggle", "mycelium_fuse", "mycelium_taper", "mycelium_binding", "mycelium_align", "mycelium_anchors", "mycelium_poor", "mycelium_file",
                           "form_a", "form_b", "form_c" })
        if (parser.isSet(QString::fromLatin1(k)) && !applyCliOption(QString::fromLatin1(k), parser.value(QString::fromLatin1(k)))) return false;
    if (parser.isSet("material_iso"))            if (!applyCliOption("material_iso", parser.value("material_iso"))) return false;
    if (parser.isSet("material_ti"))             if (!applyCliOption("material_ti", parser.value("material_ti"))) return false;
    if (parser.isSet("no_fields"))               if (!applyCliOption("no_fields", "true")) return false;
    if (parser.isSet("lattice_uniform_wall"))    if (!applyCliOption("lattice_uniform_wall", "true")) return false;
    if (parser.isSet("print_min_wall_mm"))       if (!applyCliOption("print_min_wall_mm", parser.value("print_min_wall_mm"))) return false;
    if (parser.isSet("print_max_overhang_deg"))  if (!applyCliOption("print_max_overhang_deg", parser.value("print_max_overhang_deg"))) return false;
    if (parser.isSet("print_max_thin_fraction")) if (!applyCliOption("print_max_thin_fraction", parser.value("print_max_thin_fraction"))) return false;
    if (parser.isSet("print_max_island_vox"))    if (!applyCliOption("print_max_island_vox", parser.value("print_max_island_vox"))) return false;
    if (parser.isSet("print_max_components"))    if (!applyCliOption("print_max_components", parser.value("print_max_components"))) return false;
    if (parser.isSet("print_min_directions"))    if (!applyCliOption("print_min_directions", parser.value("print_min_directions"))) return false;
    if (parser.isSet("print_max_unsupported_fraction")) if (!applyCliOption("print_max_unsupported_fraction", parser.value("print_max_unsupported_fraction"))) return false;
    if (parser.isSet("require_printable"))       if (!applyCliOption("require_printable", "true")) return false;
    if (parser.isSet("remove_islands"))          if (!applyCliOption("remove_islands", "true")) return false;
    if (parser.isSet("lattice_metric_y"))        if (!applyCliOption("lattice_metric_y", parser.value("lattice_metric_y"))) return false;
    if (parser.isSet("lattice_metric_z"))        if (!applyCliOption("lattice_metric_z", parser.value("lattice_metric_z"))) return false;

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
    if (parser.isSet("fft_scheme"))              if (!applyCliOption("fft_scheme", parser.value("fft_scheme"))) return false;
    if (parser.isSet("fft_tol"))                 if (!applyCliOption("fft_tol", parser.value("fft_tol"))) return false;
    if (parser.isSet("fft_void_eta"))            if (!applyCliOption("fft_void_eta", parser.value("fft_void_eta"))) return false;
    if (parser.isSet("fft_discretization"))      if (!applyCliOption("fft_discretization", parser.value("fft_discretization"))) return false;
    if (parser.isSet("fft_max_iter"))            if (!applyCliOption("fft_max_iter", parser.value("fft_max_iter"))) return false;
    if (parser.isSet("local_criterion"))         if (!applyCliOption("local_criterion", parser.value("local_criterion"))) return false;
    if (parser.isSet("hill_space"))              if (!applyCliOption("hill_space", parser.value("hill_space"))) return false;
    if (parser.isSet("yield_stress"))            if (!applyCliOption("yield_stress", parser.value("yield_stress"))) return false;
    if (parser.isSet("yield_percentile"))        if (!applyCliOption("yield_percentile", parser.value("yield_percentile"))) return false;
    if (parser.isSet("eps"))                     if (!applyCliOption("eps", parser.value("eps"))) return false;
    if (parser.isSet("output"))                  if (!applyCliOption("output", parser.value("output"))) return false;
    if (parser.isSet("working_directory"))       if (!applyCliOption("working_directory", parser.value("working_directory"))) return false;
    if (parser.isSet("csv"))   if (!applyCliOption("csv", parser.value("csv"))) return false;

    if (parser.isSet("seed")) {
        if (!applyCliOption("seed", parser.value("seed"))) return false;
    }
    if (parser.isSet("seed-method")) {
        if (!applyCliOption("seed_method", parser.value("seed-method"))) return false;
    }
    if (parser.isSet("seed-sigma")) {
        if (!applyCliOption("seed_sigma", parser.value("seed-sigma"))) return false;
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

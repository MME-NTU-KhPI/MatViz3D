# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

MatViz3D is a Qt6/QML desktop application for microstructure modeling using cellular automata. It grows synthetic 3D grain structures on a voxel grid, assigns crystallographic texture (grain orientations) to them, and estimates the resulting effective stress/strain response and elastic stiffness either via a real ANSYS FEM run or an in-memory FFT (Moulinec-Suquet) homogenization solver.

## Build

Requires Qt 6 (developed against 6.8.3 / CI uses 6.6.3), a MinGW or GCC toolchain, and HDF5.

**Windows (qmake + mingw):**
```
C:\Windows\System32\cmd.exe /A /Q /K C:\Qt\6.8.3\mingw_64\bin\qtenv2.bat   # puts qmake/mingw32-make on PATH
qmake MatViz3D.pro
mingw32-make -j4
```
Or build in-place inside the existing Qt Creator shadow build dir: `build\Desktop_Qt_6_8_3_MinGW_64_bit-Debug\` (already has a generated `Makefile`; just `cd` there and run `mingw32-make`).

**Linux:**
```
qmake6 MatViz3D.pro
make -j$(nproc)
```

HDF5 paths are auto-detected in `MatViz3D.pro` (`C:\Program Files\HDF_Group\HDF5\<version>` on Windows, `/usr/include/hdf5/serial` on Linux).

There is no test suite in this repo. Validate changes by building, running `qmllint <file>.qml` on touched QML, and exercising the relevant feature through the GUI or `--nogui --autostart` CLI path.

### solver_compare (standalone ANSYS-vs-FFT harness)

`solver_compare/solver_compare.pro` builds a separate console executable that reuses the real `StressAnalysis`/`StressAnalysisFFT` translation units (not a reimplementation) to A/B the two solvers on identical grain orientations for a given seed:
```
cd solver_compare
qmake solver_compare.pro
make -j
./solver_compare --help
```
ANSYS must be on PATH for `--solver both|ansys`; it degrades to FFT-only otherwise.

## Running

GUI: run the built executable directly.

Headless/scripted, via `commandline_parser.cpp` options (`--help` for the full list):
- `--nogui --autostart` — generate a structure with no UI and exit (add `--run_stress_calc` to also solve).
- `--algorithm <name> --size <n> --points <n> --seed <n>` — structure generation (add `--animate` to step iteration by iteration instead of one shot).
- `--minkowski_p <p> --periodic` — Voronoi tessellation controls.
- `--composite_dim <1d|2d|3d> --composite_packing <square|hexagonal> --fiber_volume_fraction <0..1> --fibers_per_row <n>` — Composite RVE layout (`--periodic` applies here too).
- `--fiber_aspect_ratio <a/b> --fiber_angle_scatter <deg> --fiber_center_jitter <0..1> --fiber_allow_overlap` — Composite RVE imperfections.
- `--matrix_material <name> --fiber_material <name>` — the two Composite constituents (both from `material_properties.db`); replaces `--material` for that algorithm.
- `--material <name>` — material from `material_properties.db` (Cu, Fe, W, ...); supplies the cubic constants both stress solvers use and the lattice the texture presets are built for.
- `--texture <random|extrusion|rolling|recrystallization|shear|scattered_cube> --lattice <fcc|bcc> --scatter <deg>` — crystallographic texture preset (`--lattice` overrides the material's lattice).
- `--run_stress_calc --solver <ansys|fft> --stress_mode <single|dataset> --eps <exx,eyy,ezz,exy,eyz,exz>` — stress analysis.
- `--output <dir>`, `--working_directory <dir>`, `--num_rnd_loads <n>`, `--np <n>`.

## Architecture

### Global state: `Parameters` (parameters.h/.cpp)
A singleton (`Parameters::instance()`) exposing almost all cross-cutting state as **static** members (grid `size`/`points`, RNG `seed`, the live voxel grid `Parameters::voxels` (`int32_t***`), `Parameters::textureComponents`, stress-solver settings, etc.). It's also registered into QML as a singleton type (`import parameters 1.0`). Because the members are static, there is effectively one global mutable state shared by the algorithm layer, the texture editor, and both stress solvers — read/write it directly rather than threading state through constructors.

### Structure generation
`Parent_Algorithm` (parent_algorithm.h) is the base class for every algorithm; each owns the voxel grid (`int32_t*** voxels`, x/y/z grain-id volume) and iterates `Next_Iteration()` until `getDone()`. `MainWindowAlgorithmHandler` drives execution (`runAlgorithm`/`runStressCalculation`) and pushes results into the 3D view.

**Adding an algorithm (the plugin model).** `voronoi.cpp` and `composite.cpp` are the reference. An algorithm is described by one `AlgorithmPlugin` (algorithmplugin.h) carrying its name, parameter schema and factory together, declared at namespace scope in its own .cpp with `MATVIZ_REGISTER_ALGORITHM(...)`. That static registrar puts it in `AlgorithmFactory` before `main()` runs, so nothing else has to name it: `SchemaController::algorithmNames()` feeds the UI combo box straight from the registry, and `schemaFor()` builds the parameter panel. Adding an algorithm = adding one .h/.cpp pair plus the two `.pro` lines. Shared schema pieces live in algorithmplugin.cpp — `baseAlgorithmSchema()` (size/points/wave), `materialParamField()`, `textureParamFields()`.

All algorithms in MatViz3D — `Voronoi`, `Composite`, `Moore`, `Neumann`, `Radial`, `DLCA`, and `Probability` — are self-registering `AlgorithmPlugin`s declared in their own `.cpp` with `MATVIZ_REGISTER_ALGORITHM(...)`. Historical algorithms `Probability_Circle` and `Probability_Ellipse` are unified into `Probability` as shape presets ("Sphere (Circle)", "Triaxial Ellipsoid", etc.).

`ParamField` (paramfield.h) types are `Int/Double/Bool/Enum/PointsMode/Action`, rendered by `DynamicParamBlock.qml` (the QML switch matches the enum ordinal). Two indirections keep schemas buildable during static initialisation, before `QCoreApplication` exists: `optionsProvider` names a runtime source for `Enum` options (`"materials"` -> the SQL table), and `action` / `actionOnValue` raise `SchemaController::actionTriggered(id)` for QML to turn into a window (this is how the texture dropdown's "Custom (editor)" entry and the "Open editor..." button reach `TextureView.qml`).

**`Voronoi` (voronoi.h/.cpp)** — the reference algorithm. Assigns each voxel to the nearest seed under a Minkowski L_p metric (`Parameters::minkowski_p`; p=1 octahedral, 2 Euclidean, large p cuboidal), with minimum-image offsets when `Parameters::is_periodic`. The nearest-seed search uses a CSR bucket grid over the seeds with an expanding-ring bound (`d_p >= the ring's axis gap` for any p > 0) and a tabulated `|d|^p`, so an arbitrary real p costs the same as p=1. The tessellation is exact and one-shot, but `Next_Iteration()` still reveals it in distance bands so animation/GIF recording work and end on the identical structure. Set `MATVIZ_VORONOI_VERIFY=1` to cross-check the bucket search against brute force and the banded reveal against the exact tessellation.

**`Composite` (composite.h/.cpp)** — fiber-reinforced RVE, and the second plugin-style algorithm. One, two or three orthogonal families of continuous elliptical fibers (`composite_dim` = 1D along Z / 2D along X,Y / 3D along X,Y,Z) on a square or row-staggered hexagonal lattice. Each family is decided entirely in the 2D plane normal to its axis and rasterized into an id plane (ids plus the normalised elliptical radius), which a single O(N^3) combine pass then merges — so cost is O(F·N^2 + N^3), not voxels × fibers. Crossings go to whichever fiber the voxel sits **deepest inside**, and exact ties are dealt out alternately by `(x+y+z) % candidates`. Both halves of that rule matter: taking the first family listed instead gave family X ~1.8x the volume of family Y in a 0/90 layup, and on a symmetric lattice a quarter of the crossing volume ties exactly. With the rule in place a 2D crossed cell comes out with Ex == Ey to the digit, and a 3D orthogonal one with Ex == Ey == Ez to 0.2%.

`fiber_volume_fraction` is an *input*, not an outcome: the fiber semi-axes are bisected against the volume fraction measured on a sub-sampled copy of the grid (`m_coarseN`, capped at 128) until the target is hit, which matters for 2D/3D where the families intersect and the areas do not simply add. Unless `fiber_allow_overlap` is set, the target is clamped to the no-overlap packing limit and the clamp is logged. Because Vf(r) is a staircase on a voxel grid — and on a perfect lattice every fiber crosses each step together — the bisection returns the radius whose *measured* fraction is closest to the target rather than whichever side it converged on; the achieved value is always reported.

The imperfection knobs (`fiber_center_jitter`, `fiber_aspect_ratio`, `fiber_angle_scatter`) all run off `Parameters::seed`. Ellipse area is held at `pi*a*b = pi*r^2`, so a/b changes shape at constant Vf. When the ellipses are aligned (zero scatter) the lattice is stretched by the same a/b, which keeps the packing ceiling at the circular value for any aspect ratio; once the fibers can rotate that stretch buys nothing and the lattice stays isotropic, so the reachable Vf falls off as ~1/(a/b). Jittered centers are rejection-sampled against their neighbours unless overlap is allowed. `Next_Iteration()` re-rasterizes at a growing radius, so the animated path ends on exactly the structure `Generate_To_End()` produces in one shot.

The matrix is grain id 1; every fiber gets its own id from 2 up, because orientation is carried per grain id and each fiber's material frame comes from its own axis and ellipse angle. Composite publishes a `PhaseAssignment` (matrix material + fiber material + per-fiber orientation) that both solvers read — see "Multi-phase materials" below.

### Crystallographic texture
`TextureLibrary` (texturelibrary.h/.cpp) samples per-grain crystal orientations as Bunge ZXZ Euler angles, either uniformly random (`Mode::Random`) or from ODF texture components (`Mode::Components`, built from a `TextureLibrary::Process` preset — rolling/extrusion/recrystallization/shear — for an FCC or BCC lattice). `TextureController` + `TextureView.qml` is the interactive editor; on apply it writes into `Parameters::textureComponents`, which is the single source both stress solvers read from. Algorithm parameter panels and the CLI instead go through `Parameters::setTexturePreset()/setTextureScatter()`, which rebuild `textureComponents` from a preset via `rebuildTextureFromPreset()`. The preset `"custom"` means "owned by the editor" and suppresses that rebuild — applying in the editor sets it (`markTextureCustom()`), so a hand-edited texture is never silently overwritten. Note that the `Random` preset is represented by an *empty* `textureComponents`, which is what every existing caller already treats as uniformly random. `stressresult.h::buildGrainOrientations()` is the shared, seed-driven bridge that turns `textureComponents` into a `grain id -> orientation` table — it's called by *both* solvers so a given `Parameters::seed` produces the identical polycrystal realization in ANSYS and FFT (this is what makes `solver_compare` a fair A/B test).

### Multi-phase materials
`phasematerial.h` carries `PhaseMaterial` (a name + a **full Voigt 6x6 stiffness in Pa, in the material's own axes** — nothing assumes cubic) and `PhaseAssignment` (the material list, `grain id -> phase`, and optionally `grain id -> Bunge ZXZ orientation`). An algorithm that knows its structure has more than one constituent publishes one into `Parameters::phaseAssignment`; this is the multi-phase analogue of `Parameters::textureComponents`.

`stressresult.h::resolveGrainMaterials()` is the shared bridge — called by *both* `StressAnalysisFFT::makeSession()` and `StressAnalysis`'s `sharedGrainMaterialsFor()`, so for one seed the two backends see the identical per-grain orientation *and* constituent. An **empty or mismatched** assignment falls back to exactly the historical single-material path (`db_material`'s cubic constants + texture-sampled orientations), which is what keeps every other algorithm unchanged. `MainWindowAlgorithmHandler::executeAlgorithm()` clears the assignment before each run so a stale one can never be applied to a different structure's grain ids.

Backend specifics:
- **FFT** — `FFTSolverSession` has a second constructor taking a per-grain Voigt stiffness (material axes) and rotating each by that grain's orientation via `mvh::aniso_grain_mandel()`. The `void_C11` argument sizes the soft void phase; pass the largest C11 in play.
- **ANSYS** — `setAnisoMaterial(matId, C[6][6])` emits one `TB,ANEL,<matId>,1,21` block per constituent, and `setGrainMaterials()` gives the wrapper a `grain id -> material number` table that the EBLOCK's MAT column now reads (it used to be hardcoded to 1). Two conventions matter here, both confirmed against real v252 solves:
  - The 21 TBDATA slots are the **row-wise upper triangle** (D11 D12 D13 D14 D15 D16, D22 D23 …, D66) — the layout the previous hardcoded cubic block used. Not the column-wise order the docs suggest: that would put 0 in D33 and the material would be singular.
  - **ANSYS orders the shear components (xy, yz, xz); Voigt — and so this codebase, the material database and the FFT solver — orders them (yz, xz, xy).** `setAnisoMaterial` permutes via `A2V = {0,1,2,5,3,4}`. Without it a transversely isotropic fiber's in-plane shear modulus lands in Gxz instead of Gxy. This was invisible for as long as it was because a cubic material has all three shear moduli equal and no normal-shear coupling.

`DBManager::stiffnessMatrix()` reads all 21 columns by name, expanding rows that only have c11/c12/c44 filled into the cubic matrix those imply. `material_properties.db` now also carries composite constituents: `Epoxy`, `E-glass`, `Al2O3`, `SiC` (type `iso`) and `C-fiber` (type `ti` — transversely isotropic **about axis 3**, which is the axis `Composite` aligns to the fiber). Seeding is per-material insert-if-absent, so an existing database picks up new rows without losing edits.

`fft_homog.hpp::pick_reference()` needed a fix for this to work at all. Its per-phase modulus estimate reads only the shear diagonal and the lambda block — it never looks at `C[0..2][0..2]` — which tracks fine for cubic crystals but comes out ~6x too soft for a transversely isotropic fiber, and the basic scheme then **diverges** (error growing, then NaN) rather than converging slowly. It now also enforces the scheme's sufficient condition directly (`2*C0 - C(x)` positive definite, using a power-iteration `lambda_max` per phase) and raises the reference when the heuristic falls short. The reference medium does not change the fixed point, only stability and rate, so this can only make an existing run more robust; it is a no-op for every cubic material in the library.

Even so, the basic scheme converges linearly in phase contrast, and it is slower than you would guess: a plain **dense Cu polycrystal** (Zener 3.2, no second phase) already needs ~800 iterations at `fft_tol` 1e-5 on a 20^3 grid, i.e. ~70 s per load case and ~7 min for the six of a stiffness run. Carbon/epoxy is ~40x contrast and will hit `fft_max_iter` (default 1000) on a full dataset build. Prefer a lower-contrast pair (E-glass/epoxy, W/Al) when iterating, or the ANSYS backend, which does not care about contrast. This is a property of the basic scheme, not of any particular algorithm — `Moore` and `Voronoi` produce fully dense structures (solid fraction 1) and take the same time for the same grain count.

Useful self-checks, all cheap and all falsifiable. Every one of these holds for **both** backends, and the first three are exact because a homogeneous cell is BC-independent:
- both phases the same material must return that material's own constants (Epoxy -> E = 3.509 GPa, E-glass -> 71.99 GPa);
- a 1D cell of C-fiber must return C-fiber's own constants (Ez = 230, Ex = Ey = 15, Gxy = 7.01, Gyz = Gxz = 15 GPa — note the moduli are reported as 1/S_ii, so the shears print at 2x);
- a two-material cell whose materials happen to be identical must return the same, which is what proves material 2 is defined and its mat_id resolves;
- a 2D crossed cell must give Ex == Ey, and a 3D orthogonal cell with an isotropic matrix Ex == Ey == Ez (FFT only — periodic);
- a UD carbon/epoxy cell must match the rule of mixtures on the axial modulus: at the achieved Vf = 0.4444, `Vf*230 + (1-Vf)*3.5` = 104.2 GPa, and ANSYS returns 104.2 GPa.

### Stress analysis: two interchangeable solver backends
`StressAnalysisController` (Q_OBJECT, exposed to QML as `stressAnalysisController`) is the single UI-facing entry point and picks between:
- **`StressAnalysis`** (stressanalysis.cpp) — drives a real ANSYS process via `ansyswrapper.cpp` (writes APDL, launches ANSYS, parses results). Needs ANSYS installed and reachable.
- **`StressAnalysisFFT`** (stressanalysis_fft.cpp) — an in-memory, dependency-free FFT homogenizer. `fft_homog.hpp` is the header-only Moulinec-Suquet basic-scheme solver (self-contained complex FFT, OpenMP-parallelized); `fft_solver_session.hpp`'s `FFTSolverSession` adapts it to the pipeline's tensor conventions (Mandel vs. pipeline strain/stress ordering — read the conventions comment at the top of that file before touching the math).

Both solvers implement the same three-phase pipeline and write the same HDF5 schema, so downstream code (`LoadStepManager`, the 3D field viewer) doesn't care which one ran:
1. **Phase 1.0** — 6 canonical unit-strain solves -> effective compliance/stiffness `S`/`C`/`P` matrices + moduli.
2. **Phase 1.5** — ~150 random loads -> least-squares fit of a Hill-ellipsoid yield criterion (`hillcriterion.cpp`).
3. **Phase 2.0** — ~300 loads sampled on the Hill ellipsoid -> full per-voxel field results written to HDF5 (via `hdf5wrapper.cpp`) and reloaded through `LoadStepManager` for visualization.

`StressAnalysisController` also exposes cheaper standalone entry points that skip phases 1.5/2.0 (single-load-case solve, and a 6-load "stiffness matrix only" computation) for fast iteration/debugging without a full dataset build.

Result types are unified in `stressresult.h` (`SingleShotResult`, `StiffnessMatrixResult`, `FieldVisualizationData`) so the controller and QML can treat ANSYS and FFT output identically.

### Threading convention
Structure generation and both solvers run off the Qt main thread via `QtConcurrent::run` + `QFutureWatcher`, per the pattern in `stressanalysiscontroller.cpp`. The worker lambdas must **not** touch QML-facing singletons (`OpenGLWidgetQML::getInstance()`, `LoadStepManager::getInstance()`) directly — those are unsynchronized and are only touched from the corresponding `on*Finished()` slot back on the main thread. If a worker needs to push incremental progress to the UI (e.g. live FFT convergence), it must marshal back via `QMetaObject::invokeMethod(controller, lambda, Qt::QueuedConnection)`, never call controller methods directly from the worker thread.

### QML shell
`main.qml`/`MainWindow.qml` is the app shell; feature windows (`StatisticsView.qml`, `TextureView.qml`, `StressAnalysisView.qml`, `MaterialDatabaseView.qml`, `AboutView.qml`) are lazily instantiated via `Loader { active: false }` and toggled on from the main window's menu. Each is backed by one `QObject`-derived controller constructed in `main.cpp` and registered on `engine.rootContext()` (`mainWindowWrapper`, `schemaController`, `statisticsController`, `exportController`, `stressAnalysisController`, `textureController`, plus the `Parameters` QML singleton). 3D rendering goes through `OpenGLWidgetQML` (a `QQuickFramebufferObject`) backed by `RenderOpenGL`.

### Other supporting pieces
- `hdf5wrapper.cpp` — all HDF5 read/write; `loadstepmanager.cpp` owns the loaded-dataset state the 3D viewer/plots read from.
- `grain_analyzer.cpp` — post-hoc grain statistics (feeds `StatisticsView.qml`).
- `dbmanager.cpp` — SQL-backed material property database (`MaterialDatabaseView.qml`). `DBManager::materialDatabase()/materialNames()/cubicConstants()` are static and open the connection lazily, so headless runs resolve `--material` without constructing a `DBManager`. The table stores GPa; `Parameters::cubicConstantsPa()` converts and is what *both* solvers call instead of hardcoding constants — an unselected material falls back to the historical Cu values (168.4/121.4/75.4 GPa). The row's `Type` column drives `Parameters::materialLattice()`, so the texture presets follow the material unless `--lattice` overrides.
- `exportcontroller.cpp` — GIF recording of structure generation, other export flows.
- `matviz_homog.hpp` — shared crystal-rotation/homogenization math (e.g. `mvh::cubic_grain_mandel`) used by the FFT path.

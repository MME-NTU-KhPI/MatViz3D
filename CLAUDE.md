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
- `--algorithm <name> --size <n> --points <n> --seed <n>` — structure generation.
- `--texture <random|extrusion|rolling|recrystallization|shear> --lattice <fcc|bcc> --scatter <deg>` — crystallographic texture preset.
- `--run_stress_calc --solver <ansys|fft> --stress_mode <single|dataset> --eps <exx,eyy,ezz,exy,eyz,exz>` — stress analysis.
- `--output <dir>`, `--working_directory <dir>`, `--num_rnd_loads <n>`, `--np <n>`.

## Architecture

### Global state: `Parameters` (parameters.h/.cpp)
A singleton (`Parameters::instance()`) exposing almost all cross-cutting state as **static** members (grid `size`/`points`, RNG `seed`, the live voxel grid `Parameters::voxels` (`int32_t***`), `Parameters::textureComponents`, stress-solver settings, etc.). It's also registered into QML as a singleton type (`import parameters 1.0`). Because the members are static, there is effectively one global mutable state shared by the algorithm layer, the texture editor, and both stress solvers — read/write it directly rather than threading state through constructors.

### Structure generation: cellular automata
`Parent_Algorithm` (parent_algorithm.h) is the base class for every grain-growth algorithm; each owns the voxel grid (`int32_t*** voxels`, x/y/z grain-id volume) and iterates `Next_Iteration()` until `getDone()`. Concrete algorithms — `Moore`, `Neumann`, `Radial`, `Composite`, `DLCA`, `Probability_Algorithm`, `Probability_Circle`, `Probability_Ellipse` — register themselves by name in `AlgorithmFactory` (algorithmfactory.cpp's `registerAlgorithms()`), which QML/CLI code looks up by string. `MainWindowAlgorithmHandler` drives execution (`runAlgorithm`/`runStressCalculation`) and pushes results into the 3D view.

### Crystallographic texture
`TextureLibrary` (texturelibrary.h/.cpp) samples per-grain crystal orientations as Bunge ZXZ Euler angles, either uniformly random (`Mode::Random`) or from ODF texture components (`Mode::Components`, built from a `TextureLibrary::Process` preset — rolling/extrusion/recrystallization/shear — for an FCC or BCC lattice). `TextureController` + `TextureView.qml` is the interactive editor; on apply it writes into `Parameters::textureComponents`, which is the single source both stress solvers read from. `stressresult.h::buildGrainOrientations()` is the shared, seed-driven bridge that turns `textureComponents` into a `grain id -> orientation` table — it's called by *both* solvers so a given `Parameters::seed` produces the identical polycrystal realization in ANSYS and FFT (this is what makes `solver_compare` a fair A/B test).

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
- `dbmanager.cpp` — SQL-backed material property database (`MaterialDatabaseView.qml`).
- `exportcontroller.cpp` — GIF recording of structure generation, other export flows.
- `matviz_homog.hpp` — shared crystal-rotation/homogenization math (e.g. `mvh::cubic_grain_mandel`) used by the FFT path.

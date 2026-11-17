# Neural tab: model files and the GPU solver

The **Neural** tab of the Data panel generates a Lattice-like cell for given target properties (Young's and shear moduli, Poisson's ratios, anisotropy ratios, density) and a printing material. Three networks do it, all run by the program's own C++ code (no Python, no ML library inside MatViz3D):

| Network | What it does | Weights | In git |
|---|---|---|---|
| decoder | code 16 × 16 × 16 × 4 → voxel cell 64³ | `models/cell_decoder.bin`, 2.3 MB | yes, built into the program (`models.qrc`) |
| diffusion network | target properties + material → code (DDIM, 50 steps) | `models/cell_diffusion.bin`, 102 MB | **no** |
| encoder + property predictor | voxel cell → the 24 target properties (the `predicted` column) | `models/cell_predictor.bin`, 12 MB | **no** |

The networks are trained outside this repository (the `cellgen` training project) and exported into these files by the scripts of this folder.

## Getting the weights

`cell_diffusion.bin` and `cell_predictor.bin` are git-ignored: GitHub refuses files above 100 MB, and the weights change with every training run. Get both files from whoever trains the model and put them into `models/` of the source tree, or anywhere the program looks:

| Order | Place |
|---|---|
| 1 | `MATVIZ_CELL_DIFFUSION=<file>`, `MATVIZ_CELL_PREDICTOR=<file>` (when set, only that file is tried) |
| 2 | `models/` next to the executable |
| 3 | `Resources/models/` of the macOS bundle, `models/` next to the `.app` |
| 4 | `models/` of the source tree the program was built from (`MATVIZ_SOURCE_DIR`, set by both `.pro` files) |

`MATVIZ_CELL_DECODER=<file>` replaces the built-in decoder by the decoder of another training run.

Without the files the program still builds and runs: the tab says the model is missing and decodes a *random* code (noise-like, unprintable cells), the `predicted` column is hidden, and the tests that need the files (`TestCellDiffusion`, `TestCellPredictor`) are skipped, not failed. The header of each file carries the name of the run and a note with what was measured on its cells; the tab shows that note.

## Solve and compare on the GPU

**Solve and compare** computes the stiffness of the generated cell with the settings the dataset was solved with (six unit loads, conjugate gradients, Willot's rotated scheme, tolerance 1e-5, void 1e-4 and a second pass with void 1e-6 when the solid carries less than 98.5 % of the strain energy) and puts it next to the request.

- With **Python 3.9+, NumPy and PyTorch** that sees a GPU (NVIDIA `cuda`, Apple Silicon `mps`, Intel `xpu`), the program runs `gpu_stiffness.py` as a separate process: seconds per cell.
- Without them it solves with the same scheme in C++ on the CPU: minutes per cell (`--np` sets the threads).

The program looks for the interpreter in `MATVIZ_GPU_PYTHON`, then in `/opt/anaconda3`, `~/anaconda3`, `~/miniconda3`, `~/miniforge3`, Homebrew, `/usr/local` and `python3` on `PATH`, and for the script in `MATVIZ_GPU_SCRIPT`, next to the executable, in the bundle and in the source tree. `MATVIZ_GPU_SOLVER=0` forces the CPU. On **Windows** none of these paths exist, so set the interpreter explicitly and restart the program:

```bat
setx MATVIZ_GPU_PYTHON "C:\Users\<name>\AppData\Local\Programs\Python\Python312\python.exe"
```

To check the solver on a computer before relying on it, run `stiffness_check.py` (self-test, dataset cells, GPU against CPU): see [STIFFNESS_CHECK.md](STIFFNESS_CHECK.md).

## Files of this folder

| File | Use |
|---|---|
| `gpu_stiffness.py` | the GPU solver the program calls (also importable) |
| `stiffness_check.py`, `STIFFNESS_CHECK.md` | stand-alone check of that solver on any computer |
| `export_cell_decoder.py` | training checkpoint → `models/cell_decoder.bin` (rebuild the program afterwards: the decoder is built in) |
| `export_cell_diffusion.py` | training checkpoint → `models/cell_diffusion.bin` |
| `export_cell_predictor.py` | training checkpoint → `models/cell_predictor.bin` |
| `diffusion_check.cpp`, `check_cell_diffusion.py` | C++ against PyTorch on the same request and starting noise |

## After a new training run

```bash
python3 tools/neural/export_cell_diffusion.py --cellgen <training folder> --run dif6 --ckpt gen_step005000.pt \
        --note "what was measured on the cells of this model" --reference
python3 tools/neural/export_cell_predictor.py --cellgen <training folder> --run pred1 --ckpt best.pt --reference
```

`--reference` prints the numbers the C++ tests compare with (`tests/test_celldiffusion.cpp`, `tests/test_cellpredictor.cpp`); add them to the test when the architecture or the run changes. A new run of the same architecture needs no code change and no rebuild: the program reads the file when the first cell is generated. To compare the C++ sampler with PyTorch on one request:

```bash
clang++ -O2 -std=c++17 -I. tools/neural/diffusion_check.cpp celldiffusion.cpp cellnet.cpp celldecoder.cpp -o diffusion_check \
        -DMATVIZ_USE_ACCELERATE -framework Accelerate -Xpreprocessor -fopenmp \
        -I/opt/homebrew/opt/libomp/include -L/opt/homebrew/opt/libomp/lib -lomp
./diffusion_check models/cell_diffusion.bin models/cell_decoder.bin out
python3 tools/neural/check_cell_diffusion.py out --cellgen <training folder> --run dif6 --ckpt gen_step005000.pt
```

Expected: one pass within ~1e-5, the code after 50 steps within ~1e-4, the decoded cell differing in at most a few of 262 144 voxels (rounding order of the sums).

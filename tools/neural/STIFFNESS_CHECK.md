# Checking the stiffness solver on any computer

`stiffness_check.py` computes the stiffness of a solid / void cell with the solver of the **Solve and compare** button of the Neural tab (`gpu_stiffness.py`, which must sit in the same folder). It runs on the GPU when PyTorch sees one, otherwise on the CPU.

`gpu_stiffness.py` is the only part of MatViz3D written in Python. Structure generation, the printability check, the CPU FFT solver the dataset was computed with and all three networks of the Neural tab are C++. When the program finds no Python with PyTorch and a GPU, Solve and compare uses the same scheme in C++ on the CPU, only slower.

## Installation

Python 3.9 or newer, NumPy and PyTorch are needed, and h5py for `.hdf5` files. Supported GPUs: NVIDIA (`cuda`), Apple Silicon (`mps`) and Intel (`xpu`). Without one, AMD on Windows included, the CPU computes, about 10-15 times slower.

Windows with NVIDIA:

1. Install Python from python.org with "Add python.exe to PATH" ticked.
2. `py -m pip install numpy h5py`
3. Install PyTorch built for CUDA: pytorch.org gives the command under "Get Started" (Windows, Pip, CUDA). A plain `pip install torch` on Windows installs a build without CUDA.
4. Check: `py -c "import torch; print(torch.cuda.is_available())"` must print `True`.

Mac: `python3 -m pip install torch numpy h5py`.

## Running

`py` on Windows, `python3` on Mac and Linux:

| Command | What it does |
|---|---|
| `py stiffness_check.py` | self-test, about a minute |
| `py stiffness_check.py cell.hdf5` | a cell from a MatViz3D or dataset file, compared with the answer stored in the file |
| `py stiffness_check.py folder` | every `.hdf5`, `.h5`, `.npy` file of the folder and a summary table |
| `py stiffness_check.py cell.npy` | an n × n × n array, non-zero = solid |
| `py stiffness_check.py cell.raw --n 64` | n³ bytes in the order the program hands a cell to the solver |

Options: `--device cuda|mps|xpu|cpu` (where to compute), `--compare_cpu` (repeat on the CPU and compare), `--e_gpa`, `--nu`, `--aniso` (the material; by default from the file, else 2 GPa, 0.38, 1), `--json results.json` (save the results).

## Self-test

| Cell | Checked against |
|---|---|
| `solid`, a full cube | the answer equals the material matrix |
| `layers_x`, `layers_y`, `layers_z`, flat layers | the answer equals the exact formula for layers; the material is softer along z, so mixed-up axes show as a difference |
| `gyroid` | cubic symmetry: Ex = Ey = Ez, the three shear moduli equal |
| `gyroid` 32³ | the GPU and the CPU give one answer |

The run must end with `all 6 checks passed`. For the layers the solid share of the energy is about 0 and a second pass runs: across the layers the solid carries no load, as it should.

## Examples with their output

Each block starts with the command (`$` is the terminal prompt) followed by its output unchanged. The outputs were produced on a Mac with an Apple GPU (`mps`) from the folder of the script, with dataset cells `000004.hdf5` and `000066.hdf5` copied into a folder `examples` next to it (dataset cells are not kept in this repository). A network training shared the GPU at the time, so the times are higher than usual. On a computer with NVIDIA, `cuda` and the name of the card appear instead of `mps`; the moduli and the differences must agree to the last digits, the time and the number of iterations may differ slightly. Lines starting with `...` show the progress every 5 seconds and differ from run to run; they are left out in examples 3 and 4.

### 1. Self-test

```
$ python3 stiffness_check.py
Python 3.12.7 (/opt/anaconda3/bin/python3), PyTorch 2.12.0, NumPy 1.26.4, Darwin arm64
devices PyTorch sees: mps, cpu
solving on mps: Apple GPU (Metal), arm64

self-test on 64^3 cells: material nu 0.38, aniso 0.8 (stiffness across z is 0.8 of that in the plane), gyroid with aniso 1

1. solid: solid share 1.000
   mps: 0 iterations, converged, void 1e-4, solid share 1.000, 1.9 s, GPU memory 1.17 GB
   answer against the material matrix, largest difference / C11: 1.48e-07 (limit 1e-04) -> PASS

2. layers_x: solid share 0.500
   mps: 10 iterations, converged, second pass with void 1e-6 (solid held 0.000 < 0.985 of the energy with 1e-4), solid share 0.000, 4.1 s, GPU memory 1.17 GB
   (across the layers nothing but the void carries a load, so a solid share near 0, the second pass and moduli near 0 across are the right answer)
   answer against the exact layer formula, largest difference / C11: 6.78e-08 (limit 1e-04) -> PASS
   moduli / E here:  Ex 0.00000 Ey 0.50000 Ez 0.40000 Gxy 0.00000 Gyz 0.14493 Gxz 0.00000
   moduli / E exact: Ex 0.00000 Ey 0.50000 Ez 0.40000 Gxy 0.00000 Gyz 0.14493 Gxz 0.00000

3. layers_y: solid share 0.500
   mps: 15 iterations, converged, second pass with void 1e-6 (solid held 0.000 < 0.985 of the energy with 1e-4), solid share 0.000, 3.9 s, GPU memory 1.17 GB
   (across the layers nothing but the void carries a load, so a solid share near 0, the second pass and moduli near 0 across are the right answer)
   answer against the exact layer formula, largest difference / C11: 6.78e-08 (limit 1e-04) -> PASS
   moduli / E here:  Ex 0.50000 Ey 0.00000 Ez 0.40000 Gxy 0.00000 Gyz 0.00000 Gxz 0.14493
   moduli / E exact: Ex 0.50000 Ey 0.00000 Ez 0.40000 Gxy 0.00000 Gyz 0.00000 Gxz 0.14493

4. layers_z: solid share 0.500
   mps: 24 iterations, converged, second pass with void 1e-6 (solid held 0.000 < 0.985 of the energy with 1e-4), solid share 0.000, 5.0 s, GPU memory 1.17 GB
   (across the layers nothing but the void carries a load, so a solid share near 0, the second pass and moduli near 0 across are the right answer)
   answer against the exact layer formula, largest difference / C11: 1.08e-07 (limit 1e-04) -> PASS
   moduli / E here:  Ex 0.50000 Ey 0.50000 Ez 0.00000 Gxy 0.18116 Gyz 0.00000 Gxz 0.00000
   moduli / E exact: Ex 0.50000 Ey 0.50000 Ez 0.00000 Gxy 0.18116 Gyz 0.00000 Gxz 0.00000

5. gyroid: solid share 0.293
      ... 90 iterations, residual 3.1e-04, loads done 3 of 6
   mps: 735 iterations, converged, void 1e-4, solid share 0.998, 10.0 s, GPU memory 1.17 GB
   moduli / E: Ex 0.10613 Ey 0.10613 Ez 0.10613 Gxy 0.04555 Gyz 0.04555 Gxz 0.04555
   cubic symmetry: spread of Ex, Ey, Ez and of the three G, relative: 4.91e-05 (limit 5e-04) -> PASS

6. gyroid 32^3 on mps and on the CPU (the CPU is slower; a smaller cell)
      ... 30 iterations, residual 1.3e-02, loads done 0 of 6
      ... 70 iterations, residual 4.6e-04, loads done 0 of 6
      ... 100 iterations, residual 8.3e-04, loads done 3 of 6
      ... 130 iterations, residual 8.8e-05, loads done 3 of 6
      ... 160 iterations, residual 1.6e-04, loads done 3 of 6
   mps: 815 iterations, converged, void 1e-4, solid share 0.996, 1.9 s, GPU memory 1.17 GB
   cpu: 813 iterations, converged, void 1e-4, solid share 0.996, 35.2 s
   moduli on the GPU against the CPU, largest relative difference: 1.18e-04 (limit 1e-03) -> PASS

all 6 checks passed on mps
```

### 2. One cell of a dataset file

Cell 000004: material 2.568 GPa, ν 0.4305, across the print layers 0.811 of the stiffness along them; the script reads all of it from the file.

```
$ python3 stiffness_check.py examples/000004.hdf5
Python 3.12.7 (/opt/anaconda3/bin/python3), PyTorch 2.12.0, NumPy 1.26.4, Darwin arm64
devices PyTorch sees: mps, cpu
solving on mps: Apple GPU (Metal), arm64

000004.hdf5 (group 1): 64^3, solid share of the voxels 0.468, material E 2.568 GPa, nu 0.4305, aniso 0.811 (from the file)
      ... 90 iterations, residual 8.1e-04, loads done 0 of 6
      ... 190 iterations, residual 1.8e-04, loads done 0 of 6
      ... 290 iterations, residual 3.2e-05, loads done 0 of 6
   mps: 1913 iterations, converged, void 1e-4, solid share 0.993, 18.6 s, GPU memory 1.17 GB
   moduli, GPa:  Ex 0.42945  Ey 0.32558  Ez 0.30963  Gxy 0.13711  Gyz 0.11986  Gxz 0.14314
   relative to E: Ex 0.16723  Ey 0.12678  Ez 0.12057  Gxy 0.05339  Gyz 0.04667  Gxz 0.05574
   stiffness matrix C, GPa (order xx yy zz xy yz xz, engineering shear):
                   xx         yy         zz         xy         yz         xz
        xx    0.57624    0.13882    0.17651    0.04483    0.00632    0.12677
        yy    0.13882    0.38392    0.12619    0.02908    0.01039    0.02594
        zz    0.17651    0.12619    0.41734    0.01044    0.00743    0.11110
        xy    0.04483    0.02908    0.01044    0.14833    0.02877    0.00272
        yz    0.00632    0.01039    0.00743    0.02877    0.12704    0.01542
        xz    0.12677    0.02594    0.11110    0.00272    0.01542    0.18869
   stored answer of the file: E differ by 0.0012 %, G by 0.0111 %, the matrix by 0.0039 % -> PASS (limit 0.1 %)

everything passed
```

### 3. Every cell of a folder

Cell 000066 is solved twice: with the void at 1e-4 the solid carries 0.983 of the energy, below the threshold 0.985, so a second pass runs with the void at 1e-6, as in the dataset.

```
$ python3 stiffness_check.py examples
Python 3.12.7 (/opt/anaconda3/bin/python3), PyTorch 2.12.0, NumPy 1.26.4, Darwin arm64
devices PyTorch sees: mps, cpu
solving on mps: Apple GPU (Metal), arm64

(the block of cell 000004.hdf5, as in example 2)

000066.hdf5 (group 1): 64^3, solid share of the voxels 0.400, material E 2.115 GPa, nu 0.4465, aniso 0.672 (from the file)
   mps: 6335 iterations, converged, second pass with void 1e-6 (solid held 0.983 < 0.985 of the energy with 1e-4), solid share 1.000, 95.7 s, GPU memory 1.17 GB
   moduli, GPa:  Ex 0.36720  Ey 0.05530  Ez 0.18655  Gxy 0.07954  Gyz 0.06103  Gxz 0.11391
   relative to E: Ex 0.17362  Ey 0.02615  Ez 0.08820  Gxy 0.03761  Gyz 0.02885  Gxz 0.05386
   stiffness matrix C, GPa (order xx yy zz xy yz xz, engineering shear):
                   xx         yy         zz         xy         yz         xz
        xx    0.47292    0.08205    0.14912   -0.00456   -0.00797    0.05021
        yy    0.08205    0.08975    0.09679    0.00296   -0.00298    0.00125
        zz    0.14912    0.09679    0.34499   -0.01280   -0.06027    0.03337
        xy   -0.00456    0.00296   -0.01280    0.08461    0.00893   -0.02298
        yz   -0.00797   -0.00298   -0.06027    0.00893    0.07529   -0.01102
        xz    0.05021    0.00125    0.03337   -0.02298   -0.01102    0.12779
   stored answer of the file: E differ by 0.0005 %, G by 0.0005 %, the matrix by 0.0005 % -> PASS (limit 0.1 %)

cell                                 grid  seconds   Ex / E   vs file  result
000004.hdf5 (group 1)                64^3     24.7   0.1672  0.0012 %  PASS
000066.hdf5 (group 1)                64^3     95.7   0.1736  0.0005 %  PASS

everything passed
```

### 4. GPU against CPU

```
$ python3 stiffness_check.py gyroid --n 32 --compare_cpu
Python 3.12.7 (/opt/anaconda3/bin/python3), PyTorch 2.12.0, NumPy 1.26.4, Darwin arm64
devices PyTorch sees: mps, cpu
solving on mps: Apple GPU (Metal), arm64

gyroid (built in): 32^3, solid share of the voxels 0.285, material E 2 GPa, nu 0.38, aniso 1 (default)
   mps: 815 iterations, converged, void 1e-4, solid share 0.996, 1.8 s, GPU memory 0.10 GB
   moduli, GPa:  Ex 0.19515  Ey 0.19516  Ez 0.19515  Gxy 0.07917  Gyz 0.07918  Gxz 0.07917
   relative to E: Ex 0.09758  Ey 0.09758  Ez 0.09758  Gxy 0.03959  Gyz 0.03959  Gxz 0.03959
   stiffness matrix C, GPa (order xx yy zz xy yz xz, engineering shear):
                   xx         yy         zz         xy         yz         xz
        xx    0.28563    0.13853    0.13853   -0.00000    0.00000   -0.00001
        yy    0.13853    0.28565    0.13854    0.00000    0.00000   -0.00000
        zz    0.13853    0.13854    0.28564    0.00000    0.00000   -0.00000
        xy   -0.00000    0.00000    0.00000    0.07917    0.00000    0.00000
        yz    0.00000    0.00000    0.00000    0.00000    0.07918    0.00000
        xz   -0.00001   -0.00000   -0.00000    0.00000    0.00000    0.07917
   the same on the CPU ...
   cpu: 813 iterations, converged, void 1e-4, solid share 0.996, 29.8 s
   GPU against CPU: moduli differ by 0.0118 % at most, the GPU is 16.4 times faster -> PASS

everything passed
```

## Reading the output

- `converged`: the solve converged. With `NOT CONVERGED` the answer cannot be trusted.
- `second pass with void 1e-6`: the cell was solved again with a softer void, by the same rule as the whole dataset.
- `solid share` below 0.9: the cell has no load path in at least one direction.
- The moduli Ex, Ey, Ez, Gxy, Gyz, Gxz and the matrix C are in GPa, order xx, yy, zz, xy, yz, xz.
- `stored answer of the file ... PASS`: the difference from the answer stored in the dataset file is at most 0.1 % (usually 0.001-0.01 %).

Exit code 0 means everything passed.

## Common problems

| Message | What to do |
|---|---|
| `CUDA: this PyTorch is a CPU-only build` | install PyTorch built for CUDA (Installation, step 3) |
| `PyTorch is built for CUDA ... but sees no card` | update the NVIDIA driver |
| `reading .hdf5 needs h5py` | `py -m pip install h5py` |
| `out of memory` | a 128³ cell needs about 5 GB of GPU memory, 64³ about 1 GB; use `--device cpu` |

## MatViz3D on Windows

The program looks for Python only in macOS and Linux paths. On Windows, point it to the Python the self-test passed with and restart it:

```
setx MATVIZ_GPU_PYTHON "C:\Users\<name>\AppData\Local\Programs\Python\Python312\python.exe"
```

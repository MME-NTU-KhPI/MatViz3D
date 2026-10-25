# Dataset orchestrator

`make_dataset.py` turns a YAML description of the design space into a table
"geometry + material law → effective properties" by running MatViz3D headless,
one process per cell.

```bash
python3 make_dataset.py plan    dataset.yaml            # sample -> manifest.csv, meta.json
python3 make_dataset.py run     dataset.yaml --jobs 2   # MatViz3D per row; resumable
python3 make_dataset.py collect dataset.yaml            # HDF5 -> dataset.parquet, waves.parquet
python3 make_dataset.py check   dataset.yaml            # sanity checks -> card.md
python3 make_dataset.py status  dataset.yaml            # progress, ETA
```

Output folder (`output_dir` in the YAML):

| file | contents |
|---|---|
| `manifest.csv` | one row per planned cell: id, group, status, seed, law, every `p_*` parameter, exit code, seconds. Also the journal: `run` skips rows that are not `pending`/`retry`. |
| `meta.json` | date, MatViz3D commit, grid size, laws |
| `cells/NNNNNN.hdf5` | the cell: voxels, `C_matrix`, `/1/geometry/*` (achieved density, wall, printability, wave list, law) |
| `logs/NNNNNN.log` | full command line and program output |
| `dataset.parquet` | one row per cell with `E_x/y/z`, `G_*`, `nu_*`, Voigt/Reuss averages, universal anisotropy index, Hashin–Shtrikman ratio, density, wall, printability verdict |
| `waves.parquet` | the geometry vector: `(id, i, kx, ky, kz, amp, phase)` per wave term (Fourier, spinodoid, TPMS anchors) |
| `card.md` | counts per family, pass rates of the checks, value ranges |

Statuses: `done` (exit 0), `unprintable` (exit 3: geometry saved, no stiffness),
`retry` → `failed` (anything else, one retry), `skipped` (the family already has
its quota of accepted cells). A `quota` counts accepted cells; `oversample`
(global or per family) says how many rows to draw for it, and rejected rows cost
~0.3 s each, so a generous oversample is cheap. A cell is regenerated bit-for-bit
from its manifest row (same seed, same parameters).

Config blocks: `common` (parameters for every family), `laws` (assigned round-robin,
`aniso` ≠ 1 uses `--material_ti`), `families` (`quota`, `params`, `flags`).
A parameter is a fixed value, a `[lo, hi]` range (Sobol-sampled; integer bounds
give integers) or `{choices: [...]}`. `oversample` multiplies quotas to cover
printability rejection. `dataset_smoke.yaml` is a ten-cell 32³ check of the
whole pipeline (~3 min).

Dependencies: numpy, scipy, h5py, pandas, pyarrow, pyyaml.


## Adaptive grid

The FFT solve underestimates the stiffness of thin walls: with `v` voxels across
the thinnest wall the error is roughly `350 / v^2` percent (gyroid and Fourier
sheets, 48^3 ... 160^3 study), so a fixed 64^3 grid is fine for struts and far off
for thin sheets and honeycombs. With a `grid:` block in the config every row is
processed in two runs:

1. a probe on `grid.probe`^3 (geometry + printability only, a few seconds) that
   reports the wall thickness in voxels; unprintable rows stop here (status
   `unprintable`);
2. the stiffness solve on the smallest size in `grid.sizes` where the wall spans
   at least `grid.min_wall_vox` voxels; rows that no size resolves get status
   `unresolved` and are not solved.

`grid.pinned` maps a family or group to a fixed size (noise fields are drawn on
the grid, so another grid is another cell). Rows whose family is listed in
`grid.single_cell_families` and that have no grading and no jitter are solved as
ONE unit cell (`--cells_per_edge 1`): periodic homogenisation gives the same
effective properties for the tiled block, and the wall gets `cells_per_edge`
times more voxels for free. The manifest and `dataset.parquet` record `grid`,
`wall_vox` (at the probe grid), `wall_mm_probe` and `solve_cells`; the card lists
the grid shares and checks that every accepted cell is resolved.

# Dataset orchestrator

`make_dataset.py` turns a YAML description of the design space into a table
"geometry + material law → effective properties" by running MatViz3D headless,
one process per cell.

```bash
python3 make_dataset.py plan    dataset.yaml            # sample -> manifest.csv, meta.json
python3 make_dataset.py run     dataset.yaml --jobs 2   # MatViz3D per row; resumable
python3 make_dataset.py top-up  dataset.yaml            # more rows for groups short of their quota
python3 make_dataset.py recheck dataset.yaml --jobs 6   # current checks again on the accepted rows
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
`unresolved` (exit 4, see below), `out_of_band` (exit 5, see below),
`no_load_path` (exit 7, see below; the cell file is kept),
`deferred` (exit 6: `run --defer-above N` probed the row and found that its wall
needs a grid above N³; a later `run` without the flag solves it, `--timeout`
overrides `timeout_s` for that slow pass),
`retry` → `failed` (anything else, one retry), `skipped` (the family already has
its quota of accepted cells). A `quota` counts accepted cells; `oversample`
(global or per family) says how many rows to draw for it, and rejected rows cost
~0.3 s each, so a generous oversample is cheap. A cell is regenerated bit-for-bit
from its manifest row (same seed, same parameters).

Seeds: row k gets cell seed `seed0 + k - 1`, the laws come from a stream seeded
`seed0 + 99`, and group number gi draws its parameters from the Sobol stream
`sobol_seed0 + 1000 gi + 7` (default 0). A Sobol stream always starts with the
same points, so two configs with the same groups and seeds share the first rows of
every group: the pilot (100000 / 0) and the first v1 plan shared 324 of the
pilot's 448 accepted cells by shape. v1 uses 300000 / 300000, so the pilot stays
an independent test set.

Config blocks: `common` (parameters for every family), `laws` (a list assigned
round-robin, or ranges of `E_GPa`, `nu`, `aniso` drawn per row from their own Sobol
sequence; `isotropic_share: 0.2` gives that fraction of the rows `aniso` = 1, a
material that is the same in every direction, by mapping the lowest fifth of the
aniso coordinate to 1, so E and nu are drawn exactly as without it; `aniso` ≠ 1
uses `--material_ti`), `families` (`quota`, `params`, `flags`).
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

`grid.pinned` maps a family or group to a fixed size. The noise families no
longer need it: their white noise is drawn on a fixed 64^3 grid and interpolated,
so the same row is the same foam on 64^3 and 128^3 (overlap 95-98 %, against 21 %
when it was drawn on the cube's own grid; pilot cells made at 64^3 are
bit-identical). Rows whose family is listed in
`grid.single_cell_families` and that have no grading and no jitter are solved as
ONE unit cell (`--cells_per_edge 1`): periodic homogenisation gives the same
effective properties for the tiled block, and the wall gets `cells_per_edge`
times more voxels for free. The manifest and `dataset.parquet` record `grid`,
`wall_vox` (at the probe grid), `wall_mm_probe` and `solve_cells`; the card lists
the grid shares and checks that every accepted cell is resolved.

## Wave cutoff

`max_wave_number: k` keeps the wave table of the families whose shape is the
drawn waves (`spinodoid`, `fourier_sheet`, `fourier_skeletal`) inside one shape
vector: every wave vector with |k| <= k per edge of the solved box (k = 4: 256
numbers, amplitude and phase per ± pair). The probe reads the longest wave; a
longer one ends the row with status `out_of_band` before the solve, and the
manifest and `dataset.parquet` record it as `k_max`. Fourier rows stay inside by
`fourier_kmax <= k`; a spinodoid draws |k| in 0.7-1.3 x its wavelengths per box
(`cells_per_edge`), so 2-3 wavelengths fit k = 4, except when its cones hold
fewer than 5 directions and the band widens to 0.5-2 x. TPMS tables are exact
unit vectors per cell and are not checked.

## Load path

A cell is only a material if its solid joins every neighbouring cell. Two checks
reject the ones that do not; both are on by default.

1. **Before the solve** (probe, part of the printability check): the solid must
   join its own periodic images in three independent directions
   (`print_min_directions`, default 3). A slab joins in two, a rod in one. A
   graded cell whose thin end pinched off, smoothed noise cut into sheets, or a
   carve-out that severed every strut fails here with status `unprintable` and a
   verdict that names the missing direction ("joins its periodic neighbours in 2
   of 3 directions (no path along Z)").
2. **After the solve**: the solid must carry at least `min_load_path_share`
   (default 0.9) of the strain energy in every unit load, after the soft-void
   re-solve. Below that the void holds the cell up and the stiffness in that
   direction is the void's. Status `no_load_path`; the cell file stays, so the row
   can be opened in the dataset browser. It catches what the probe grid cannot
   see when the solve grid differs. Healthy pilot cells carry 0.97 or more, the
   broken ones 0.0003 or less.

`recheck` applies both to rows already accepted, as a fresh run would: it runs the
probe again with the current binary (381 rows in 29 s with `--jobs 6`) and reads
the energy share from the existing cell. Only the manifest changes. On the pilot
it rejected 12 of 381 accepted cells (8 noise_skeletal, 2 fourier, a carved BCC
and a graded Schwarz P), all with E / E_s between 3e-6 and 8e-4 in some
direction.


## Card checks on the moduli

`check` tests every accepted cell against two exact facts rather than against
typical values.

- **Voigt bound.** No modulus of the cell (E_x, E_y, E_z, G_xy, G_yz, G_xz)
  exceeds the volume average of the two phases in that direction:
  rho · solid + (1 − rho) · void, with E_z and the shears through z of the
  printed solid `aniso` times the in-plane ones and the void the solver's soft
  isotropic phase (E = `void_eta` · C11 of the solid, nu 0.3). It holds for any
  geometry; prismatic honeycombs reach it along z (pilot: within 4e-5). The
  Hashin–Shtrikman bound is still written to the table (`E_over_hs`) but is not
  a check: it holds for isotropic cells only, and plate lattices or cubic sheets
  are stiffer along the axes than their mean (14 of 448 pilot cells, all within
  the Voigt bound).
- **E_x = E_y = E_z within 2 %** for cells whose shape is invariant under
  x → y → z and whose solid is isotropic: TPMS, struts, plates, site
  primitives, `cubic_assembly`, Fourier families with `fourier_symmetry cubic`,
  and unions or carve-outs of two of them; graded or jittered cells are left
  out (pilot: 35 cells, largest spread 0.09 %).

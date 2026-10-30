#!/usr/bin/env python3
"""Dataset orchestrator for MatViz3D Lattice cells: geometry + material law -> properties.

Four commands, one config file (YAML), one manifest (CSV) that doubles as the
journal, so an interrupted run resumes where it stopped.

    make_dataset.py plan    dataset.yaml        # sample the design space -> manifest.csv
    make_dataset.py run     dataset.yaml        # run MatViz3D headless for every pending row
    make_dataset.py top-up  dataset.yaml        # more rows for the groups short of their quota
    make_dataset.py recheck dataset.yaml        # post-solve checks again on the accepted rows
    make_dataset.py collect dataset.yaml        # HDF5 files -> dataset.parquet (+ waves.parquet)
    make_dataset.py check   dataset.yaml        # sanity checks -> card.md
    make_dataset.py status  dataset.yaml        # progress and ETA

Row statuses: pending -> done | unprintable (exit 3, geometry kept, no stiffness)
| unresolved (4) | out_of_band (5) | deferred (6) | no_load_path (7: the solid
carries too little of the energy in some load even with the soft void; cell file
kept) | failed (anything else, retried once) | skipped (family quota of accepted
cells already met).

Everything a row needs is in the manifest (parameters, law, seed), so any cell
can be regenerated bit-for-bit from one line of it.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import os
import shutil
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

import numpy as np
import yaml

try:
    from scipy.stats import qmc
except ImportError:  # pragma: no cover
    qmc = None

# ─────────────────────────────────────────────────────────────────────────────
#  Config
# ─────────────────────────────────────────────────────────────────────────────

# Keys that MatViz3D accepts as --key value (everything else in a family block
# is treated as bookkeeping).
PARAM_KEYS = {
    "lattice_family", "cells_per_edge", "relative_density", "lattice_grading", "lattice_jitter",
    "orientation_angle_a", "orientation_angle_b", "orientation_angle_c",
    "spinodoid_theta_x", "spinodoid_theta_y", "spinodoid_theta_z",
    "fourier_terms", "fourier_kmin", "fourier_kmax", "fourier_decay",
    "noise_smooth", "lattice_cell_mm",
    "fourier_symmetry", "plate_hole", "lattice_union",
    "lattice_subtract", "lattice_subtract_size", "lattice_smooth",
    "honeycomb_angle", "honeycomb_rib",
    "assembly_struts", "assembly_plates", "assembly_shells", "assembly_holes", "assembly_spread",
    "foam_mode", "foam_frame", "foam_bubbles",
    "mycelium_mode", "mycelium_length", "mycelium_spores", "mycelium_unit", "mycelium_angle", "mycelium_wiggle",
    "mycelium_fuse", "mycelium_taper", "mycelium_binding", "mycelium_anchors", "mycelium_poor",
    "print_min_wall_mm", "print_max_overhang_deg", "print_max_thin_fraction",
    "print_max_island_vox", "print_max_unsupported_fraction", "print_min_directions",
}
FLAG_KEYS = {"lattice_uniform_wall", "honeycomb_custom"}
INT_KEYS = {"cells_per_edge", "fourier_terms", "print_max_island_vox", "print_min_directions",
            "assembly_struts", "assembly_plates", "assembly_shells", "assembly_holes", "foam_bubbles",
            "mycelium_spores"}
# Families whose shape IS the drawn waves: their wave table is the shape vector the
# geometry network works in, so every wave has to fit its frequency cutoff
# (config `max_wave_number`, |k| per edge of the solved box). The TPMS tables are
# exact unit vectors per cell and are not checked.
WAVE_FAMILIES = {"spinodoid", "fourier_sheet", "fourier_skeletal"}


def load_config(path: Path) -> dict:
    cfg = yaml.safe_load(path.read_text(encoding="utf-8"))
    base = path.parent
    cfg["_config_path"] = str(path)
    out = Path(cfg.get("output_dir", "data"))
    cfg["_out"] = out if out.is_absolute() else (base / out).resolve()
    mv = Path(cfg.get("matviz", "MatViz3D"))
    cfg["_matviz"] = mv if mv.is_absolute() else (base / mv).resolve()
    cfg.setdefault("size", 64)
    cfg.setdefault("fft_tol", 1e-4)
    cfg["fft_tol"] = float(cfg["fft_tol"])          # YAML reads 1e-4 as a string
    cfg.setdefault("fft_max_iter", 3000)
    cfg.setdefault("timeout_s", 1800)
    cfg.setdefault("oversample", 1.0)
    cfg.setdefault("seed0", 1000)
    # Group gi draws its parameters from the Sobol stream sobol_seed0 + 1000 gi + 7.
    # Two datasets with the same groups and sobol_seed0 start every group with the
    # same points; a different sobol_seed0 (and seed0, which sets the cell seeds and
    # the law stream) keeps a held-out set such as the pilot disjoint.
    cfg.setdefault("sobol_seed0", 0)
    cfg.setdefault("common", {})
    cfg.setdefault("laws", [{"id": "petg_ref", "E_GPa": 2.0, "nu": 0.38, "aniso": 1.0}])
    # Adaptive grid (optional). `sizes`: candidate grids per row; the smallest one on
    # which the thinnest wall spans >= `min_wall_vox` voxels is used, measured on a
    # fast `probe` run (geometry + printability only). Families or groups in
    # `pinned` always use the given grid (the smoothed noise needed it while it was
    # drawn on the cube's own grid; it is drawn on a fixed 64^3 grid now, so it
    # takes the adaptive grid like the rest). `single_cell_families` are periodic and, when the
    # row has no grading or jitter, are solved as ONE unit cell instead of the
    # cells_per_edge^3 block: same effective properties, 2-5x the wall resolution.
    grid = cfg.get("grid") or {}
    cfg["grid"] = grid
    grid.setdefault("sizes", [int(cfg["size"])])
    grid["sizes"] = sorted(int(s) for s in grid["sizes"])
    grid.setdefault("min_wall_vox", 0)
    grid.setdefault("probe", int(cfg["size"]))
    grid.setdefault("pinned", {})
    grid.setdefault("single_cell_families", [])
    return cfg


def manifest_path(cfg) -> Path:
    return cfg["_out"] / "manifest.csv"


def read_manifest(cfg) -> list[dict]:
    p = manifest_path(cfg)
    if not p.exists():
        return []
    with p.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def write_manifest(cfg, rows: list[dict]) -> None:
    p = manifest_path(cfg)
    keys: list[str] = []
    for r in rows:
        for k in r:
            if k not in keys:
                keys.append(k)
    tmp = p.with_suffix(".csv.tmp")
    with tmp.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        for r in rows:
            w.writerow(r)
    tmp.replace(p)


# ─────────────────────────────────────────────────────────────────────────────
#  plan: Sobol sampling of every family block
# ─────────────────────────────────────────────────────────────────────────────

def _spec_kind(spec):
    """[lo, hi] -> range; {'choices': [...]} -> choice; scalar -> fixed."""
    if isinstance(spec, dict) and "choices" in spec:
        return "choice"
    if isinstance(spec, (list, tuple)) and len(spec) == 2 and all(isinstance(v, (int, float)) for v in spec):
        return "range"
    return "fixed"


def row_allowed(block: dict, row: dict) -> bool:
    """Constraints from the block's `require` dict. `min_density_x_cell` maps a
    family (or `default`) to the smallest relative_density * lattice_cell_mm the
    row may have: the wall thickness of plate and sheet families is close to
    a_family * density * cell, so this keeps draws out of the unprintable corner
    (walls under 0.8 mm) without narrowing the ranges themselves."""
    req = block.get("require") or {}
    mdc = req.get("min_density_x_cell")
    if mdc:
        fam = str(row.get("lattice_family", ""))
        thr = mdc.get(fam, mdc.get("default")) if isinstance(mdc, dict) else mdc
        if thr is not None:
            try:
                if float(row.get("relative_density", 0)) * float(row.get("lattice_cell_mm", 0)) < float(thr):
                    return False
            except (TypeError, ValueError):
                return False
    return True


def group_sobol_seed(cfg, gi: int) -> int:
    """Seed of the Sobol stream of the gi-th group (plan and top-up must agree)."""
    return int(cfg["sobol_seed0"]) + 1000 * gi + 7


def sample_family(group: str, block: dict, common: dict, n: int, sobol_seed: int) -> list[dict]:
    params = dict(common)
    params.update(block.get("params", {}))
    if "lattice_family" not in params:
        params["lattice_family"] = group
    keys = sorted(params)
    dims = [k for k in keys if _spec_kind(params[k]) != "fixed"]
    # Draw more points than needed: rows that violate the block's `require`
    # constraints are dropped and the sequence simply continues.
    n_draw = 1 << max(0, (4 * n - 1).bit_length())
    if qmc is not None and dims:
        u = qmc.Sobol(d=len(dims), scramble=True, seed=sobol_seed).random(n_draw)
    else:
        u = np.random.default_rng(sobol_seed).random((n_draw, max(1, len(dims))))
    rows = []
    for i in range(n_draw):
        if len(rows) >= n:
            break
        row = {}
        for k in keys:
            spec = params[k]
            kind = _spec_kind(spec)
            if kind == "fixed":
                row[k] = spec
            else:
                x = float(u[i, dims.index(k)])
                if kind == "range":
                    lo, hi = spec
                    if k in INT_KEYS or (isinstance(lo, int) and isinstance(hi, int) and k != "relative_density"):
                        row[k] = int(lo + math.floor(x * (hi - lo + 1)))
                        row[k] = min(row[k], int(hi))
                    else:
                        row[k] = round(lo + x * (hi - lo), 6)
                else:
                    ch = spec["choices"]
                    row[k] = ch[min(len(ch) - 1, int(x * len(ch)))]
        # a spinodoid with every cone closed falls back to isotropic in the
        # program; make that explicit here instead
        if row.get("lattice_family") == "spinodoid":
            if all(float(row.get(f"spinodoid_theta_{a}", 90)) <= 0 for a in "xyz"):
                row["spinodoid_theta_z"] = 90
        for fl in block.get("flags", []):
            row[fl] = True
        if not row_allowed(block, row):
            continue
        rows.append(row)
    if len(rows) < n:
        print(f"  warning: {group}: only {len(rows)} of {n} draws satisfy `require`; widen the ranges or lower the thresholds")
    return rows


def matviz_commit(cfg) -> str:
    """Git commit of the MatViz3D checkout the binary lives in (best effort)."""
    try:
        r = subprocess.run(["git", "-C", str(cfg["_matviz"].parent), "rev-parse", "--short", "HEAD"],
                           capture_output=True, text=True, timeout=10)
        return r.stdout.strip() if r.returncode == 0 else ""
    except Exception:
        return ""


def law_sampler(cfg, skip: int = 0):
    """Law of row k: either a list of named laws assigned round-robin, or a dict
    of ranges {E_GPa: [lo, hi], nu: [lo, hi], aniso: [lo, hi]} sampled per row
    (Sobol, own seed), so the material is a continuous axis of the dataset;
    `isotropic_share: f` gives that fraction of the rows aniso = 1.
    `skip` continues the sequence after that many rows (top-ups)."""
    laws = cfg["laws"]
    law_rng = None
    if isinstance(laws, dict):
        law_rng = qmc.Sobol(d=3, scramble=True, seed=int(cfg["seed0"]) + 99) if qmc is not None else np.random.default_rng(int(cfg["seed0"]) + 99)
        if skip:
            if qmc is not None:
                law_rng.fast_forward(skip)
            else:
                law_rng.random((skip, 3))
    def draw_law(k: int) -> dict:
        if law_rng is None:
            return laws[k % len(laws)]
        u = law_rng.random(1)[0] if qmc is not None else law_rng.random(3)
        def span(key, default):
            r = laws.get(key, default)
            return (r[0] + float(u[["E_GPa", "nu", "aniso"].index(key)]) * (r[1] - r[0])) if isinstance(r, (list, tuple)) else float(r)
        aniso = span("aniso", 1.0)
        # `isotropic_share` of the rows get a material that is the same in every
        # direction (aniso = 1): the lowest share of the aniso coordinate maps to 1,
        # the rest is stretched over the range, so the other coordinates and the
        # low-discrepancy spread stay as they were and the share is exact.
        share, r = float(laws.get("isotropic_share", 0) or 0), laws.get("aniso")
        if share > 0 and isinstance(r, (list, tuple)):
            ua = float(u[2])
            aniso = 1.0 if ua < share else r[0] + (ua - share) / (1.0 - share) * (r[1] - r[0])
        return {"id": "sampled", "E_GPa": round(span("E_GPa", 2.0), 3), "nu": round(span("nu", 0.38), 4), "aniso": round(aniso, 3)}
    return draw_law


def cmd_topup(cfg, args):
    """Append rows for the groups whose accepted cells fall short of their quota,
    and for groups new in the config, without replanning: each group continues
    its own Sobol sequence (the same seed as `plan`, so the new points follow the
    ones already drawn), the laws continue theirs, ids and seeds continue the
    manifest. Rows still pending count by the group's acceptance so far; deferred
    rows do not (they are the slow pass). Needed rows = shortfall / acceptance x
    --factor; the acceptance falls back to 1 / oversample below 10 tried rows.

    `--isotropic N` adds instead, per group, rows whose material is the same in
    every direction (aniso = 1; E and nu continue the law sequence) until N such
    cells are expected to be accepted -- for a dataset planned before
    `isotropic_share`, so it also has the pure-shape examples."""
    rows = read_manifest(cfg)
    if not rows:
        sys.exit("no manifest; run `plan` first")
    only = set(args.groups.split(",")) if args.groups else None
    by: dict[str, list[str]] = {}
    for r in rows:
        by.setdefault(r["group"], []).append(r["status"])
    iso_target = int(getattr(args, "isotropic", 0) or 0)
    is_iso = lambda r: abs(float(r.get("law_aniso") or 1.0) - 1.0) < 1e-9
    draw_law = law_sampler(cfg, skip=len(rows))
    rid = max(int(r["id"]) for r in rows) + 1
    added: list[dict] = []
    report = []
    for gi, (group, block) in enumerate(cfg["families"].items()):
        if only and group not in only:
            continue
        st = by.get(group, [])
        quota = int(block.get("quota", 0))
        done = st.count("done")
        pending = sum(1 for x in st if x in ("pending", "retry"))
        tried = sum(1 for x in st if x in ("done", "unprintable", "unresolved", "out_of_band", "no_load_path", "failed"))
        rate = done / tried if tried >= 10 else 1.0 / float(block.get("oversample", cfg["oversample"]))
        rate = max(rate, 0.1)
        short = quota - done - rate * pending
        if iso_target:
            grp = [r for r in rows if r["group"] == group and is_iso(r)]
            short = iso_target - sum(1 for r in grp if r["status"] == "done") \
                - rate * sum(1 for r in grp if r["status"] in ("pending", "retry"))
        if short <= 0:
            continue
        n = int(math.ceil(short / rate * float(args.factor)))
        points = sample_family(group, block, cfg["common"], len(st) + n, sobol_seed=group_sobol_seed(cfg, gi))[len(st):]
        for p in points:
            law = draw_law(rid - 1)
            if iso_target:
                law = dict(law, aniso=1.0)
            row = {"id": f"{rid:06d}", "group": group, "status": "pending", "seed": int(cfg["seed0"]) + rid - 1,
                   "law_id": law["id"], "law_E_GPa": law["E_GPa"], "law_nu": law["nu"], "law_aniso": law.get("aniso", 1.0)}
            for k, v in p.items():
                row["p_" + k] = v
            added.append(row)
            rid += 1
        report.append((group, quota, done, rate, len(points)))
    for group, quota, done, rate, k in report:
        print(f"  {group:22s} quota {quota:5d}, accepted {done:5d}, acceptance {rate:4.0%} -> {k} new rows"
              + (f" (isotropic, for {iso_target} accepted)" if iso_target else ""))
    if not added:
        print("every group has its quota (or enough pending rows); nothing added")
        return
    write_manifest(cfg, rows + added)
    mp = cfg["_out"] / "meta.json"
    meta = json.loads(mp.read_text(encoding="utf-8")) if mp.exists() else {}
    meta.setdefault("topups", []).append({"at": datetime.now().isoformat(timespec="seconds"), "rows": len(added),
                                          "groups": {g: k for g, _, _, _, k in report}, "factor": float(args.factor),
                                          "matviz": str(cfg["_matviz"]), "matviz_commit": matviz_commit(cfg),
                                          **({"isotropic": iso_target} if iso_target else {})})
    mp.write_text(json.dumps(meta, indent=2, ensure_ascii=False), encoding="utf-8")
    shutil.copyfile(cfg["_config_path"], cfg["_out"] / "dataset.yaml")
    print(f"added {len(added)} rows (ids {added[0]['id']}..{added[-1]['id']}) -> {manifest_path(cfg)}")


def cmd_plan(cfg, args):
    out = cfg["_out"]
    out.mkdir(parents=True, exist_ok=True)
    (out / "cells").mkdir(exist_ok=True)
    (out / "logs").mkdir(exist_ok=True)
    shutil.copyfile(cfg["_config_path"], out / "dataset.yaml")

    draw_law = law_sampler(cfg)
    rows: list[dict] = []
    rid = 1
    seed = int(cfg["seed0"])
    for gi, (group, block) in enumerate(cfg["families"].items()):
        n = int(math.ceil(block.get("quota", 0) * float(block.get("oversample", cfg["oversample"]))))
        if n <= 0:
            continue
        for p in sample_family(group, block, cfg["common"], n, sobol_seed=group_sobol_seed(cfg, gi)):
            law = draw_law(rid - 1)
            row = {"id": f"{rid:06d}", "group": group, "status": "pending", "seed": seed,
                   "law_id": law["id"], "law_E_GPa": law["E_GPa"], "law_nu": law["nu"], "law_aniso": law.get("aniso", 1.0)}
            for k, v in p.items():
                row["p_" + k] = v
            rows.append(row)
            rid += 1
            seed += 1
    write_manifest(cfg, rows)
    meta = {"created": datetime.now().isoformat(timespec="seconds"), "rows": len(rows),
            "size": cfg["size"], "fft_tol": cfg["fft_tol"], "laws": cfg["laws"],
            "seed0": int(cfg["seed0"]), "sobol_seed0": int(cfg["sobol_seed0"]),
            "matviz": str(cfg["_matviz"]), "matviz_commit": matviz_commit(cfg)}
    (out / "meta.json").write_text(json.dumps(meta, indent=2, ensure_ascii=False), encoding="utf-8")
    by = {}
    for r in rows:
        by[r["group"]] = by.get(r["group"], 0) + 1
    print(f"planned {len(rows)} rows -> {manifest_path(cfg)}")
    for g, c in by.items():
        print(f"  {g:18s} {c}")


# ─────────────────────────────────────────────────────────────────────────────
#  run: one MatViz3D process per row
# ─────────────────────────────────────────────────────────────────────────────

def build_command(cfg, row: dict, out_file: Path, size: int | None = None, solve: bool = True,
                  cells_override: int | None = None, void_eta: float | None = None) -> list[str]:
    """MatViz3D command line for one row. `size` overrides the grid, `solve=False`
    stops after geometry + printability, `cells_override` replaces cells_per_edge."""
    cmd = [str(cfg["_matviz"]), "--nogui", "--nologo", "--autostart", "--algorithm", "Lattice",
           "--size", str(size or cfg["size"]), "--seed", str(row["seed"])]
    for k, v in row.items():
        if not k.startswith("p_"):
            continue
        key = k[2:]
        if key in FLAG_KEYS:
            if str(v).lower() in ("true", "1", "yes"):
                # honeycomb_custom is a valued option in the CLI (true|false); the others are bare flags
                cmd += ["--" + key, "true"] if key == "honeycomb_custom" else ["--" + key]
            continue
        if key not in PARAM_KEYS or v in ("", None):
            continue
        cmd += ["--" + key, str(v)]
    E, nu, an = float(row["law_E_GPa"]), float(row["law_nu"]), float(row.get("law_aniso", 1.0))
    if abs(an - 1.0) < 1e-9:
        cmd += ["--material_iso", f"{E},{nu}"]
    else:
        cmd += ["--material_ti", f"{E},{nu},{an}"]
    if cells_override:
        if "--cells_per_edge" in cmd:
            cmd[cmd.index("--cells_per_edge") + 1] = str(cells_override)
        else:
            cmd += ["--cells_per_edge", str(cells_override)]
    cmd += ["--require_printable", "--no_fields"]
    if solve:
        cmd += ["--run_stress_calc", "--solver", "fft", "--stress_mode", "stiffness",
                "--fft_tol", str(cfg["fft_tol"]), "--fft_max_iter", str(cfg["fft_max_iter"])]
        eta = void_eta if void_eta is not None else cfg.get("fft_void_eta")
        if eta:
            cmd += ["--fft_void_eta", str(eta)]
        if cfg.get("fft_discretization"):
            cmd += ["--fft_discretization", str(cfg["fft_discretization"])]
    if cfg.get("np"):
        cmd += ["--np", str(cfg["np"])]
    cmd += ["--output", str(out_file)]
    return cmd


def single_cell(cfg, row: dict) -> bool:
    """True when the row is a periodic lattice (deterministic family, no grading,
    no jitter) tiled more than once: it is then solved as a single unit cell."""
    fam = str(row.get("p_lattice_family", ""))
    if fam not in set(cfg["grid"]["single_cell_families"]):
        return False
    try:
        cells = int(float(row.get("p_cells_per_edge") or 1))
        grading = float(row.get("p_lattice_grading") or 0)
        jitter = float(row.get("p_lattice_jitter") or 0)
    except (TypeError, ValueError):
        return False
    return cells > 1 and grading == 0 and jitter == 0


def choose_grid(cfg, row: dict, wall_vox: float):
    """Grid for the stiffness solve, or None when no candidate resolves the wall."""
    g = cfg["grid"]
    fam = str(row.get("p_lattice_family", ""))
    pinned = g["pinned"].get(fam) or g["pinned"].get(row.get("group", ""))
    candidates = [int(pinned)] if pinned else g["sizes"]
    for n in candidates:
        if wall_vox * n / float(g["probe"]) >= float(g["min_wall_vox"]):
            return n
    return None


def max_wave_number(h5path: Path) -> float | None:
    """Longest wave vector |k| in the file's wave table, None when there is no table."""
    import h5py
    with h5py.File(h5path, "r") as f:
        ds = f.get("1/geometry/lattice_wave_terms")
        if ds is None:
            return None
        w = np.asarray(ds[()], dtype=float).reshape(-1, 5)
    if not len(w):
        return None
    return float(np.sqrt((w[:, :3] ** 2).sum(axis=1)).max())


def out_of_band(cfg, row: dict, k_max) -> bool:
    """True when a wave-family row has a wave longer than `max_wave_number`. A spinodoid
    draws |k| in 0.7-1.3 x its wavelengths per box but widens that to 0.5-2 x when its
    cones hold fewer than 5 directions, so the cap is checked on the generated waves."""
    cap = cfg.get("max_wave_number")
    if not cap or k_max in (None, ""):
        return False
    if str(row.get("p_lattice_family", "")) not in WAVE_FAMILIES:
        return False
    return float(k_max) > float(cap) + 1e-9


def _run_logged(cmd: list[str], log_file: Path, timeout: float, mode: str) -> int:
    try:
        with log_file.open(mode, encoding="utf-8") as lf:
            lf.write("# " + " ".join(cmd) + "\n\n")
            lf.flush()
            proc = subprocess.run(cmd, stdout=lf, stderr=subprocess.STDOUT, timeout=timeout)
        return proc.returncode
    except subprocess.TimeoutExpired:
        return -1
    except OSError as e:
        with log_file.open("a", encoding="utf-8") as lf:
            lf.write(f"\n# could not start: {e}\n")
        return -2


def run_one(cfg, row: dict) -> tuple[int, float, dict]:
    """Run MatViz3D for one manifest row; returns (exit code, seconds, info).

    With an adaptive grid this is two runs: a fast probe (geometry + printability
    on the probe grid) that yields the wall thickness in voxels, then the
    stiffness solve on the grid that resolves that wall. Exit code 3 = the
    printability check failed, 4 = no candidate grid resolves the wall,
    5 = a wave-family row has a wave longer than `max_wave_number`,
    6 = the wall needs a grid above `run --defer-above` (row deferred, not solved),
    7 = after the solve the solid carries too little of the energy in some load.
    """
    out = cfg["_out"]
    out_file = out / "cells" / f"{row['id']}.hdf5"
    log_file = out / "logs" / f"{row['id']}.log"
    if out_file.exists():
        out_file.unlink()                 # MatViz3D appends groups; start clean
    g = cfg["grid"]
    adaptive = float(g["min_wall_vox"]) > 0 or len(g["sizes"]) > 1
    cells = 1 if single_cell(cfg, row) else None
    info: dict = {"solve_cells": cells or row.get("p_cells_per_edge") or 1}
    t0 = time.time()
    size = int(cfg["size"])
    if adaptive:
        probe_file = out / "probe" / f"{row['id']}.hdf5"
        probe_file.parent.mkdir(parents=True, exist_ok=True)
        if probe_file.exists():
            probe_file.unlink()
        code = _run_logged(build_command(cfg, row, probe_file, size=int(g["probe"]), solve=False, cells_override=cells),
                           log_file, min(float(cfg["timeout_s"]), 600.0), "w")
        if code != 0:
            return code, time.time() - t0, info
        try:
            import h5py
            with h5py.File(probe_file, "r") as f:
                info["wall_vox"] = round(float(f["1/geometry/lattice_wall_voxels"][()]), 2)
                info["wall_mm_probe"] = round(float(f["1/geometry/lattice_wall_mm"][()]), 3)
            km = max_wave_number(probe_file)
            if km is not None:
                info["k_max"] = round(km, 3)
        except Exception as e:
            with log_file.open("a", encoding="utf-8") as lf:
                lf.write(f"\n# probe unreadable: {e}\n")
            return -3, time.time() - t0, info
        finally:
            if probe_file.exists():
                probe_file.unlink()
        if out_of_band(cfg, row, info.get("k_max")):
            info["grid"] = ""
            return 5, time.time() - t0, info
        chosen = choose_grid(cfg, row, info["wall_vox"])
        if chosen is None:
            info["grid"] = ""
            return 4, time.time() - t0, info
        size = chosen
        defer = cfg.get("_defer_above")
        if defer and size > int(defer):
            info["grid"] = size            # the grid it will need, for the later pass
            return 6, time.time() - t0, info
    info["grid"] = size
    code = _run_logged(build_command(cfg, row, out_file, size=size, solve=True, cells_override=cells),
                       log_file, float(cfg["timeout_s"]), "a" if adaptive else "w")
    if code == 0 and not adaptive and cfg.get("max_wave_number"):
        try:
            km = max_wave_number(out_file)
        except Exception:
            km = None
        if km is not None:
            info["k_max"] = round(km, 3)
        if out_of_band(cfg, row, km):
            out_file.unlink(missing_ok=True)
            return 5, time.time() - t0, info
    info["void_eta"] = cfg.get("fft_void_eta", "")
    # Adaptive void stiffness: a stiff phase-like void (1e-4) converges fast and is
    # exact for most cells; when the solid carries less than `min_solid_energy_share`
    # of the strain energy in some unit load, the void is propping the cell up. The
    # row is then solved again with `fft_void_eta_fallback`. With
    # `void_extrapolate: true` the two solves are combined by Richardson
    # extrapolation to a stiffness-free void, C0 = (r C(eta2) - C(eta1)) / (r - 1),
    # r = eta1/eta2: the void's contribution is linear in its stiffness (re-entrant
    # honeycomb: 15.18 and 7.55 MPa at 1e-4 and 1e-5 give 6.70; a direct 1e-6 solve
    # gives 6.73, 1e-8 gives 6.63), and 1e-5 costs a third of the iterations of 1e-6.
    fb, thr = cfg.get("fft_void_eta_fallback"), cfg.get("min_solid_energy_share")
    # On the expensive grids the soft-void re-solve costs 1-2 h per cell while the void
    # error at a share of ~0.97 is a few percent, about the grid error itself, so the
    # threshold can be lower there: min_solid_energy_share_by_grid: {128: 0.97}.
    by_grid = cfg.get("min_solid_energy_share_by_grid") or {}
    thr = by_grid.get(size, by_grid.get(str(size), thr))
    if code == 0 and fb and thr:
        import h5py
        import numpy as np
        try:
            with h5py.File(out_file, "r") as f:
                share = min(float(x) for x in f["1/geometry/solid_energy_share_loads"][()])
                C1 = np.array(f["1/C_matrix"][()], dtype=float)
        except Exception:
            share, C1 = float("nan"), None
        info["energy_share_first"] = round(share, 4) if share == share else ""
        if not (share >= float(thr)):
            first = out_file.with_name(out_file.stem + ".void1.hdf5")
            if out_file.exists():
                out_file.replace(first)
            eta1, eta2 = float(cfg.get("fft_void_eta") or 1e-4), float(fb)
            code = _run_logged(build_command(cfg, row, out_file, size=size, solve=True, cells_override=cells, void_eta=eta2),
                               log_file, float(cfg["timeout_s"]), "a")
            info["void_eta"] = fb
            if code == 0 and cfg.get("void_extrapolate") and C1 is not None:
                try:
                    with h5py.File(out_file, "r+") as f:
                        C2 = np.array(f["1/C_matrix"][()], dtype=float)
                        r = eta1 / eta2
                        C0 = (r * C2 - C1) / (r - 1.0)
                        Cs = 0.5 * (C0 + C0.T)
                        if np.all(np.linalg.eigvalsh(Cs) > 0) and np.all(np.diag(C0) > 0):
                            S0 = np.linalg.inv(C0)
                            f["1/C_matrix"][...] = C0.astype(np.float32)
                            if "1/S_matrix" in f:
                                f["1/S_matrix"][...] = S0.astype(np.float32)
                            if "1/P_matrix" in f:
                                P = -S0 / np.diag(S0)[None, :]
                                f["1/P_matrix"][...] = P.astype(np.float32)
                            if "1/Effective_Moduli" in f:
                                f["1/Effective_Moduli"][...] = (1.0 / np.diag(S0)).astype(np.float32)
                            for name, M in ((f"C_matrix_void_{eta1:g}", C1), (f"C_matrix_void_{eta2:g}", C2)):
                                if name in f["1"]:
                                    del f["1"][name]
                                f["1"].create_dataset(name, data=M.astype(np.float32))
                            f["1/geometry"].attrs["void_extrapolated"] = 1
                            info["void_extrap"] = 1
                        else:
                            info["void_extrap"] = 0          # extrapolation not positive definite: keep C(eta2)
                except Exception as e:
                    with log_file.open("a", encoding="utf-8") as lf:
                        lf.write(f"\n# void extrapolation failed: {e}\n")
                    info["void_extrap"] = 0
            if first.exists():
                first.unlink()
    if code == 0:
        share = final_energy_share(out_file)
        if share == share:
            info["energy_share_final"] = round(share, 4)
        if no_load_path(cfg, share):
            return 7, time.time() - t0, info      # the file stays, for a look in the browser
    return code, time.time() - t0, info


def final_energy_share(h5) -> float:
    """Smallest share of the strain energy the solid carries over the six unit loads."""
    try:
        import h5py
        with h5py.File(h5, "r") as f:
            return min(float(x) for x in f["1/geometry/solid_energy_share_loads"][()])
    except Exception:
        return float("nan")


def no_load_path(cfg, share: float) -> bool:
    """After the soft-void re-solve the solid still carries less than
    `min_load_path_share` (0.9) of the energy in some load: there is no solid
    path through the cell in that direction and the void holds it up -- a graded
    cell whose thin end pinched off, noise cut into sheets, a carve-out that
    severed every strut. Its stiffness in that direction is the void's, not the
    material's. A healthy cell carries > 0.97 after the re-solve."""
    thr = cfg.get("min_load_path_share", 0.9)
    return bool(thr) and share == share and share < float(thr)


def quotas_of(cfg) -> dict[str, int]:
    return {g: int(b.get("quota", 0)) for g, b in cfg["families"].items()}


def run_pass(cfg, rows: list[dict], todo: list[dict], jobs: int, label: str) -> None:
    """Run `todo` with `jobs` worker threads (one MatViz3D process each).

    A family's quota counts ACCEPTED cells (status done). Once it is met the
    family's remaining rows are marked `skipped` instead of being solved, so
    printability rejection does not unbalance the dataset and no time goes
    into cells nobody asked for.
    """
    import queue
    import threading
    quotas = quotas_of(cfg)
    q: "queue.Queue[dict]" = queue.Queue()
    for r in todo:
        q.put(r)
    lock = threading.Lock()
    t_all = time.time()
    counter = {"n": 0}

    def accepted(group: str) -> int:
        return sum(1 for r in rows if r["group"] == group and r["status"] == "done")

    def worker():
        while True:
            try:
                row = q.get_nowait()
            except queue.Empty:
                return
            group = row["group"]
            with lock:
                if quotas.get(group) and accepted(group) >= quotas[group] and not cfg.get("_ignore_quota"):
                    row["status"] = "skipped"
                    write_manifest(cfg, rows)
                    continue
            code, secs, info = run_one(cfg, row)
            if code == 0:
                status = "done"
            elif code == 3:
                status = "unprintable"
            elif code == 4:
                status = "unresolved"
            elif code == 5:
                status = "out_of_band"
            elif code == 6:
                status = "deferred"
            elif code == 7:
                status = "no_load_path"
            else:
                status = "failed" if row["status"] == "retry" else "retry"
            with lock:
                counter["n"] += 1
                row.update(info)
                row.update({"status": status, "exit_code": code, "seconds": round(secs, 1),
                            "finished": datetime.now().isoformat(timespec="seconds")})
                write_manifest(cfg, rows)
                finished = sum(1 for r in rows if r["status"] not in ("pending", "retry", "deferred"))
                # rows still to do: pending / retry, plus the deferred ones this pass solves
                left = sum(1 for r in rows if r["status"] in ("pending", "retry")) \
                    + (0 if cfg.get("_defer_above") else sum(1 for r in todo if r["status"] == "deferred"))
                per_row = (time.time() - t_all) / counter["n"]
                print(f"{label}[{counter['n']}/{len(todo)}] {row['id']} {group:16s} -> {status:11s} {secs:6.1f}s "
                      f"grid {info.get('grid', cfg['size'])} wall {info.get('wall_vox', '?')} vox | "
                      f"accepted {accepted(group)}/{quotas.get(group, '?')}, finished {finished}/{len(rows)}, "
                      f"ETA <= {left * per_row / 3600:.2f} h", flush=True)

    threads = [threading.Thread(target=worker, daemon=True) for _ in range(jobs)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()


def interleaved(rows: list[dict], todo: list[dict]) -> list[dict]:
    """`todo` with every group advancing in proportion to its planned rows (row k
    of a group of n at (k - 1/2) / n), each group in its own manifest order. The
    manifest holds the groups one after another, so in id order a run stopped
    half way has only the first groups; interleaved, it has all of them, each
    with the leading points of its Sobol sequence."""
    pos, size = {}, {}
    for r in rows:
        size[r["group"]] = size.get(r["group"], 0) + 1
        pos[r["id"]] = size[r["group"]]
    return sorted(todo, key=lambda r: ((pos[r["id"]] - 0.5) / size[r["group"]], r["id"]))


def cmd_run(cfg, args):
    rows = read_manifest(cfg)
    if not rows:
        sys.exit("no manifest; run `plan` first")
    if not cfg["_matviz"].exists():
        sys.exit(f"MatViz3D binary not found: {cfg['_matviz']}")
    jobs = max(1, int(args.jobs))
    if jobs > 1 and not cfg.get("np"):
        cfg["np"] = max(1, (os.cpu_count() or 2) // jobs)   # share the cores between processes
    if args.timeout:
        cfg["timeout_s"] = float(args.timeout)
    if args.ignore_quota:
        cfg["_ignore_quota"] = True          # solve every row of the pass, quota met or not
    # --defer-above N: a fast pass. Rows whose wall needs a grid above N^3 are
    # probed (seconds) and marked `deferred` instead of being solved; a later run
    # without the flag picks them up together with the pending ones.
    defer = int(args.defer_above or 0)
    if defer:
        cfg["_defer_above"] = defer
    statuses = ("pending", "retry") if defer else ("pending", "retry", "deferred")
    todo = [r for r in rows if r["status"] in statuses]
    if args.order == "interleave":
        todo = interleaved(rows, todo)
    if args.limit:
        todo = todo[: args.limit]
    finished = sum(1 for r in rows if r["status"] in ("done", "unprintable", "unresolved", "out_of_band", "no_load_path", "failed"))
    g = cfg["grid"]
    grid_note = (f"adaptive grid {g['sizes']} (>= {g['min_wall_vox']} voxels per wall, probe {g['probe']}^3)"
                 if float(g["min_wall_vox"]) > 0 or len(g["sizes"]) > 1 else f"grid {cfg['size']}^3")
    print(f"{len(todo)} rows this pass, {finished} finished before, {len(rows)} total, "
          f"{jobs} parallel job(s) x {cfg.get('np') or 'all'} threads, {grid_note}, fft_tol {cfg['fft_tol']}, "
          f"timeout {cfg['timeout_s']:.0f} s" + (f", rows needing more than {defer}^3 deferred" if defer else ""))
    if not todo:
        return
    run_pass(cfg, rows, todo, jobs, "")
    retries = [r for r in todo if r["status"] == "retry"]
    if retries and not args.no_retry:
        print(f"retrying {len(retries)} rows once")
        run_pass(cfg, rows, retries, jobs, "retry ")


# ─────────────────────────────────────────────────────────────────────────────
#  collect: HDF5 -> one row of properties
# ─────────────────────────────────────────────────────────────────────────────

def _h5_str(ds):
    v = ds[()]
    if isinstance(v, bytes):
        return v.decode(errors="ignore")
    try:
        return v.tobytes().decode(errors="ignore").rstrip("\x00")
    except Exception:
        return str(v)


def hs_upper_E(E_s: float, nu_s: float, porosity: float) -> float:
    """Hashin-Shtrikman upper bound on the Young's modulus of an isotropic porous
    solid (void = second phase with zero stiffness)."""
    if not (0.0 <= porosity < 1.0) or E_s <= 0:
        return float("nan")
    K = E_s / (3 * (1 - 2 * nu_s))
    G = E_s / (2 * (1 + nu_s))
    f = porosity
    K_hs = K + f / (-1.0 / K + 3 * (1 - f) / (3 * K + 4 * G))
    G_hs = G + f / (-1.0 / G + 6 * (1 - f) * (K + 2 * G) / (5 * G * (3 * K + 4 * G)))
    return 9 * K_hs * G_hs / (3 * K_hs + G_hs)


def solid_c11(E, nu, aniso):
    """C11 of the printed solid (Parameters::lawStiffnessPa: transversely
    isotropic about z, E_z = aniso E, G_yz = G_xz = aniso G) -- the stiffness the
    FFT solver scales its void phase by (void E = fft_void_eta x C11, nu 0.3).
    Element-wise over arrays."""
    E, nu, r = (np.asarray(v, dtype=float) for v in (E, nu, aniso))
    E, nu, r = np.broadcast_arrays(E, nu, r)
    S = np.zeros(E.shape + (3, 3))
    S[..., 0, 0] = S[..., 1, 1] = 1 / E
    S[..., 2, 2] = 1 / (r * E)
    S[..., 0, 1] = S[..., 1, 0] = S[..., 0, 2] = S[..., 2, 0] = S[..., 1, 2] = S[..., 2, 1] = -nu / E
    ok = np.isfinite(S).all(axis=(-2, -1))
    S[~ok] = np.eye(3)
    C11 = np.linalg.inv(S)[..., 0, 0]   # the normal block alone: shears decouple
    return np.where(ok, C11, np.nan)


def lattice_props(h5path: Path) -> tuple[dict, list[list[float]]]:
    """One dataset row from one MatViz3D HDF5 file, plus the wave list (may be empty).

    Conventions: C_matrix is in the program's pipeline Voigt order
    [xx, yy, zz, xy, yz, xz] with TENSOR shear strain, so 1/S_ii on a shear
    entry is twice the shear modulus; G below is corrected for that.
    """
    import h5py
    row: dict = {}
    waves: list[list[float]] = []
    with h5py.File(h5path, "r") as f:
        g = f["1/geometry"]
        for k, ds in g.items():
            if k in ("parameters_json", "parameters_summary", "lattice_wave_terms"):
                continue
            try:
                v = ds[()]
            except Exception:
                continue
            if getattr(ds, "dtype", None) is not None and ds.dtype.kind in ("S", "O", "i") and ds.shape != ():
                row[k] = _h5_str(ds)
            elif ds.shape == ():
                row[k] = float(v) if hasattr(v, "__float__") else _h5_str(ds)
        if "lattice_wave_terms" in g:
            waves = [list(map(float, w)) for w in g["lattice_wave_terms"][()]]
        row["n_waves"] = len(waves)
        if "1/C_matrix" in f:
            C = np.array(f["1/C_matrix"][()], dtype=float)
            S = np.linalg.inv(C)
            Ex, Ey, Ez = 1 / S[0, 0], 1 / S[1, 1], 1 / S[2, 2]
            row.update({
                "E_x": Ex, "E_y": Ey, "E_z": Ez,
                "G_xy": 0.5 / S[3, 3], "G_yz": 0.5 / S[4, 4], "G_xz": 0.5 / S[5, 5],
                "nu_xy": -S[1, 0] / S[0, 0], "nu_xz": -S[2, 0] / S[0, 0], "nu_yz": -S[2, 1] / S[1, 1],
                "E_ratio": max(Ex, Ey, Ez) / max(min(Ex, Ey, Ez), 1e-30),
            })
            # Voigt / Reuss averages and the universal anisotropy index A_U
            # (0 = isotropic), with the shear entries moved to engineering form.
            C44, C55, C66 = C[3, 3] / 2, C[4, 4] / 2, C[5, 5] / 2
            S44, S55, S66 = 2 * S[3, 3], 2 * S[4, 4], 2 * S[5, 5]
            KV = (C[0, 0] + C[1, 1] + C[2, 2] + 2 * (C[0, 1] + C[0, 2] + C[1, 2])) / 9
            GV = (C[0, 0] + C[1, 1] + C[2, 2] - C[0, 1] - C[0, 2] - C[1, 2] + 3 * (C44 + C55 + C66)) / 15
            KR = 1 / (S[0, 0] + S[1, 1] + S[2, 2] + 2 * (S[0, 1] + S[0, 2] + S[1, 2]))
            GR = 15 / (4 * (S[0, 0] + S[1, 1] + S[2, 2]) - 4 * (S[0, 1] + S[0, 2] + S[1, 2]) + 3 * (S44 + S55 + S66))
            row.update({"K_voigt": KV, "G_voigt": GV, "K_reuss": KR, "G_reuss": GR,
                        "A_universal": 5 * GV / GR + KV / KR - 6 if GR > 0 and KR > 0 else float("nan")})
            E_s = float(row.get("mat_e_gpa", 0)) * 1e9
            nu_s = float(row.get("mat_nu", 0.38))
            rho = float(row.get("lattice_achieved_density", float("nan")))
            if E_s > 0 and not math.isnan(rho):
                row["E_hs_upper"] = hs_upper_E(E_s, nu_s, 1 - rho)
                row["E_over_Es"] = (Ex + Ey + Ez) / 3 / E_s
                row["E_over_hs"] = (Ex + Ey + Ez) / 3 / row["E_hs_upper"] if row["E_hs_upper"] > 0 else float("nan")
            if "1/iterations_total" in f:
                row["fft_iterations"] = int(f["1/iterations_total"][()])
        row["has_stiffness"] = "1/C_matrix" in f
        try:
            row["connected"] = int(float(row.get("print_components", 1))) == 1
        except (TypeError, ValueError):
            row["connected"] = True
        if "print_connected_directions" in row:
            try:
                row["joined_3d"] = int(float(row["print_connected_directions"])) == 3
            except (TypeError, ValueError):
                pass
    return row, waves


def cmd_collect(cfg, args):
    import pandas as pd
    rows = read_manifest(cfg)
    out = cfg["_out"]
    records, wave_records = [], []
    for r in rows:
        if r["status"] not in ("done", "unprintable", "no_load_path"):
            continue
        h5 = out / "cells" / f"{r['id']}.hdf5"
        if not h5.exists():
            continue
        try:
            props, waves = lattice_props(h5)
        except Exception as e:  # keep going, note the problem
            props, waves = {"collect_error": str(e)}, []
        rec = {"id": r["id"], "group": r["group"], "status": r["status"], "seed": int(r["seed"]),
               "law_id": r["law_id"], "law_E_GPa": float(r["law_E_GPa"]), "law_nu": float(r["law_nu"]),
               "law_aniso": float(r.get("law_aniso", 1.0)), "seconds": float(r.get("seconds") or 0)}
        for k, v in r.items():
            if k.startswith("p_"):
                rec[k] = v
        for k in ("grid", "wall_vox", "wall_mm_probe", "solve_cells", "void_eta", "energy_share_first", "energy_share_final", "void_extrap", "k_max"):
            if r.get(k) not in (None, ""):
                try:
                    rec[k] = int(float(r[k])) if k in ("grid", "solve_cells", "void_extrap") else float(r[k])
                except ValueError:
                    pass
        rec.update(props)
        rec["file"] = str(h5.relative_to(out))
        records.append(rec)
        for i, w in enumerate(waves):
            wave_records.append({"id": r["id"], "i": i, "kx": w[0], "ky": w[1], "kz": w[2], "amp": w[3], "phase": w[4]})
    df = pd.DataFrame(records)
    df.to_parquet(out / "dataset.parquet", index=False)
    pd.DataFrame(wave_records).to_parquet(out / "waves.parquet", index=False)
    print(f"collected {len(df)} rows ({int(df['has_stiffness'].sum()) if len(df) else 0} with stiffness), "
          f"{len(wave_records)} wave terms -> {out / 'dataset.parquet'}")


# ─────────────────────────────────────────────────────────────────────────────
#  check: sanity report
# ─────────────────────────────────────────────────────────────────────────────

def cmd_check(cfg, args):
    import pandas as pd
    out = cfg["_out"]
    rows = read_manifest(cfg)
    df = pd.read_parquet(out / "dataset.parquet") if (out / "dataset.parquet").exists() else pd.DataFrame()
    meta = json.loads((out / "meta.json").read_text(encoding="utf-8")) if (out / "meta.json").exists() else {}
    # a top-up made with a newer binary (new families) records its commit
    later = [f"{t['matviz_commit']} (top-up {t['at'][:10]}: {', '.join(t['groups'])})"
             for t in meta.get("topups", []) if t.get("matviz_commit") and t["matviz_commit"] != meta.get("matviz_commit")]
    lines = [f"# Dataset card — {out.name}", "",
             f"generated {datetime.now().isoformat(timespec='minutes')}; planned {meta.get('created', '?')}; "
             f"MatViz3D commit {meta.get('matviz_commit') or '?'}" + "".join(f", then {c}" for c in later)
             + f"; grid {cfg['size']}^3; FFT tol {cfg['fft_tol']}", "",
             ("Material laws: " + ("; ".join(f"{l['id']} (E={l['E_GPa']} GPa, nu={l['nu']}, aniso={l.get('aniso', 1.0)})" for l in cfg["laws"]) if isinstance(cfg["laws"], list) else f"sampled per row: {cfg['laws']}")), ""]
    st = {}
    for r in rows:
        st[r["status"]] = st.get(r["status"], 0) + 1
    lines += ["## Manifest", "", "| status | rows |", "|---|---|"] + [f"| {k} | {v} |" for k, v in sorted(st.items())] + [""]
    if len(df):
        quotas = quotas_of(cfg)
        st_rows = {}
        for r in rows:
            st_rows.setdefault(r["group"], {}).setdefault(r["status"], 0)
            st_rows[r["group"]][r["status"]] += 1
        lines += ["## By family", "", "| group | quota | accepted | unprintable | unresolved | out of band | no load path | unprintable share | mean s/accepted cell |", "|---|---|---|---|---|---|---|---|---|"]
        for g, d in df.groupby("group"):
            acc = d[d["status"] == "done"]
            stg = st_rows.get(g, {})          # manifest counts: unprintable probes write no HDF5
            unpr, unres, oob = stg.get("unprintable", 0), stg.get("unresolved", 0), stg.get("out_of_band", 0)
            nlp = stg.get("no_load_path", 0)
            tried = sum(v for k, v in stg.items() if k not in ("pending", "retry", "skipped"))
            lines.append(f"| {g} | {quotas.get(g, '?')} | {len(acc)} | {unpr} | {unres} | {oob} | {nlp} | "
                         f"{(unpr / tried if tried else 0):.0%} | {acc['seconds'].mean() if len(acc) else float('nan'):.0f} |")
        lines.append("")
        if "grid" in df:
            acc = df[df["status"] == "done"]
            lines += ["## Grids (accepted cells)", "", "| grid | cells | share | mean s/cell | min wall vox at that grid |", "|---|---|---|---|---|"]
            gcfg = cfg["grid"]
            for gsize, d in acc.groupby("grid"):
                wv = pd.to_numeric(d["wall_vox"], errors="coerce") * float(gsize) / float(gcfg["probe"]) if "wall_vox" in d else pd.Series(dtype=float)
                lines.append(f"| {int(gsize)}³ | {len(d)} | {len(d)/max(len(acc),1):.0%} | {d['seconds'].mean():.0f} | {wv.min() if len(wv) else float('nan'):.1f} |")
            lines.append("")
        s = df[(df["has_stiffness"] == True) & (df["status"] == "done")]
        checks = []   # (name, boolean Series over the rows the check applies to)
        if len(s):
            num = lambda col, default=0.0: pd.to_numeric(s[col], errors="coerce").fillna(default) if col in s else pd.Series(default, index=s.index)
            checks.append(("all moduli positive", (s[["E_x", "E_y", "E_z", "G_xy", "G_yz", "G_xz"]] > 0).all(axis=1)))
            # Voigt bound C <= rho C_s + (1 - rho) C_void, read on the diagonal of
            # the compliance: no modulus of the cell exceeds the volume average
            # of the two phases' moduli in that direction (E_z and the shears
            # through z of the printed solid are aniso times the in-plane ones;
            # the void is the solver's soft isotropic phase). It holds for any
            # geometry -- the Hashin-Shtrikman bound only for isotropic cells,
            # and plates or cubic sheets are stiffer along the axes than their
            # mean. Prismatic honeycombs reach it along z.
            law = lambda m, l, d: num(m, float("nan")).where(num(m, float("nan")).notna(), num(l, d))   # solved with, else drawn
            if "lattice_achieved_density" in s:
                E_s, nu_s, r = law("mat_e_gpa", "law_E_GPa", float("nan")) * 1e9, law("mat_nu", "law_nu", 0.38), law("mat_aniso", "law_aniso", 1.0)
                G_s = E_s / (2 * (1 + nu_s))
                E_v = num("void_eta", float(cfg.get("fft_void_eta", 1e-4))) * pd.Series(solid_c11(E_s, nu_s, r), index=s.index)
                G_v = E_v / (2 * 1.3)
                rho = num("lattice_achieved_density", float("nan"))
                bound = {"E_x": (E_s, E_v), "E_y": (E_s, E_v), "E_z": (r * E_s, E_v),
                         "G_xy": (G_s, G_v), "G_yz": (r * G_s, G_v), "G_xz": (r * G_s, G_v)}
                voigt = pd.concat([num(k, float("nan")) / (rho * m_s + (1 - rho) * m_v) for k, (m_s, m_v) in bound.items()], axis=1).max(axis=1)
                s = s.assign(voigt_ratio=voigt)
                checks.append(("every modulus within the Voigt bound (volume average of solid and void, x1.001)", voigt.dropna() <= 1.001))
            if "E_over_Es" in s:
                checks.append(("E/E_s within (0, 1)", (s["E_over_Es"] > 0) & (s["E_over_Es"] < 1)))
            # Ex = Ey = Ez needs a shape invariant under x -> y -> z and an
            # isotropic solid. Every family below is (incl. unions and carve-outs
            # of two of them), the Fourier families with cubic orbits too;
            # grading and jitter break it.
            cubic_families = {"gyroid_sheet", "gyroid_skeletal", "schwarz_p_sheet", "schwarz_p_skeletal", "schwarz_d_sheet",
                              "schwarz_d_skeletal", "iwp_sheet", "iwp_skeletal", "simple_cubic", "bcc", "fcc", "octet",
                              "plate_sc", "plate_bcc", "plate_fcc", "plate_sc_bcc", "plate_sc_fcc", "plate_bcc_fcc", "plate_sc_bcc_fcc",
                              "spheres_sc", "spheres_bcc", "spheres_fcc", "cylinders_axes", "cubic_assembly",
                              "foam_kelvin", "foam_rhombic", "foam_weaire_phelan"}
            text = lambda col: s[col].fillna("").astype(str) if col in s else pd.Series("", index=s.index)
            fam, sym = text("p_lattice_family"), text("p_fourier_symmetry")
            shape_cubic = fam.isin(cubic_families) | (fam.isin({"fourier_sheet", "fourier_skeletal"}) & (sym == "cubic"))
            for extra in ("p_lattice_union", "p_lattice_subtract"):
                shape_cubic &= (text(extra) == "") | text(extra).isin(cubic_families)
            cubic_mask = shape_cubic & (law("mat_aniso", "law_aniso", 1.0) == 1.0) \
                & (num("p_lattice_grading") == 0) & (num("p_lattice_jitter") == 0)
            if cubic_mask.any():
                checks.append(("cubic cells, isotropic solid: E_x = E_y = E_z within 2 %", s.loc[cubic_mask, "E_ratio"] < 1.02))
            if "fft_iterations" in s:
                checks.append(("FFT converged (iterations < 6 x cap)", num("fft_iterations") < 6 * float(cfg["fft_max_iter"])))
            if "lattice_achieved_density" in s and "p_relative_density" in s:
                checks.append(("achieved density within 0.03 of target", (num("lattice_achieved_density") - num("p_relative_density")).abs() <= 0.03))
            weak = s[["E_x", "E_y", "E_z"]].min(axis=1) / num("mat_e_gpa", float("nan")) / 1e9 < 1e-3
            checks.append(("not very soft in any direction (min E / E_s >= 1e-3; bending-dominated cells fall below)", ~weak))
            if "connected" in s:
                checks.append(("one connected solid body", s["connected"].astype(bool)))
            if "joined_3d" in s and s["joined_3d"].notna().any():
                checks.append(("solid joins its neighbours in 3 directions", s.loc[s["joined_3d"].notna(), "joined_3d"].astype(bool)))
            if "energy_share_final" in s:
                lp = pd.to_numeric(s["energy_share_final"], errors="coerce").dropna()
                if len(lp):
                    checks.append((f"solid carries >= {cfg.get('min_load_path_share', 0.9)} of the energy in every load",
                                   lp >= float(cfg.get("min_load_path_share", 0.9))))
            if "grid" in s and "wall_vox" in s and float(cfg["grid"]["min_wall_vox"]) > 0:
                resolved = num("wall_vox") * num("grid") / float(cfg["grid"]["probe"]) >= float(cfg["grid"]["min_wall_vox"]) - 1e-9
                checks.append((f"wall resolved by >= {cfg['grid']['min_wall_vox']} voxels", resolved))
            if "sensitivity_index" in s:
                checks.append(("strain concentration index >= 1", num("sensitivity_index") >= 0.999))
            if "solid_energy_share" in s:
                checks.append(("solid carries >= 95 % of the strain energy (void artifact small)", num("solid_energy_share") >= 0.95))
            cap = cfg.get("max_wave_number")
            if cap and "k_max" in s and "p_lattice_family" in s:
                wm = s["p_lattice_family"].astype(str).isin(WAVE_FAMILIES) & pd.to_numeric(s["k_max"], errors="coerce").notna()
                if wm.any():
                    checks.append((f"wave families: every |k| <= {cap}", pd.to_numeric(s.loc[wm, "k_max"]) <= float(cap) + 1e-9))
            pcols = [c for c in s.columns if c.startswith("p_")]
            checks.append(("unique parameter rows", ~s.duplicated(subset=pcols)))
            lines += ["## Checks (accepted cells)", "", "| check | pass | n |", "|---|---|---|"] + \
                     [f"| {n_} | {m.mean():.1%} | {len(m)} |" for n_, m in checks] + [""]
            lines += ["## Ranges", "", "| quantity | min | median | max |", "|---|---|---|---|"]
            for q in ["lattice_achieved_density", "E_over_Es", "voigt_ratio", "E_ratio", "A_universal", "nu_xy", "lattice_wall_mm", "wall_vox", "sensitivity_index", "solid_strain_ratio", "solid_energy_share", "fft_iterations", "seconds"]:
                if q in s:
                    v = pd.to_numeric(s[q], errors="coerce").dropna()
                    if len(v):
                        lines.append(f"| {q} | {v.min():.4g} | {v.median():.4g} | {v.max():.4g} |")
            lines.append("")
    card = out / "card.md"
    card.write_text("\n".join(lines), encoding="utf-8")
    print("\n".join(lines))
    print(f"-> {card}")


def cmd_recheck(cfg, args):
    """Apply the current checks again to rows already accepted, after a check was
    added or a threshold changed -- what a fresh run would decide for them:
    the probe with the current binary (geometry + printability on the probe grid;
    a failure makes the row `unprintable`), then the post-solve check on the
    existing cell (`no_load_path`). Only the manifest changes; cell files stay."""
    import concurrent.futures as cf
    import h5py
    rows = read_manifest(cfg)
    out = cfg["_out"]
    todo = [r for r in rows if r["status"] == "done" and (out / "cells" / f"{r['id']}.hdf5").exists()]
    probe_size = int(cfg["grid"]["probe"])
    (out / "probe").mkdir(parents=True, exist_ok=True)

    def one(r):
        probe_file = out / "probe" / f"{r['id']}.recheck.hdf5"
        probe_file.unlink(missing_ok=True)
        cells = 1 if single_cell(cfg, r) else None
        code = _run_logged(build_command(cfg, r, probe_file, size=probe_size, solve=False, cells_override=cells),
                           out / "logs" / f"{r['id']}.recheck.log", min(float(cfg["timeout_s"]), 600.0), "w")
        verdict = ""
        try:
            with h5py.File(probe_file, "r") as f:
                verdict = _h5_str(f["1/geometry/print_verdict"])
        except Exception:
            pass
        probe_file.unlink(missing_ok=True)
        return r, code, verdict, final_energy_share(out / "cells" / f"{r['id']}.hdf5")

    with cf.ThreadPoolExecutor(max(1, int(args.jobs))) as ex:
        results = list(ex.map(one, todo))
    changed = []
    for r, code, verdict, share in results:
        if share == share:
            r["energy_share_final"] = round(share, 4)
        if code == 3:
            r["status"] = "unprintable"
            changed.append((r, verdict))
        elif code != 0:
            print(f"  {r['id']}: probe exit {code}, row left as it was (see logs/{r['id']}.recheck.log)")
        elif no_load_path(cfg, share):
            r["status"] = "no_load_path"
            changed.append((r, f"solid share {share:.4f} < {cfg.get('min_load_path_share', 0.9)} in some load"))
    write_manifest(cfg, rows)
    print(f"{len(todo)} accepted rows rechecked, {len(changed)} rejected now:")
    for r, why in changed:
        print(f"  {r['id']} {r['group']:16s} {r.get('p_lattice_family', ''):20s} -> {r['status']}: {why}")
    if changed:
        print("groups now short of their quota can be topped up with `top-up`")


def cmd_status(cfg, args):
    rows = read_manifest(cfg)
    st = {}
    secs = []
    for r in rows:
        st[r["status"]] = st.get(r["status"], 0) + 1
        if r.get("seconds"):
            secs.append(float(r["seconds"]))
    left = st.get("pending", 0) + st.get("retry", 0) + st.get("deferred", 0)
    mean = float(np.mean(secs)) if secs else float("nan")
    print(f"rows {len(rows)}: " + ", ".join(f"{k} {v}" for k, v in sorted(st.items())))
    groups: dict[str, dict] = {}
    for r in rows:
        g = groups.setdefault(r["group"], {})
        g[r["status"]] = g.get(r["status"], 0) + 1
    quotas = quotas_of(cfg)
    for g, d in groups.items():
        print(f"  {g:18s} accepted {d.get('done', 0)}/{quotas.get(g, '?')} | " + ", ".join(f"{k} {v}" for k, v in sorted(d.items())))
    if secs:
        print(f"mean {mean:.0f} s/row, ETA for the rest {left * mean / 3600:.1f} h")


# ─────────────────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["plan", "top-up", "run", "recheck", "collect", "check", "status"])
    ap.add_argument("config", type=Path)
    ap.add_argument("--limit", type=int, default=0, help="run: at most this many rows")
    ap.add_argument("--no-retry", action="store_true", help="run: do not retry failed rows")
    ap.add_argument("--jobs", type=int, default=1, help="run: MatViz3D processes in parallel (threads are split between them)")
    ap.add_argument("--defer-above", type=int, default=0,
                    help="run: rows whose wall needs a grid above this size are marked `deferred` after the probe "
                         "instead of being solved; a later run without the flag solves them")
    ap.add_argument("--timeout", type=float, default=0, help="run: seconds per MatViz3D run, overrides timeout_s")
    ap.add_argument("--order", choices=["interleave", "id"], default="interleave",
                    help="run: groups advance together in proportion to their rows (default), or rows in id order")
    ap.add_argument("--ignore-quota", action="store_true",
                    help="run: solve every row of the pass even when its group already has its quota")
    ap.add_argument("--groups", default="", help="top-up: comma-separated groups (default: every group short of its quota)")
    ap.add_argument("--factor", type=float, default=1.25, help="top-up: rows drawn per missing cell / acceptance")
    ap.add_argument("--isotropic", type=int, default=0,
                    help="top-up: instead of the quota, add rows with aniso = 1 until this many isotropic cells "
                         "per group are expected to be accepted")
    args = ap.parse_args()
    cfg = load_config(args.config)
    {"plan": cmd_plan, "top-up": cmd_topup, "run": cmd_run, "recheck": cmd_recheck, "collect": cmd_collect,
     "check": cmd_check, "status": cmd_status}[args.command](cfg, args)


if __name__ == "__main__":
    main()

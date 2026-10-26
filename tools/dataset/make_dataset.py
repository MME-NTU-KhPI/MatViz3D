#!/usr/bin/env python3
"""Dataset orchestrator for MatViz3D Lattice cells: geometry + material law -> properties.

Four commands, one config file (YAML), one manifest (CSV) that doubles as the
journal, so an interrupted run resumes where it stopped.

    make_dataset.py plan    dataset.yaml        # sample the design space -> manifest.csv
    make_dataset.py run     dataset.yaml        # run MatViz3D headless for every pending row
    make_dataset.py collect dataset.yaml        # HDF5 files -> dataset.parquet (+ waves.parquet)
    make_dataset.py check   dataset.yaml        # sanity checks -> card.md
    make_dataset.py status  dataset.yaml        # progress and ETA

Row statuses: pending -> done | unprintable (exit 3, geometry kept, no stiffness)
| failed (anything else, retried once) | skipped (family quota of accepted cells
already met).

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
    "print_min_wall_mm", "print_max_overhang_deg", "print_max_thin_fraction",
    "print_max_island_vox", "print_max_unsupported_fraction",
}
FLAG_KEYS = {"lattice_uniform_wall", "honeycomb_custom"}
INT_KEYS = {"cells_per_edge", "fourier_terms", "print_max_island_vox"}


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
    cfg.setdefault("common", {})
    cfg.setdefault("laws", [{"id": "petg_ref", "E_GPa": 2.0, "nu": 0.38, "aniso": 1.0}])
    # Adaptive grid (optional). `sizes`: candidate grids per row; the smallest one on
    # which the thinnest wall spans >= `min_wall_vox` voxels is used, measured on a
    # fast `probe` run (geometry + printability only). Families in `pinned` always
    # use the given grid: random noise is drawn on the grid, so a different grid
    # would be a different cell. `single_cell_families` are periodic and, when the
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


def cmd_plan(cfg, args):
    out = cfg["_out"]
    out.mkdir(parents=True, exist_ok=True)
    (out / "cells").mkdir(exist_ok=True)
    (out / "logs").mkdir(exist_ok=True)
    shutil.copyfile(cfg["_config_path"], out / "dataset.yaml")

    laws = cfg["laws"]
    # Laws: either a list of named laws assigned round-robin, or a dict of
    # ranges {E_GPa: [lo, hi], nu: [lo, hi], aniso: [lo, hi]} sampled per row
    # (Sobol, own seed), so the material is a continuous axis of the dataset.
    law_rng = None
    if isinstance(laws, dict):
        law_rng = qmc.Sobol(d=3, scramble=True, seed=int(cfg["seed0"]) + 99) if qmc is not None else np.random.default_rng(int(cfg["seed0"]) + 99)
    def draw_law(k: int) -> dict:
        if law_rng is None:
            return laws[k % len(laws)]
        u = law_rng.random(1)[0] if qmc is not None else law_rng.random(3)
        def span(key, default):
            r = laws.get(key, default)
            return (r[0] + float(u[["E_GPa", "nu", "aniso"].index(key)]) * (r[1] - r[0])) if isinstance(r, (list, tuple)) else float(r)
        return {"id": "sampled", "E_GPa": round(span("E_GPa", 2.0), 3), "nu": round(span("nu", 0.38), 4), "aniso": round(span("aniso", 1.0), 3)}
    rows: list[dict] = []
    rid = 1
    seed = int(cfg["seed0"])
    for gi, (group, block) in enumerate(cfg["families"].items()):
        n = int(math.ceil(block.get("quota", 0) * float(block.get("oversample", cfg["oversample"]))))
        if n <= 0:
            continue
        for p in sample_family(group, block, cfg["common"], n, sobol_seed=1000 * gi + 7):
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
    printability check failed, 4 = no candidate grid resolves the wall.
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
        except Exception as e:
            with log_file.open("a", encoding="utf-8") as lf:
                lf.write(f"\n# probe unreadable: {e}\n")
            return -3, time.time() - t0, info
        finally:
            if probe_file.exists():
                probe_file.unlink()
        chosen = choose_grid(cfg, row, info["wall_vox"])
        if chosen is None:
            info["grid"] = ""
            return 4, time.time() - t0, info
        size = chosen
    info["grid"] = size
    code = _run_logged(build_command(cfg, row, out_file, size=size, solve=True, cells_override=cells),
                       log_file, float(cfg["timeout_s"]), "a" if adaptive else "w")
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
    return code, time.time() - t0, info


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
                if quotas.get(group) and accepted(group) >= quotas[group]:
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
            else:
                status = "failed" if row["status"] == "retry" else "retry"
            with lock:
                counter["n"] += 1
                row.update(info)
                row.update({"status": status, "exit_code": code, "seconds": round(secs, 1),
                            "finished": datetime.now().isoformat(timespec="seconds")})
                write_manifest(cfg, rows)
                finished = sum(1 for r in rows if r["status"] not in ("pending", "retry"))
                left = sum(1 for r in rows if r["status"] in ("pending", "retry"))
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


def cmd_run(cfg, args):
    rows = read_manifest(cfg)
    if not rows:
        sys.exit("no manifest; run `plan` first")
    if not cfg["_matviz"].exists():
        sys.exit(f"MatViz3D binary not found: {cfg['_matviz']}")
    jobs = max(1, int(args.jobs))
    if jobs > 1 and not cfg.get("np"):
        cfg["np"] = max(1, (os.cpu_count() or 2) // jobs)   # share the cores between processes
    todo = [r for r in rows if r["status"] in ("pending", "retry")]
    if args.limit:
        todo = todo[: args.limit]
    finished = sum(1 for r in rows if r["status"] in ("done", "unprintable", "unresolved", "failed"))
    g = cfg["grid"]
    grid_note = (f"adaptive grid {g['sizes']} (>= {g['min_wall_vox']} voxels per wall, probe {g['probe']}^3)"
                 if float(g["min_wall_vox"]) > 0 or len(g["sizes"]) > 1 else f"grid {cfg['size']}^3")
    print(f"{len(todo)} rows this pass, {finished} finished before, {len(rows)} total, "
          f"{jobs} parallel job(s) x {cfg.get('np') or 'all'} threads, {grid_note}, fft_tol {cfg['fft_tol']}")
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
    return row, waves


def cmd_collect(cfg, args):
    import pandas as pd
    rows = read_manifest(cfg)
    out = cfg["_out"]
    records, wave_records = [], []
    for r in rows:
        if r["status"] not in ("done", "unprintable"):
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
        for k in ("grid", "wall_vox", "wall_mm_probe", "solve_cells", "void_eta", "energy_share_first", "void_extrap"):
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
    lines = [f"# Dataset card — {out.name}", "",
             f"generated {datetime.now().isoformat(timespec='minutes')}; planned {meta.get('created', '?')}; "
             f"MatViz3D commit {meta.get('matviz_commit') or '?'}; grid {cfg['size']}^3; FFT tol {cfg['fft_tol']}", "",
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
        lines += ["## By family", "", "| group | quota | accepted | unprintable | unresolved | unprintable share | mean s/accepted cell |", "|---|---|---|---|---|---|---|"]
        for g, d in df.groupby("group"):
            acc = d[d["status"] == "done"]
            stg = st_rows.get(g, {})          # manifest counts: unprintable probes write no HDF5
            unpr, unres = stg.get("unprintable", 0), stg.get("unresolved", 0)
            tried = sum(v for k, v in stg.items() if k not in ("pending", "retry", "skipped"))
            lines.append(f"| {g} | {quotas.get(g, '?')} | {len(acc)} | {unpr} | {unres} | "
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
        s = df[df["has_stiffness"] == True]
        checks = []   # (name, boolean Series over the rows the check applies to)
        if len(s):
            num = lambda col, default=0.0: pd.to_numeric(s[col], errors="coerce").fillna(default) if col in s else pd.Series(default, index=s.index)
            checks.append(("all moduli positive", (s[["E_x", "E_y", "E_z", "G_xy", "G_yz", "G_xz"]] > 0).all(axis=1)))
            if "E_over_hs" in s:
                checks.append(("mean E below Hashin–Shtrikman upper bound (x1.05)", s["E_over_hs"] <= 1.05))
            if "E_over_Es" in s:
                checks.append(("E/E_s within (0, 1)", (s["E_over_Es"] > 0) & (s["E_over_Es"] < 1)))
            cubic_families = ["gyroid_sheet", "gyroid_skeletal", "schwarz_p_sheet", "schwarz_p_skeletal", "schwarz_d_sheet",
                              "schwarz_d_skeletal", "iwp_sheet", "iwp_skeletal", "simple_cubic", "bcc", "fcc", "octet"]
            cubic_mask = (s["p_lattice_family"].astype(str).isin(cubic_families) if "p_lattice_family" in s else pd.Series(False, index=s.index)) \
                & (num("law_aniso", 1.0) == 1.0) & (num("p_lattice_grading") == 0) & (num("p_lattice_jitter") == 0)
            if cubic_mask.any():
                checks.append(("cubic cells: E_x = E_y = E_z within 2 %", s.loc[cubic_mask, "E_ratio"] < 1.02))
            if "fft_iterations" in s:
                checks.append(("FFT converged (iterations < 6 x cap)", num("fft_iterations") < 6 * float(cfg["fft_max_iter"])))
            if "lattice_achieved_density" in s and "p_relative_density" in s:
                checks.append(("achieved density within 0.03 of target", (num("lattice_achieved_density") - num("p_relative_density")).abs() <= 0.03))
            weak = s[["E_x", "E_y", "E_z"]].min(axis=1) / num("mat_e_gpa", float("nan")) / 1e9 < 1e-3
            checks.append(("load path in every direction (min E / E_s >= 1e-3)", ~weak))
            if "connected" in s:
                checks.append(("one connected solid body", s["connected"].astype(bool)))
            if "grid" in s and "wall_vox" in s and float(cfg["grid"]["min_wall_vox"]) > 0:
                resolved = num("wall_vox") * num("grid") / float(cfg["grid"]["probe"]) >= float(cfg["grid"]["min_wall_vox"]) - 1e-9
                checks.append((f"wall resolved by >= {cfg['grid']['min_wall_vox']} voxels", resolved))
            if "sensitivity_index" in s:
                checks.append(("strain concentration index >= 1", num("sensitivity_index") >= 0.999))
            if "solid_energy_share" in s:
                checks.append(("solid carries >= 95 % of the strain energy (void artifact small)", num("solid_energy_share") >= 0.95))
            pcols = [c for c in s.columns if c.startswith("p_")]
            checks.append(("unique parameter rows", ~s.duplicated(subset=pcols)))
            lines += ["## Checks (accepted cells)", "", "| check | pass | n |", "|---|---|---|"] + \
                     [f"| {n_} | {m.mean():.1%} | {len(m)} |" for n_, m in checks] + [""]
            lines += ["## Ranges", "", "| quantity | min | median | max |", "|---|---|---|---|"]
            for q in ["lattice_achieved_density", "E_over_Es", "E_ratio", "A_universal", "nu_xy", "lattice_wall_mm", "wall_vox", "sensitivity_index", "solid_strain_ratio", "solid_energy_share", "fft_iterations", "seconds"]:
                if q in s:
                    v = pd.to_numeric(s[q], errors="coerce").dropna()
                    if len(v):
                        lines.append(f"| {q} | {v.min():.4g} | {v.median():.4g} | {v.max():.4g} |")
            lines.append("")
    card = out / "card.md"
    card.write_text("\n".join(lines), encoding="utf-8")
    print("\n".join(lines))
    print(f"-> {card}")


def cmd_status(cfg, args):
    rows = read_manifest(cfg)
    st = {}
    secs = []
    for r in rows:
        st[r["status"]] = st.get(r["status"], 0) + 1
        if r.get("seconds"):
            secs.append(float(r["seconds"]))
    left = st.get("pending", 0) + st.get("retry", 0)
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
    ap.add_argument("command", choices=["plan", "run", "collect", "check", "status"])
    ap.add_argument("config", type=Path)
    ap.add_argument("--limit", type=int, default=0, help="run: at most this many rows")
    ap.add_argument("--no-retry", action="store_true", help="run: do not retry failed rows")
    ap.add_argument("--jobs", type=int, default=1, help="run: MatViz3D processes in parallel (threads are split between them)")
    args = ap.parse_args()
    cfg = load_config(args.config)
    {"plan": cmd_plan, "run": cmd_run, "collect": cmd_collect, "check": cmd_check, "status": cmd_status}[args.command](cfg, args)


if __name__ == "__main__":
    main()

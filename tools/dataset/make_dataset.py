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


def sample_family(group: str, block: dict, common: dict, n: int, sobol_seed: int) -> list[dict]:
    params = dict(common)
    params.update(block.get("params", {}))
    if "lattice_family" not in params:
        params["lattice_family"] = group
    keys = sorted(params)
    dims = [k for k in keys if _spec_kind(params[k]) != "fixed"]
    if qmc is not None and dims:
        n2 = 1 << max(0, (n - 1).bit_length())          # next power of two
        u = qmc.Sobol(d=len(dims), scramble=True, seed=sobol_seed).random(n2)[:n]
    else:
        u = np.random.default_rng(sobol_seed).random((n, max(1, len(dims))))
    rows = []
    for i in range(n):
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
        rows.append(row)
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
    rows: list[dict] = []
    rid = 1
    seed = int(cfg["seed0"])
    for gi, (group, block) in enumerate(cfg["families"].items()):
        n = int(math.ceil(block.get("quota", 0) * float(block.get("oversample", cfg["oversample"]))))
        if n <= 0:
            continue
        for p in sample_family(group, block, cfg["common"], n, sobol_seed=1000 * gi + 7):
            law = laws[(rid - 1) % len(laws)]
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

def build_command(cfg, row: dict, out_file: Path) -> list[str]:
    cmd = [str(cfg["_matviz"]), "--nogui", "--nologo", "--autostart", "--algorithm", "Lattice",
           "--size", str(cfg["size"]), "--seed", str(row["seed"])]
    for k, v in row.items():
        if not k.startswith("p_"):
            continue
        key = k[2:]
        if key in FLAG_KEYS:
            if str(v).lower() in ("true", "1", "yes"):
                cmd.append("--" + key)
            continue
        if key not in PARAM_KEYS or v in ("", None):
            continue
        cmd += ["--" + key, str(v)]
    E, nu, an = float(row["law_E_GPa"]), float(row["law_nu"]), float(row.get("law_aniso", 1.0))
    if abs(an - 1.0) < 1e-9:
        cmd += ["--material_iso", f"{E},{nu}"]
    else:
        cmd += ["--material_ti", f"{E},{nu},{an}"]
    cmd += ["--require_printable", "--no_fields",
            "--run_stress_calc", "--solver", "fft", "--stress_mode", "stiffness",
            "--fft_tol", str(cfg["fft_tol"]), "--fft_max_iter", str(cfg["fft_max_iter"])]
    if cfg.get("np"):
        cmd += ["--np", str(cfg["np"])]
    cmd += ["--output", str(out_file)]
    return cmd


def run_one(cfg, row: dict) -> tuple[int, float]:
    """Run MatViz3D for one manifest row; returns (exit code, seconds)."""
    out = cfg["_out"]
    out_file = out / "cells" / f"{row['id']}.hdf5"
    log_file = out / "logs" / f"{row['id']}.log"
    if out_file.exists():
        out_file.unlink()                 # MatViz3D appends groups; start clean
    cmd = build_command(cfg, row, out_file)
    t0 = time.time()
    try:
        with log_file.open("w", encoding="utf-8") as lf:
            lf.write("# " + " ".join(cmd) + "\n\n")
            lf.flush()
            proc = subprocess.run(cmd, stdout=lf, stderr=subprocess.STDOUT, timeout=float(cfg["timeout_s"]))
        code = proc.returncode
    except subprocess.TimeoutExpired:
        code = -1
    except OSError as e:
        with log_file.open("a", encoding="utf-8") as lf:
            lf.write(f"\n# could not start: {e}\n")
        code = -2
    return code, time.time() - t0


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
            code, secs = run_one(cfg, row)
            if code == 0:
                status = "done"
            elif code == 3:
                status = "unprintable"
            else:
                status = "failed" if row["status"] == "retry" else "retry"
            with lock:
                counter["n"] += 1
                row.update({"status": status, "exit_code": code, "seconds": round(secs, 1),
                            "finished": datetime.now().isoformat(timespec="seconds")})
                write_manifest(cfg, rows)
                finished = sum(1 for r in rows if r["status"] not in ("pending", "retry"))
                left = sum(1 for r in rows if r["status"] in ("pending", "retry"))
                per_row = (time.time() - t_all) / counter["n"]
                print(f"{label}[{counter['n']}/{len(todo)}] {row['id']} {group:16s} -> {status:11s} {secs:6.1f}s | "
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
    finished = sum(1 for r in rows if r["status"] in ("done", "unprintable", "failed"))
    print(f"{len(todo)} rows this pass, {finished} finished before, {len(rows)} total, "
          f"{jobs} parallel job(s) x {cfg.get('np') or 'all'} threads")
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
             "Material laws: " + "; ".join(f"{l['id']} (E={l['E_GPa']} GPa, nu={l['nu']}, aniso={l.get('aniso', 1.0)})" for l in cfg["laws"]), ""]
    st = {}
    for r in rows:
        st[r["status"]] = st.get(r["status"], 0) + 1
    lines += ["## Manifest", "", "| status | rows |", "|---|---|"] + [f"| {k} | {v} |" for k, v in sorted(st.items())] + [""]
    if len(df):
        quotas = quotas_of(cfg)
        lines += ["## By family", "", "| group | quota | accepted | unprintable | unprintable share | mean s/accepted cell |", "|---|---|---|---|---|---|"]
        for g, d in df.groupby("group"):
            acc = d[d["status"] == "done"]
            lines.append(f"| {g} | {quotas.get(g, '?')} | {len(acc)} | {int((d['status'] == 'unprintable').sum())} | "
                         f"{(d['status'] == 'unprintable').mean():.0%} | {acc['seconds'].mean() if len(acc) else float('nan'):.0f} |")
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
            pcols = [c for c in s.columns if c.startswith("p_")]
            checks.append(("unique parameter rows", ~s.duplicated(subset=pcols)))
            lines += ["## Checks (accepted cells)", "", "| check | pass | n |", "|---|---|---|"] + \
                     [f"| {n_} | {m.mean():.1%} | {len(m)} |" for n_, m in checks] + [""]
            lines += ["## Ranges", "", "| quantity | min | median | max |", "|---|---|---|---|"]
            for q in ["lattice_achieved_density", "E_over_Es", "E_ratio", "A_universal", "nu_xy", "lattice_wall_mm", "fft_iterations", "seconds"]:
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

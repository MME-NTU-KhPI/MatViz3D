"""Stiffness of a solid / void voxel cell by the solver of the Neural tab (gpu_stiffness.py, next to this file), on any computer:
on the GPU when PyTorch has one (NVIDIA through CUDA, Apple through MPS, Intel through XPU), else on the CPU. A check of the
solver and of the machine that needs neither the program nor the dataset. Explanations: STIFFNESS_CHECK.md (STIFFNESS_CHECK.pdf in the handed-over folder).

    python stiffness_check.py                         self-test on built-in cells with known answers (a minute or two)
    python stiffness_check.py cell.hdf5               a cell file of MatViz3D or of the dataset; compared with the answer stored in it
    python stiffness_check.py some_folder             every .hdf5 / .h5 / .npy file of the folder, then a table
    python stiffness_check.py cell.npy                a numpy array n x n x n, nonzero = solid, axes x, y, z
    python stiffness_check.py cell.raw --n 64         n^3 bytes as the program hands them to the solver: index (iz * n + iy) * n + ix
    python stiffness_check.py gyroid --n 128          a built-in cell: solid, layers_x, layers_y, layers_z, gyroid

    --device auto|cuda|mps|xpu|cpu    where to solve (auto: the first GPU found, else the CPU)
    --compare_cpu                     solve every cell once more on the CPU: the difference and the speed-up
    --e_gpa, --nu, --aniso            the material (default: from the HDF5 file, else 2 GPa, 0.38, 1)
    --set                             the group of an HDF5 file (default: last_set, else 1)
    --tol, --max_iter                 conjugate-gradient tolerance (1e-5) and iteration limit (10000), as the program
    --json results.json               every result with the full matrix

Exit code 0 when every check passed, 1 when one failed, 2 when PyTorch or the input is missing.
"""
import argparse, json, math, pathlib, platform, sys, time

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import numpy as np

try:
    import torch
except ImportError as exc:
    print(f'PyTorch is not installed for this Python ({sys.executable}): {exc}\nSee STIFFNESS_CHECK, section 01 (installation).')
    sys.exit(2)
import gpu_stiffness as gs
import warnings
# PyTorch on the Apple GPU warns from inside its FFT about a resized output; harmless, and the same on every cell
warnings.filterwarnings('ignore', message='An output with one or more elements was resized')

S2 = np.array([1.0, 1.0, 1.0, math.sqrt(2.0), math.sqrt(2.0), math.sqrt(2.0)])
ORDER = 'xx yy zz xy yz xz'
MODULI = ('Ex', 'Ey', 'Ez', 'Gxy', 'Gyz', 'Gxz')
BUILTIN = ('solid', 'layers_x', 'layers_y', 'layers_z', 'gyroid')
PASS_STORED = 0.1          # %: largest difference of a modulus from the answer stored in a file that still passes
PASS_EXACT = 1e-4          # largest difference from an exact answer, as a share of C11 of the material
PASS_SYMMETRY = 5e-4       # largest relative spread of Ex, Ey, Ez (and of the three G) of a cubic cell
PASS_CPU = 1e-3            # largest difference of a modulus between the GPU and the CPU, as a share


# ---------------------------------------------------------------------------------------------------------------- devices

def devices():
    """the devices PyTorch can use here, best first, and why a GPU is missing"""
    found, notes = [], []
    hip = getattr(torch.version, 'hip', None)
    if torch.cuda.is_available():
        found.append('cuda')
    elif platform.system() == 'Darwin':
        pass                                                                    # no CUDA on a Mac: nothing to explain
    elif torch.version.cuda is None and hip is None:
        notes.append('CUDA: this PyTorch is a CPU-only build (an NVIDIA card needs the CUDA build of PyTorch)')
    else:
        notes.append(f'CUDA: PyTorch is built for {"ROCm " + hip if hip else "CUDA " + str(torch.version.cuda)} but sees no card '
                     f'(no such GPU, or its driver is older than this PyTorch needs)')
    mps = getattr(torch.backends, 'mps', None)
    if mps is not None and mps.is_available():
        found.append('mps')
    xpu = getattr(torch, 'xpu', None)
    if xpu is not None and xpu.is_available():
        found.append('xpu')
    found.append('cpu')
    return found, notes


def describe(dev):
    if dev == 'cuda':
        p = torch.cuda.get_device_properties(0); hip = getattr(torch.version, 'hip', None)
        return f'{p.name}, {p.total_memory / 2 ** 30:.1f} GB, {"ROCm " + hip if hip else "CUDA " + str(torch.version.cuda)}'
    if dev == 'mps':
        return f'Apple GPU (Metal), {platform.machine()}'
    if dev == 'xpu':
        return f'{torch.xpu.get_device_name(0)} (Intel XPU)'
    return f'{platform.processor() or platform.machine()}, {torch.get_num_threads()} threads'


def memory_reset(dev):
    if dev == 'cuda':
        torch.cuda.reset_peak_memory_stats()


def memory_used(dev):
    """GB of GPU memory the solve took (the peak on CUDA, what the driver holds on MPS); None on the CPU or when this PyTorch
    cannot tell"""
    try:
        if dev == 'cuda':
            return torch.cuda.max_memory_allocated() / 2 ** 30
        if dev == 'mps':
            return torch.mps.driver_allocated_memory() / 2 ** 30
        if dev == 'xpu':
            return torch.xpu.max_memory_allocated() / 2 ** 30
    except (AttributeError, RuntimeError):
        pass
    return None


def free(dev):
    try:
        if dev == 'cuda':
            torch.cuda.empty_cache()
        elif dev == 'mps':
            torch.mps.empty_cache()
    except (AttributeError, RuntimeError):
        pass


# ---------------------------------------------------------------------------------------------------------------- the solve

def to_program(CM):
    """Mandel matrix -> the order of the program (xx, yy, zz, xy, yz, xz) with engineering shear"""
    return (CM / np.outer(S2, S2))[np.ix_(gs.PERM, gs.PERM)]


def moduli(C):
    """Ex, Ey, Ez, Gxy, Gyz, Gxz of a stiffness in the order of the program"""
    return 1.0 / np.diag(np.linalg.inv(C))


def stiffness(chi, nu, aniso, dev, tol, max_iter, show=True):
    """the rule of the program and the dataset: void 1e-4, and a second solve with void 1e-6 when the solid holds less than
    0.985 of the strain energy in some load (0.97 on 128^3 and finer grids). -> dict, C relative to E of the material"""
    n = chi.shape[0]; min_share = 0.97 if n >= 128 else 0.985
    solver = gs.Solver(np.ascontiguousarray(chi, dtype=bool), nu, aniso, dev); last = [time.time()]

    def progress(step, residual, loads):
        if show and time.time() - last[0] >= 5.0:
            last[0] = time.time(); print(f'      ... {step} iterations, residual {residual:.1e}, loads done {loads} of 6', flush=True)
    solver.progress = progress
    memory_reset(dev); t0 = time.time()
    first = solver.solve(1e-4, tol, max_iter); r = dict(first); r['passes'] = 1
    if not (first['share'] >= min_share):
        r = dict(solver.solve(1e-6, tol, max_iter)); r['passes'] = 2; r['iterations'] += first['iterations']
    r['share_first'] = first['share']; r['seconds'] = time.time() - t0; r['memory_gb'] = memory_used(dev); r['min_share'] = min_share
    free(dev)
    return r


def material(nu, aniso):
    """the stiffness of the material relative to its E (order of the program) and the void phase of the same law"""
    Cs, c11 = gs.law_mandel(np, nu, aniso)
    return Cs, c11


def layers_exact(nu, aniso, eta, f, axis):
    """exact stiffness of flat layers normal to `axis`: solid share f, void (eta x C11, nu 0.3) the rest. In the layers the
    in-plane strain and the stress on the layer plane are the same, which gives the answer in closed form (any material)."""
    Cs, c11 = gs.law_mandel(np, nu, aniso); Cv = gs.iso_mandel(np, eta * c11, 0.3)
    N = {0: [0, 5, 4], 1: [1, 3, 5], 2: [2, 3, 4]}[axis]                       # Mandel order xx yy zz yz xz xy: the components on the plane
    I = [k for k in range(6) if k not in N]; inv = np.linalg.inv; sub = lambda C, a, b: C[np.ix_(a, b)]
    phases = ((f, Cs), (1.0 - f, Cv))
    A = sum(w * inv(sub(C, N, N)) for w, C in phases)
    B = sum(w * inv(sub(C, N, N)) @ sub(C, N, I) for w, C in phases)
    K = sum(w * (sub(C, I, I) - sub(C, I, N) @ inv(sub(C, N, N)) @ sub(C, N, I)) for w, C in phases)
    L = sum(w * sub(C, I, N) @ inv(sub(C, N, N)) for w, C in phases)
    Ai = inv(A); C = np.zeros((6, 6))
    C[np.ix_(N, N)] = Ai; C[np.ix_(N, I)] = Ai @ B; C[np.ix_(I, I)] = K + L @ Ai @ B; C[np.ix_(I, N)] = L @ Ai
    return to_program(0.5 * (C + C.T)), c11


# ---------------------------------------------------------------------------------------------------------------- inputs

def builtin(name, n):
    if name == 'solid':
        return np.ones((n, n, n), bool)
    if name.startswith('layers_'):
        axis = 'xyz'.index(name[-1]); chi = np.zeros((n, n, n), bool); sl = [slice(None)] * 3; sl[axis] = slice(0, n // 2); chi[tuple(sl)] = True
        return chi
    if name == 'gyroid':                                                        # sheet gyroid, about a third solid, one period per edge
        x = (np.arange(n) + 0.5) * 2.0 * np.pi / n; X, Y, Z = np.meshgrid(x, x, x, indexing='ij')
        return np.abs(np.sin(X) * np.cos(Y) + np.sin(Y) * np.cos(Z) + np.sin(Z) * np.cos(X)) < 0.45
    raise ValueError(f'unknown built-in cell {name}; there are {", ".join(BUILTIN)}')


def text(v):
    return bytes(np.asarray(v).astype(np.uint8)).decode(errors='replace') if np.asarray(v).dtype.kind in 'iu' else str(v)


def read_hdf5(path, group):
    """-> cell (x, y, z), material from the file or None, stored answer (order of the program, Pa) or None, notes"""
    try:
        import h5py
    except ImportError:
        raise SystemExit('reading .hdf5 needs h5py: python -m pip install h5py')
    notes = []
    with h5py.File(path, 'r') as f:
        if group is None:
            last = str(int(f['last_set'][()])) if 'last_set' in f else None
            group = next((g for g in (last, '1') if g and g in f and 'voxels' in f[g]), None)
            group = group or next((k for k in f if isinstance(f[k], h5py.Group) and 'voxels' in f[k]), None)
        if group is None or group not in f or 'voxels' not in f[group]:
            raise SystemExit(f'{path}: no group with "voxels" (give one with --set)')
        g = f[group]; vox = g['voxels'][...]; law, stored = None, None
        if 'geometry' in g and 'mat_e_gpa' in g['geometry']:
            geo = g['geometry']; law = {'e_gpa': float(geo['mat_e_gpa'][()]), 'nu': float(geo['mat_nu'][()]), 'aniso': float(geo['mat_aniso'][()])}
        if 'C_matrix' in g:
            Cf = g['C_matrix'][...].astype(np.float64); U = np.triu(Cf * np.where(np.arange(6) >= 3, 0.5, 1.0)[None, :])   # the files hold the tensor shear basis
            stored = U + U.T - np.diag(np.diag(U))
            solver = text(g['solver'][...]) if 'solver' in g else ''
            if solver and solver.lower() != 'fft':
                notes.append(f'the stored answer comes from the "{solver}" solver, not from FFT: a few % difference is normal')
    if vox.ndim != 3 or len(set(vox.shape)) != 1:
        raise SystemExit(f'{path}: voxels {vox.shape}, a cube n x n x n is needed')
    return vox > 0, law, stored, f'{path.name} (group {group})', notes


def read_cell(spec, a):
    """-> list of (label, cell, material or None, stored answer or None, notes)"""
    if spec in BUILTIN:
        return [(f'{spec} (built in)', builtin(spec, a.n or 64), None, None, [])]
    path = pathlib.Path(spec)
    if path.is_dir():
        files = sorted(p for p in path.iterdir() if p.suffix.lower() in ('.hdf5', '.h5', '.npy'))
        if not files:
            raise SystemExit(f'{path}: no .hdf5 / .h5 / .npy files')
        return [c for p in files for c in read_cell(str(p), a)]
    if not path.exists():
        raise SystemExit(f'{spec}: no such file, folder or built-in cell ({", ".join(BUILTIN)})')
    ext = path.suffix.lower()
    if ext in ('.hdf5', '.h5'):
        chi, law, stored, label, notes = read_hdf5(path, a.set)
        return [(label, chi, law, stored, notes)]
    if ext == '.npy':
        v = np.load(path)
        if v.ndim != 3 or len(set(v.shape)) != 1:
            raise SystemExit(f'{path}: array {v.shape}, a cube n x n x n is needed')
        return [(path.name, v != 0, None, None, [])]
    raw = np.fromfile(path, dtype=np.uint8); n = a.n or round(raw.size ** (1.0 / 3.0))
    if raw.size != n ** 3:
        raise SystemExit(f'{path}: {raw.size} bytes, not n^3 for n = {n} (give --n)')
    return [(path.name, np.ascontiguousarray((raw.reshape(n, n, n) != 0).transpose(2, 1, 0)), None, None, [])]


# ---------------------------------------------------------------------------------------------------------------- report

def fmt_matrix(C, unit):
    rows = [f'      {"":>4}' + ''.join(f'{h:>11}' for h in ORDER.split())]
    for name, row in zip(ORDER.split(), C):
        rows.append(f'      {name:>4}' + ''.join(f'{x / unit:11.5f}' for x in row))
    return '\n'.join(rows)


def run_line(dev, r):
    mem = f', GPU memory {r["memory_gb"]:.2f} GB' if r['memory_gb'] is not None else ''
    state = 'converged' if r['converged'] else f'NOT CONVERGED (residual {r["residual"]:.1e})'
    second = f', second pass with void 1e-6 (solid held {r["share_first"]:.3f} < {r["min_share"]} of the energy with 1e-4)' if r['passes'] == 2 else ', void 1e-4'
    return f'{dev}: {r["iterations"]} iterations, {state}{second}, solid share {r["share"]:.3f}, {r["seconds"]:.1f} s{mem}'


def solve_cell(label, chi, law, stored, notes, a, dev, results):
    """one cell: solve, print, compare with what is known -> True when nothing failed"""
    n = chi.shape[0]; src = 'from the file' if law else 'given' if a.material_given else 'default'
    law = dict(law or {}); law.update({k: getattr(a, k) for k in ('e_gpa', 'nu', 'aniso') if getattr(a, k) is not None})
    law = {'e_gpa': law.get('e_gpa', 2.0), 'nu': law.get('nu', 0.38), 'aniso': law.get('aniso', 1.0)}
    E = law['e_gpa'] * 1e9; ok = True
    print(f'\n{label}: {n}^3, solid share of the voxels {chi.mean():.3f}, material E {law["e_gpa"]:g} GPa, nu {law["nu"]:g}, '
          f'aniso {law["aniso"]:g} ({src})')
    for note in notes:
        print(f'   note: {note}')
    if chi.all():
        print('   note: every voxel is solid (a filled cube or a polycrystal): the answer is the material itself')
    if not chi.any():
        print('   FAIL: the cell has no solid voxel'); return False
    try:
        r = stiffness(chi, law['nu'], law['aniso'], dev, a.tol, a.max_iter)
    except Exception as exc:                                                    # e.g. out of memory, or an operation the device lacks
        print(f'   FAIL: the solve on {dev} stopped: {type(exc).__name__}: {exc}\n   (try --device cpu, or a smaller --n)'); return False
    print('   ' + run_line(dev, r)); ok &= r['converged']
    if r['share'] < 0.9:
        print(f'   note: after the solve the solid holds only {r["share"]:.3f} of the energy in some load: the cell has no solid path '
              'in some direction (the dataset rejects such cells)')
    C = r['C'] * E; m = moduli(C)
    print('   moduli, GPa:  ' + '  '.join(f'{k} {v / 1e9:.5f}' for k, v in zip(MODULI, m)))
    print('   relative to E: ' + '  '.join(f'{k} {v / E:.5f}' for k, v in zip(MODULI, m)))
    print(f'   stiffness matrix C, GPa (order {ORDER}, engineering shear):\n' + fmt_matrix(C, 1e9))
    rec = {'cell': label, 'n': n, 'solid_share_voxels': float(chi.mean()), 'material': law, 'device': dev, 'C_Pa': C.tolist(),
           'moduli_Pa': dict(zip(MODULI, m.tolist())), **{k: r[k] for k in ('iterations', 'converged', 'passes', 'void_eta', 'share', 'seconds', 'memory_gb')}}
    if stored is not None:
        ms = moduli(stored); d = (m / ms - 1.0) * 100; dm = np.linalg.norm(C - stored) / np.linalg.norm(stored) * 100
        good = max(abs(d).max(), dm) <= PASS_STORED; ok &= good
        print(f'   stored answer of the file: E differ by {abs(d[:3]).max():.4f} %, G by {abs(d[3:]).max():.4f} %, the matrix by {dm:.4f} % '
              f'-> {"PASS" if good else "DIFFERS"} (limit {PASS_STORED} %)')
        rec.update(stored_diff_E_pct=float(abs(d[:3]).max()), stored_diff_G_pct=float(abs(d[3:]).max()), stored_diff_matrix_pct=float(dm))
    if a.compare_cpu and dev != 'cpu':
        print('   the same on the CPU ...', flush=True)
        rc = stiffness(chi, law['nu'], law['aniso'], 'cpu', a.tol, a.max_iter); mc = moduli(rc['C'] * E); dc = abs(m / mc - 1.0).max()
        good = dc <= PASS_CPU; ok &= good
        print(f'   {run_line("cpu", rc)}\n   GPU against CPU: moduli differ by {dc * 100:.4f} % at most, the GPU is {rc["seconds"] / r["seconds"]:.1f} times faster '
              f'-> {"PASS" if good else "DIFFERS"}')
        rec.update(cpu_seconds=rc['seconds'], cpu_diff_pct=float(dc * 100))
    results.append(rec)
    return ok


def self_test(a, dev, results):
    """built-in cells whose answers are known without any reference: the material itself, flat layers (closed-form answer, three
    directions, a material softer across z so that a mix-up of the axes shows), the cubic symmetry of a gyroid, GPU against CPU"""
    n = a.n or 64; nu, aniso = 0.38, 0.8; checks = []

    def report(name, value, limit, what, extra='', converged=True):
        good = bool(value <= limit) and converged; checks.append((name, good))
        print(f'   {what}: {value:.2e} (limit {limit:.0e}){"" if converged else ", but the solve did not converge"} -> {"PASS" if good else "FAIL"}{extra}')
        return good

    print(f'\nself-test on {n}^3 cells: material nu {nu}, aniso {aniso} (stiffness across z is {aniso} of that in the plane), '
          f'gyroid with aniso 1')
    for k, name in enumerate(('solid',) + tuple(f'layers_{c}' for c in 'xyz')):
        chi = builtin(name, n); print(f'\n{k + 1}. {name}: solid share {chi.mean():.3f}', flush=True)
        try:
            r = stiffness(chi, nu, aniso, dev, a.tol, a.max_iter)
        except Exception as exc:
            print(f'   FAIL: {type(exc).__name__}: {exc}'); checks.append((name, False)); continue
        print('   ' + run_line(dev, r))
        if name == 'solid':
            Cs, c11 = material(nu, aniso); want = to_program(Cs)
            report(name, np.abs(r['C'] - want).max() / c11, PASS_EXACT, 'answer against the material matrix, largest difference / C11', converged=r['converged'])
        else:
            want, c11 = layers_exact(nu, aniso, r['void_eta'], float(chi.mean()), 'xyz'.index(name[-1]))
            mg, mw = moduli(r['C']), moduli(want)
            print(f'   (across the layers nothing but the void carries a load, so a solid share near 0, the second pass and moduli near 0 across are the right answer)')
            report(name, np.abs(r['C'] - want).max() / c11, PASS_EXACT, 'answer against the exact layer formula, largest difference / C11', converged=r['converged'],
                   extra=
                   ('\n   moduli / E here:  ' + ' '.join(f'{k} {v:.5f}' for k, v in zip(MODULI, mg)) +
                   '\n   moduli / E exact: ' + ' '.join(f'{k} {v:.5f}' for k, v in zip(MODULI, mw))))
        results.append({'check': name, 'device': dev, 'C_rel': r['C'].tolist(), 'seconds': r['seconds'], 'iterations': r['iterations']})

    chi = builtin('gyroid', n); print(f'\n5. gyroid: solid share {chi.mean():.3f}', flush=True)
    assert (chi == chi.transpose(1, 2, 0)).all(), 'the voxel gyroid must map onto itself when the axes turn x -> y -> z'
    try:
        r = stiffness(chi, nu, 1.0, dev, a.tol, a.max_iter); print('   ' + run_line(dev, r)); m = moduli(r['C'])
        print('   moduli / E: ' + ' '.join(f'{k} {v:.5f}' for k, v in zip(MODULI, m)))
        spread = max(np.ptp(m[:3]) / m[:3].mean(), np.ptp(m[3:]) / m[3:].mean())
        report('gyroid', spread, PASS_SYMMETRY, 'cubic symmetry: spread of Ex, Ey, Ez and of the three G, relative', converged=r['converged'])
        results.append({'check': 'gyroid', 'device': dev, 'C_rel': r['C'].tolist(), 'seconds': r['seconds'], 'iterations': r['iterations']})
    except Exception as exc:
        print(f'   FAIL: {type(exc).__name__}: {exc}'); checks.append(('gyroid', False))

    if dev != 'cpu':
        nc = 32; chi = builtin('gyroid', nc); print(f'\n6. gyroid {nc}^3 on {dev} and on the CPU (the CPU is slower; a smaller cell)', flush=True)
        try:
            rg = stiffness(chi, nu, 1.0, dev, a.tol, a.max_iter); rc = stiffness(chi, nu, 1.0, 'cpu', a.tol, a.max_iter)
            print('   ' + run_line(dev, rg) + '\n   ' + run_line('cpu', rc))
            report('gpu_vs_cpu', abs(moduli(rg['C']) / moduli(rc['C']) - 1.0).max(), PASS_CPU, 'moduli on the GPU against the CPU, largest relative difference',
                   converged=rg['converged'] and rc['converged'])
        except Exception as exc:
            print(f'   FAIL: {type(exc).__name__}: {exc}'); checks.append(('gpu_vs_cpu', False))

    bad = [c for c, g in checks if not g]
    print(f'\n{"all " + str(len(checks)) + " checks passed" if not bad else str(len(bad)) + " of " + str(len(checks)) + " checks FAILED: " + ", ".join(bad)} on {dev}')
    return not bad


def main():
    ap = argparse.ArgumentParser(description='Stiffness of a voxel cell by the solver of the Neural tab, on the GPU or the CPU (see STIFFNESS_CHECK)')
    ap.add_argument('cells', nargs='*', help='files, folders or built-in cells; none = the self-test')
    ap.add_argument('--device', default='auto', choices=('auto', 'cuda', 'mps', 'xpu', 'cpu'))
    ap.add_argument('--n', type=int, help='grid of a built-in cell (64), or of a .raw file')
    ap.add_argument('--e_gpa', type=float); ap.add_argument('--nu', type=float); ap.add_argument('--aniso', type=float)
    ap.add_argument('--set', help='group of an HDF5 file')
    ap.add_argument('--tol', type=float, default=1e-5); ap.add_argument('--max_iter', type=int, default=10000)
    ap.add_argument('--compare_cpu', action='store_true'); ap.add_argument('--json')
    a = ap.parse_args(); a.material_given = any(getattr(a, k) is not None for k in ('e_gpa', 'nu', 'aniso'))

    found, notes = devices()
    print(f'Python {sys.version.split()[0]} ({sys.executable}), PyTorch {torch.__version__}, NumPy {np.__version__}, {platform.system()} {platform.machine()}')
    print(f'devices PyTorch sees: {", ".join(found)}' + ''.join(f'\n   {x}' for x in notes))
    dev = found[0] if a.device == 'auto' else a.device
    if dev not in found:
        print(f'--device {dev}: PyTorch sees no such device here'); return 2
    print(f'solving on {dev}: {describe(dev)}' + ('' if dev != 'cpu' or a.device == 'cpu' else '  (no GPU found: the CPU is slow, see STIFFNESS_CHECK)'))

    results = []
    if not a.cells:
        ok = self_test(a, dev, results)
    else:
        try:
            cells = [c for spec in a.cells for c in read_cell(spec, a)]
        except SystemExit as exc:                                                   # a missing file, a wrong size, no h5py
            print(exc); return 2
        ok = True; table = []
        for label, chi, law, stored, cnotes in cells:
            good = solve_cell(label, chi, law, stored, cnotes, a, dev, results); ok &= good
            table.append((label, chi.shape[0], good, results[-1] if results and results[-1].get('cell') == label else None))
        if len(cells) > 1:
            print(f'\n{"cell":<34} {"grid":>6} {"seconds":>8} {"Ex / E":>8} {"vs file":>9}  result')
            for label, n, good, rec in table:
                if rec is None:
                    print(f'{label[:34]:<34} {n:>4}^3 {"":>8} {"":>8} {"":>9}  FAIL'); continue
                e = rec['moduli_Pa']['Ex'] / (rec['material']['e_gpa'] * 1e9); d = rec.get('stored_diff_E_pct')
                print(f'{label[:34]:<34} {n:>4}^3 {rec["seconds"]:8.1f} {e:8.4f} {"" if d is None else f"{d:.4f} %":>9}  {"PASS" if good else "FAIL"}')
        print(f'\n{"everything passed" if ok else "SOMETHING FAILED (see above)"}')
    if a.json:
        pathlib.Path(a.json).write_text(json.dumps(results, indent=1)); print(f'results -> {a.json}')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())

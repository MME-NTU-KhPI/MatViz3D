"""Stiffness of a solid / void voxel cell on the GPU (PyTorch): the solve of cellstiffness.cpp, for the Neural tab.

The program starts this script as a separate process (cellstiffness_gpu.cpp) when an interpreter with PyTorch and an Apple GPU
is found; without one the program solves on the CPU as before. Same equations and settings as the C++ solver the dataset was
solved with (fft_homog.hpp, cellstiffness.cpp):

  unknown      the zero-mean compatible strain fluctuation e of the periodic cell, strain = E + e, for six unit loads E
  equation     G : (C : (E + e)) = 0, G the compatibility projection in Fourier space; conjugate gradients on A e = G : C : e
               with b = -A E, the six loads as one batch; a load stops at |r| / |b| < tol
  projection   Willot's rotated scheme: wave vector xi_j = sin(pi q_j / n) prod_{m != j} cos(pi q_m / n)
  void         a soft isotropic phase, E void = eta x C11 of the solid, nu 0.3; eta 1e-4, and a second solve with 1e-6 when
               the solid holds less than 98.5 % of the strain energy in some load
  material     the law of the dataset: E, nu in the layer plane, E and G across the layers (axis z) multiplied by the anisotropy

The Apple GPU computes in single precision. That reaches the tolerance while the void is 1e-4 of the solid (stored dataset
answers are reproduced to 0.004 % of E, median of 31 cells). With a void of 1e-6 the residual the recurrence carries drifts
from the true one (answers off by up to 0.7 %, one cell of nine did not converge), so there the true residual b - A e is
computed in double precision on the CPU and the GPU solves only the corrections (iterative refinement); the same cells then
agree with the dataset to 0.003 %. The same solver as ~/claude-code/cellgen/fft_gpu.py, kept here so the program needs
nothing outside its source tree.

    python3 gpu_stiffness.py --probe
        prints "OK <device>" when PyTorch with a GPU is there, else "NO <reason>"
    python3 gpu_stiffness.py --cell <file> --n 64 --e_gpa 2 --nu 0.38 --aniso 1 [--tol 1e-5 --max_iter 10000
                             --void 1e-4 --void_fallback 1e-6 --min_share 0.985]
        <file>: n^3 bytes, index (iz * n + iy) * n + ix, nonzero = solid (cellstiff::occupancy)

Lines on stdout: "READY <device>", "P <pass> <iteration> <residual> <loads done>" a few times per second, "F <json>" the first
solve when a second one follows, "R <json>" the answer, "E <message>" on a failure. The json: C (6 x 6, Pa, order xx, yy, zz,
xy, yz, xz, engineering shear), void_eta, share, iterations, converged, seconds, refined (refinement steps).
"""
import argparse, json, math, sys, time


def say(text):
    sys.stdout.write(text + '\n'); sys.stdout.flush()


def probe():
    try:
        import torch
    except Exception as e:                                                      # no PyTorch for this interpreter
        return None, f'{type(e).__name__}: {e}'
    if torch.backends.mps.is_available(): return 'mps', ''
    if torch.cuda.is_available(): return 'cuda', ''
    return None, 'PyTorch has no GPU here'


PERM = [0, 1, 2, 5, 3, 4]                                                       # order xx, yy, zz, xy, yz, xz from Mandel order xx, yy, zz, yz, xz, xy


def law_mandel(np, nu, r):
    """stiffness of the printed material relative to its E, Mandel components; and its C11"""
    g = 1.0 / (2.0 * (1.0 + nu)); S = np.zeros((6, 6))                         # Voigt order 11, 22, 33, 23, 13, 12
    S[0, 0] = S[1, 1] = 1.0; S[2, 2] = 1.0 / r
    for a, b in ((0, 1), (0, 2), (1, 2)): S[a, b] = S[b, a] = -nu
    S[3, 3] = S[4, 4] = 1.0 / (r * g); S[5, 5] = 1.0 / g
    s = np.array([1.0, 1.0, 1.0, math.sqrt(2.0), math.sqrt(2.0), math.sqrt(2.0)]); C = np.linalg.inv(S); return C * np.outer(s, s), C[0, 0]


def iso_mandel(np, E, nu):
    lam = E * nu / ((1.0 + nu) * (1.0 - 2.0 * nu)); mu = 0.5 * E / (1.0 + nu); C = np.zeros((6, 6))
    C[:3, :3] = lam; C[range(3), range(3)] = lam + 2.0 * mu; C[range(3, 6), range(3, 6)] = 2.0 * mu
    return C


def projector(np, n):
    """compatibility projection of every Fourier mode as a 6 x 6 matrix in Mandel components: (6, 6, n, n, n // 2 + 1), float64"""
    k = np.arange(n); kk = np.where(k <= n // 2, k, k - n).astype(np.float64); ang = math.pi * kk / n; nyq = (n % 2 == 0) & (2 * np.abs(kk) == n)
    rs = np.where(nyq, np.sign(kk), np.sin(ang)); rc = np.where(nyq, 0.0, np.cos(ang)); h = n // 2 + 1
    v = np.stack([rs[:, None, None] * rc[None, :, None] * rc[None, None, :h], rc[:, None, None] * rs[None, :, None] * rc[None, None, :h],
                  rc[:, None, None] * rc[None, :, None] * rs[None, None, :h]], -1)
    q = np.sqrt((v * v).sum(-1)); ok = q > 0; d = v / np.where(ok, q, 1.0)[..., None]                    # unit direction of the mode
    T = np.zeros((6, 3, 3)); s = 1.0 / math.sqrt(2.0)                           # the six Mandel basis tensors
    T[0, 0, 0] = T[1, 1, 1] = T[2, 2, 2] = 1.0; T[3, 1, 2] = T[3, 2, 1] = s; T[4, 0, 2] = T[4, 2, 0] = s; T[5, 0, 1] = T[5, 1, 0] = s
    m = np.einsum('cij,xyzj->cxyzi', T, d); qq = np.einsum('cxyzi,xyzi->cxyz', m, d)
    g = d[None, ..., :, None] * m[..., None, :] + m[..., :, None] * d[None, ..., None, :] - qq[..., None, None] * d[None, ..., :, None] * d[None, ..., None, :]
    r2 = math.sqrt(2.0); P = np.stack([g[..., 0, 0], g[..., 1, 1], g[..., 2, 2], r2 * g[..., 1, 2], r2 * g[..., 0, 2], r2 * g[..., 0, 1]], 0)
    return P * ok[None, None]


class Solver:
    def __init__(self, chi, nu, r, device):
        import numpy as np, torch
        self.np, self.torch, self.dev = np, torch, torch.device(device); self.chi = torch.from_numpy(chi); self.n = chi.shape[0]; self.nu, self.r = nu, r
        self.P = projector(np, self.n); self.Pt = {}; self.steps = 0; self.progress = None

    def operator(self, eta, dev, dtype):
        """the cell on one device in one precision: A(x) = G : C : x for fields x (6 loads, 6 components, n, n, n), and the
        mean stress of every load with the share of the strain energy the solid holds"""
        torch, np, n = self.torch, self.np, self.n; Cs, c11 = law_mandel(np, self.nu, self.r); Cv = iso_mandel(np, eta * c11, 0.3); dims = (-3, -2, -1)
        key = (str(dev), dtype)
        if key not in self.Pt: self.Pt[key] = torch.tensor(self.P, dtype=dtype).to(dev)
        P = self.Pt[key]; Cs_t = torch.tensor(Cs, dtype=dtype).to(dev); D = torch.tensor(Cs - Cv, dtype=dtype).to(dev); flat = self.chi.to(dtype).to(dev).reshape(1, 1, -1)
        lam, mu2 = float(Cv[0, 1]), float(Cv[3, 3])

        def stress(x):
            f = x.reshape(6, 6, -1); s = torch.matmul(D, f).mul_(flat).add_(f, alpha=mu2)      # void: lam tr(x) 1 + 2 mu x, plus (solid - void) : x in the solid
            s[:, :3] += f[:, :3].sum(1, keepdim=True) * lam; return s.reshape(x.shape)

        def A(x):
            h = torch.view_as_real(torch.fft.rfftn(stress(x), dim=dims)); h = torch.einsum('ijxyz,ljxyzc->lixyzc', P, h)
            return torch.fft.irfftn(torch.view_as_complex(h.contiguous()), s=(n, n, n), dim=dims)

        def share(eps):
            sig = stress(eps); f = eps.reshape(6, 6, -1); return sig.mean((2, 3, 4)), (f * torch.matmul(Cs_t, f) * flat).sum((1, 2)) / (eps * sig).sum((1, 2, 3, 4))
        return A, share

    def cg(self, A, b, tol, maxit, every=10, shown=None):
        """conjugate gradients from zero for six right sides at once. A side stops when the residual the recurrence carries is
        under tol |b|, or at a breakdown (p.Ap <= 0: the precision is used up). -> x, residual / |b|, steps per side"""
        torch = self.torch; dot = lambda u, v: (u * v).sum((1, 2, 3, 4))
        res = b.clone(); x = torch.zeros_like(b); p = b.clone(); rr = dot(res, res); bnorm = rr.sqrt(); zero = torch.zeros_like(rr)
        active = bnorm > 0; iters = torch.zeros(6, dtype=torch.long, device=b.device); rel = torch.where(active, torch.ones_like(rr), zero); it = 0
        while it < maxit:
            Ap = A(p); pAp = dot(p, Ap); active = active & (pAp > 0); alpha = torch.where(active, rr / pAp, zero)[:, None, None, None, None]
            x.addcmul_(p, alpha); res.addcmul_(Ap, alpha, value=-1.0); new = dot(res, res)
            rel = torch.where(active, new.sqrt() / bnorm, rel); iters = iters + active.long(); active = active & (rel >= tol)
            beta = torch.where(active, new / rr, zero)[:, None, None, None, None]; rr = torch.where(active, new, rr)
            p.mul_(beta).add_(res); it += 1; self.steps += 1
            if it % every == 0:
                left = int(active.sum())
                if self.progress: self.progress(self.steps, float(rel.max()) if shown is None else shown, 6 - left)
                if left == 0: break
        return x, rel, iters

    def solve(self, eta, tol, maxit, inner=1e-3, outer=12):
        """six unit loads with the void at eta -> dict (C relative to E of the material, in the program's order)"""
        torch, np, n, dev = self.torch, self.np, self.n, self.dev; t0 = time.time(); f32 = torch.float32; self.steps = 0
        eye = lambda d, t: torch.eye(6, dtype=t)[:, :, None, None, None].expand(6, 6, n, n, n).to(d)
        A, share = self.operator(eta, dev, f32); done = False; steps = 0
        if eta >= 1e-5:
            E = eye(dev, f32); e, rel, iters = self.cg(A, -A(E), tol, maxit); done = bool((rel < tol).all())
            if done: sig, sh = share(E + e); rel, iters = rel.cpu().double(), iters.cpu()
        if not done:                                                            # the true residual in double precision on the CPU, corrections in single
            cpu = torch.device('cpu'); f64 = torch.float64; A64, share64 = self.operator(eta, cpu, f64); dot = lambda u, v: (u * v).sum((1, 2, 3, 4))
            E = eye(cpu, f64); b = -A64(E); bn = dot(b, b).sqrt(); e = torch.zeros_like(b); res = b.clone(); iters = torch.zeros(6, dtype=torch.long)
            # a load whose stress is already in equilibrium (b = 0: shear in the plane of flat layers) is solved by e = 0;
            # its relative residual is 0, not 0 / 0 (that NaN reported a correct answer as not converged)
            relof = lambda r: torch.where(bn > 0, dot(r, r).sqrt() / bn.clamp_min(1e-300), torch.zeros_like(bn)); rel = relof(res)
            while steps < outer and bool((rel >= tol).any()) and int(iters.max()) < maxit:
                s = dot(res, res).sqrt(); s = torch.where(rel >= tol, s, torch.zeros_like(s))[:, None, None, None, None]             # a side that is done gets a zero right side
                d, _, its = self.cg(A, torch.where(s > 0, res / s.clamp_min(1e-300), torch.zeros_like(res)).float().to(dev), inner, maxit - int(iters.max()), shown=float(rel.max()))
                e += d.cpu().double() * s; res = b - A64(e); rel = relof(res); iters += its.cpu(); steps += 1
            sig, sh = share64(E + e)
        s = np.array([1.0, 1.0, 1.0, math.sqrt(2.0), math.sqrt(2.0), math.sqrt(2.0)]); CM = sig.T.cpu().double().numpy(); CM = 0.5 * (CM + CM.T)   # column j: mean stress of load j
        return {'C': (CM / np.outer(s, s))[np.ix_(PERM, PERM)], 'void_eta': eta, 'share': float(sh.min()), 'iterations': int(iters.sum()), 'converged': bool((rel < tol).all()),
                'residual': float(rel.max()), 'refined': steps, 'seconds': time.time() - t0}


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--probe', action='store_true'); ap.add_argument('--cell'); ap.add_argument('--n', type=int)
    ap.add_argument('--e_gpa', type=float, default=2.0); ap.add_argument('--nu', type=float, default=0.38); ap.add_argument('--aniso', type=float, default=1.0)
    ap.add_argument('--tol', type=float, default=1e-5); ap.add_argument('--max_iter', type=int, default=10000); ap.add_argument('--void', type=float, default=1e-4)
    ap.add_argument('--void_fallback', type=float, default=1e-6); ap.add_argument('--min_share', type=float, default=0.985); a = ap.parse_args()
    device, why = probe()
    if a.probe:
        say(f'OK {device}' if device else f'NO {why}'); return 0 if device else 1
    if not device:
        say(f'E {why}'); return 1
    import numpy as np
    raw = np.fromfile(a.cell, dtype=np.uint8)
    if a.n is None or raw.size != a.n ** 3:
        say(f'E the cell file holds {raw.size} bytes, expected n^3 = {(a.n or 0) ** 3}'); return 1
    chi = np.ascontiguousarray((raw.reshape(a.n, a.n, a.n) != 0).transpose(2, 1, 0))                    # [x][y][z]
    solver = Solver(chi, a.nu, a.aniso, device); say(f'READY {device}'); E = a.e_gpa * 1e9; shown = [0.0]; t0 = time.time()

    def out(r, first=None):
        d = dict(r); d['C'] = (r['C'] * E).tolist(); d['passes'] = 2 if first else 1; d['share_first'] = first['share'] if first else r['share']
        d['iterations'] = r['iterations'] + (first['iterations'] if first else 0); d['seconds'] = time.time() - t0; return json.dumps(d)

    def run(number, eta):
        def progress(step, residual, loads):
            if time.time() - shown[0] >= 0.15: shown[0] = time.time(); say(f'P {number} {step} {residual:.3e} {loads}')
        solver.progress = progress; return solver.solve(eta, a.tol, a.max_iter)

    r = run(0, a.void)
    if a.void_fallback > 0 and not (r['share'] >= a.min_share):
        say('F ' + out(r)); r2 = run(1, a.void_fallback); say('R ' + out(r2, r))
    else:
        say('R ' + out(r))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as e:                                                      # the program falls back to its own solver
        say(f'E {type(e).__name__}: {e}'); sys.exit(1)

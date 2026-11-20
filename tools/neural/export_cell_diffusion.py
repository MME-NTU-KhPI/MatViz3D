"""Export the diffusion network of the cell generator (cellgen, network 2) for MatViz3D.

    /opt/anaconda3/bin/python3 tools/neural/export_cell_diffusion.py --run dif6 --ckpt gen_step005000.pt   -> models/cell_diffusion.bin
    ... --out models/cell_diffusion.bin --note "..." --reference

The file is read by celldiffusion.cpp (class CellDiffusion). Little-endian:
    "MVCELLF2"  ("MVCELLF1", the file of run dif2 written before 2026-11-14, has no recipe fields: the noise, an added vector)
    int32   code channels, code size, numbers in a request, blocks per level, levels, channels[levels], attention[levels],
            attention heads, groups of GroupNorm, noise levels, level the sampling starts from, sampling steps
    int32   what the network returns (0 the noise, 1 v), how the request enters (0 an added vector, 1 tokens as well)
    float32 offset of the noise per channel, clamp of the clean code, guidance the run was checked with
    int32   training step; int32 length + bytes of the name ("dif2 step 30000"); int32 length + bytes of the note (--note:
            what was measured on the cells of this model, shown in the Neural tab next to its name)
    int32   targets; for each: int32 length + bytes of its key, int32 kind (0 plain, 1 ln, 2 ln(1 + x), 3 ln(x / E material)),
            float32 lo, hi, mean, std
    float32 mean[2], std[2] of the material (Poisson's ratio, anisotropy); mean[zc], std[zc] of the code channels
    tensors of the averaged weights in the order of the state dict (= the order of the forward pass), each int32 count + float32 values

The weights are 23.5 M (dif2) or 25.5 M (dif3 - dif6) float32 = 94 or 102 MB, so the file is not embedded in the program and
not kept in git: MatViz3D looks for it in MATVIZ_CELL_DIFFUSION, next to the executable (models/) and in the models/ folder of
the source tree.

--reference prints what the C++ test compares with (tests/test_celldiffusion.cpp): what PyTorch returns for a fixed input, and
the code after a 3-step sampling that starts from that input.
"""
import argparse, math, pathlib, struct, sys
import numpy as np, torch

HERE = pathlib.Path(__file__).resolve().parent
ap = argparse.ArgumentParser()
ap.add_argument('--cellgen', default=str(pathlib.Path.home() / 'claude-code/cellgen'))
ap.add_argument('--run', default='dif6'); ap.add_argument('--ckpt', default='best.pt')
ap.add_argument('--out', default=str(HERE.parent.parent / 'models/cell_diffusion.bin'))
ap.add_argument('--note', default='', help='what was measured on the cells of this model; the Neural tab shows it as is')
ap.add_argument('--reference', action='store_true', help='print the output of the network for the reference input, for the C++ test')
a = ap.parse_args()
sys.path.insert(0, a.cellgen)
from diffusion import UNet, T, cosine_schedule, sample                           # noqa: E402

ck = torch.load(pathlib.Path(a.cellgen) / 'runs' / a.run / a.ckpt, map_location='cpu')
cfg, norm, sd = ck['cfg'], ck['norm'], ck['avg']
net = UNet(**cfg); net.load_state_dict(sd); net.eval()
KIND = {'plain': 0, 'log': 1, 'log1p': 2, 'logE': 3}
name = f'{a.run} step {ck["step"]}'.encode(); note = a.note.encode()
PRED, COND = cfg.get('pred', 'eps'), cfg.get('cond', 'add'); GUIDANCE = float(ck['args'].get('guidance', 1.0))
START, STEPS, HEADS, GROUPS, CODE = 999 if PRED == 'v' else 980, 50, 4, 8, 16    # as in diffusion.sample and the generation checks of the run

out = bytearray(b'MVCELLF2')
out += struct.pack(f'<5i{len(cfg["ch"])}i{len(cfg["attn"])}i7i', cfg['zc'], CODE, cfg['nc'], cfg['nb'], len(cfg['ch']), *cfg['ch'], *cfg['attn'],
                   HEADS, GROUPS, T, START, STEPS, int(PRED == 'v'), int(COND == 'cross'))
out += struct.pack('<3f', float(ck.get('offset', 0.0)), float(norm['code_absmax']), GUIDANCE)
out += struct.pack('<2i', int(ck['step']), len(name)) + name + struct.pack('<i', len(note)) + note
out += struct.pack('<i', len(norm['keys']))
for j, key in enumerate(norm['keys']):
    k = key.encode(); out += struct.pack('<i', len(k)) + k + struct.pack('<i4f', KIND[norm['kind'][j]], norm['lo'][j], norm['hi'][j], norm['mean'][j], norm['std'][j])
out += struct.pack('<4f', *norm['mat_mean'], *norm['mat_std'])
out += struct.pack(f'<{2 * cfg["zc"]}f', *norm['code_mean'], *norm['code_std'])
count = 0
for key, v in sd.items():
    v = v.detach().float().contiguous().numpy().astype('<f4'); out += struct.pack('<i', v.size) + v.tobytes(); count += v.size
pathlib.Path(a.out).parent.mkdir(parents=True, exist_ok=True); pathlib.Path(a.out).write_bytes(out)
m = ck.get('metrics', {})
print(f'{a.out}: {len(out):,} bytes, {count:,} weights in {len(sd)} tensors, {name.decode()}, val loss {m.get("val_loss", "?")}, '
      f'returns {"v" if PRED == "v" else "the noise"}, request {"as tokens too" if COND == "cross" else "as an added vector"}, sampling from level {START}, '
      f'offset {ck.get("offset", 0.0)}, clamp {norm["code_absmax"]:.4f}, guidance {GUIDANCE:g}')

if a.reference:
    # The input of tests/test_celldiffusion.cpp: a smooth periodic pattern, level 500, a request for 6 targets.
    n = CODE; i, j, k = np.meshgrid(np.arange(n), np.arange(n), np.arange(n), indexing='ij'); w = 2 * math.pi / n
    x = np.stack([1.3 * np.sin(w * ((c + 1) * i + 2 * j + 3 * k) + 0.5 * c) * np.cos(w * (i - (c + 2) * j + k)) + 0.1 * c for c in range(cfg['zc'])]).astype(np.float32)
    req = np.zeros(cfg['nc'], np.float32); nt = len(norm['keys']); E, nu, r = 2.0, 0.38, 1.0
    for key, val in {'Ex': 3.0e8, 'Ey': 3.0e8, 'Ez': 3.0e8, 'density': 0.30, 'EmaxEmin': 1.5, 'nuMin': 0.1}.items():
        jx = norm['keys'].index(key); kind = norm['kind'][jx]
        val = math.log(val / (E * 1e9)) if kind == 'logE' else math.log(val) if kind == 'log' else math.log1p(val) if kind == 'log1p' else val
        req[jx] = (min(max(val, norm['lo'][jx]), norm['hi'][jx]) - norm['mean'][jx]) / norm['std'][jx]; req[nt + jx] = 1.0
    req[2 * nt:] = (np.array([nu, r]) - np.array(norm['mat_mean'])) / np.array(norm['mat_std'])
    with torch.no_grad(): e = net(torch.from_numpy(x)[None], torch.tensor([500]), torch.from_numpy(req)[None])[0].numpy()
    print('request:', ', '.join(f'{v:.6f}' for v in req[np.flatnonzero(req)]), '| at', np.flatnonzero(req).tolist())
    print(f'output ({"v" if PRED == "v" else "the noise"}): mean {e.mean():.6f}, rms {np.sqrt((e ** 2).mean()):.6f}')
    probes = [(0, 0, 0, 0), (1, 1, 2, 3), (2, 15, 0, 7), (3, 8, 8, 8), (0, 5, 11, 2), (3, 15, 15, 15), (1, 0, 9, 14), (2, 7, 3, 12)]
    print('probes: ' + ', '.join(f'{{{c}, {p}, {q}, {s}, {e[c, p, q, s]:.6f}f}}' for c, p, q, s in probes))
    # The sampler: 3 steps that start from the reference input in place of noise, with the guidance of the file.
    z = sample(net, torch.from_numpy(req)[None], 3, GUIDANCE, norm['code_absmax'], noise=torch.from_numpy(x)[None], schedule=cosine_schedule())[0].numpy()
    print(f'code after 3 steps at guidance {GUIDANCE:g}: mean {z.mean():.6f}, rms {np.sqrt((z ** 2).mean()):.6f}')
    print('probes: ' + ', '.join(f'{{{c}, {p}, {q}, {s}, {z[c, p, q, s]:.6f}f}}' for c, p, q, s in probes))

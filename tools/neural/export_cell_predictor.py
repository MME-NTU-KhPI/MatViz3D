"""Export the property predictor of the cell generator (cellgen, network 3) with the encoder of the autoencoder for MatViz3D.

    /opt/anaconda3/bin/python3 tools/neural/export_cell_predictor.py            -> models/cell_predictor.bin
    ... --run pred1 --ckpt best.pt --out models/cell_predictor.bin --note "..." --reference

The file is read by cellpredictor.cpp (class CellPredictor): voxel cell 64^3 -> code (encoder of the autoencoder the predictor
was trained on) -> the 24 properties of a request (predictor). Little-endian:
    "MVCELLP1"
    int32   cell size, code channels, code size, stem of the encoder, groups of GroupNorm, attention heads
    int32   levels of the encoder, channels[levels], blocks[levels]
    int32   levels of the predictor, channels[levels], attention[levels], blocks per level, width of the hidden layer,
            numbers of the material (2)
    int32   training step; int32 length + bytes of the name ("pred1 step 14000"); int32 length + bytes of the note (--note:
            what was measured on this model, shown in the Neural tab)
    int32   targets; for each: int32 length + bytes of its key, int32 kind (0 plain, 1 ln, 2 ln(1 + x), 3 ln(x / E material)),
            float32 lo, hi, mean, std
    float32 mean[2], std[2] of the material (Poisson's ratio, anisotropy); mean[zc], std[zc] of the code channels
    tensors of the averaged weights, the encoder first, then the predictor, each in the order of its state dict (= the order
    of the forward pass), each int32 count + float32 values

The weights are 2.6 M float32 = 10.6 MB; the file is not embedded in the program and not kept in git: MatViz3D looks for it in
MATVIZ_CELL_PREDICTOR, next to the executable (models/) and in the models/ folder of the source tree.

--reference prints what the C++ test compares with (tests/test_cellpredictor.cpp): PyTorch on a fixed cell and a fixed code.
"""
import argparse, math, pathlib, struct, sys
import numpy as np, torch

HERE = pathlib.Path(__file__).resolve().parent
ap = argparse.ArgumentParser()
ap.add_argument('--cellgen', default=str(pathlib.Path.home() / 'claude-code/cellgen'))
ap.add_argument('--run', default='pred1'); ap.add_argument('--ckpt', default='best.pt')
ap.add_argument('--out', default=str(HERE.parent.parent / 'models/cell_predictor.bin'))
ap.add_argument('--note', default='', help='what was measured on this model; the Neural tab shows it as is')
ap.add_argument('--reference', action='store_true', help='print the numbers of the reference inputs for the C++ test')
a = ap.parse_args()
sys.path.insert(0, a.cellgen)
from predictor import Predictor                                                  # noqa: E402
from model import AutoEncoder                                                    # noqa: E402

root = pathlib.Path(a.cellgen)
ck = torch.load(root / 'runs' / a.run / a.ckpt, map_location='cpu'); cfg, norm, sd = ck['cfg'], ck['norm'], ck['avg']
net = Predictor(**cfg); net.load_state_dict(sd); net.eval()
ak = torch.load(root / 'runs' / norm['ae'] / 'best.pt', map_location='cpu'); acfg = ak['cfg']
ae = AutoEncoder(**acfg); ae.load_state_dict(ak['avg']); ae.eval()
enc = [(k, v) for k, v in ae.state_dict().items() if k.startswith('enc.')]
KIND = {'plain': 0, 'log': 1, 'log1p': 2, 'logE': 3}; HEADS, GROUPS, CODE, CELL = 4, 8, 16, 64
name = f'{a.run} step {ck["step"]}'.encode(); note = a.note.encode()

out = bytearray(b'MVCELLP1')
out += struct.pack('<6i', CELL, cfg['zc'], CODE, acfg['stem'], GROUPS, HEADS)
out += struct.pack(f'<i{2 * len(acfg["ch"])}i', len(acfg['ch']), *acfg['ch'], *acfg['blocks'])
out += struct.pack(f'<i{2 * len(cfg["ch"])}i3i', len(cfg['ch']), *cfg['ch'], *cfg['attn'], cfg['nb'], cfg['hid'], cfg['nm'])
out += struct.pack('<2i', int(ck['step']), len(name)) + name + struct.pack('<i', len(note)) + note
out += struct.pack('<i', len(norm['keys']))
for j, key in enumerate(norm['keys']):
    k = key.encode(); out += struct.pack('<i', len(k)) + k + struct.pack('<i4f', KIND[norm['kind'][j]], norm['lo'][j], norm['hi'][j], norm['mean'][j], norm['std'][j])
out += struct.pack('<4f', *norm['mat_mean'], *norm['mat_std'])
out += struct.pack(f'<{2 * cfg["zc"]}f', *norm['code_mean'], *norm['code_std'])
count = 0
for key, v in enc + list(sd.items()):
    v = v.detach().float().contiguous().numpy().astype('<f4'); out += struct.pack('<i', v.size) + v.tobytes(); count += v.size
pathlib.Path(a.out).parent.mkdir(parents=True, exist_ok=True); pathlib.Path(a.out).write_bytes(out)
m = ck.get('metrics', {})
print(f'{a.out}: {len(out):,} bytes, {count:,} weights in {len(enc)} + {len(sd)} tensors, {name.decode()} on the encoder of {norm["ae"]} step {ak["step"]}, '
      f'val loss {m.get("val_loss", "?")}, typical miss of E on held-out cells {m.get("val_E_miss", "?")} %')

if a.reference:
    cm = torch.tensor(norm['code_mean']).view(1, -1, 1, 1, 1); cs = torch.tensor(norm['code_std']).view(1, -1, 1, 1, 1)
    fmt = lambda v: ', '.join(f'{x:.6f}f' for x in v)
    # 1. A cell: a sheet gyroid with two periods along x, |g| < 0.75 (the same formula in the test, in double).
    w = 2 * math.pi / CELL; i, j, k = np.meshgrid(np.arange(CELL) + 0.5, np.arange(CELL) + 0.5, np.arange(CELL) + 0.5, indexing='ij')
    g = np.sin(2 * w * i) * np.cos(w * j) + np.sin(w * j) * np.cos(w * k) + np.sin(w * k) * np.cos(2 * w * i)
    cell = np.abs(g) < 0.75; print(f'cell: {int(cell.sum())} solid voxels, closest |g| to the threshold {np.abs(np.abs(g) - 0.75).min():.2e}')
    with torch.no_grad():
        code = (ae.encode(torch.from_numpy(cell[None, None].astype(np.float32)))[0] - cm) / cs
        z = net(code, torch.tensor([0]), torch.tensor([[(0.38 - norm['mat_mean'][0]) / norm['mat_std'][0], (1.0 - norm['mat_mean'][1]) / norm['mat_std'][1]]], dtype=torch.float32))[0].numpy()
    code = code[0].numpy(); probes = [(0, 0, 0, 0), (1, 1, 2, 3), (2, 15, 0, 7), (3, 8, 8, 8), (0, 5, 11, 2), (3, 15, 15, 15), (1, 0, 9, 14), (2, 7, 3, 12)]
    print(f'code of the cell: mean {code.mean():.6f}, rms {np.sqrt((code ** 2).mean()):.6f}')
    print('code probes: ' + ', '.join(f'{{{c}, {p}, {q}, {s}, {code[c, p, q, s]:.6f}f}}' for c, p, q, s in probes))
    print('properties of the cell, z-scores (level 0, nu 0.38, aniso 1): ' + fmt(z))
    E = 2.0e9; phys = []
    for jx, key in enumerate(norm['keys']):
        v = float(z[jx]) * norm['std'][jx] + norm['mean'][jx]; kind = norm['kind'][jx]
        phys.append(math.exp(v) * E if kind == 'logE' else math.exp(v) if kind == 'log' else math.expm1(v) if kind == 'log1p' else v)
    print('in the units of the dataset for E = 2 GPa: ' + ', '.join(f'{key} {v:.6g}' for key, v in zip(norm['keys'], phys)))
    # 2. A code: the smooth periodic pattern of the diffusion test, level 500, another material.
    n = CODE; i, j, k = np.meshgrid(np.arange(n), np.arange(n), np.arange(n), indexing='ij'); w = 2 * math.pi / n
    x = np.stack([1.3 * np.sin(w * ((c + 1) * i + 2 * j + 3 * k) + 0.5 * c) * np.cos(w * (i - (c + 2) * j + k)) + 0.1 * c for c in range(cfg['zc'])]).astype(np.float32)
    with torch.no_grad():
        z = net(torch.from_numpy(x)[None], torch.tensor([500]), torch.tensor([[(0.33 - norm['mat_mean'][0]) / norm['mat_std'][0], (0.8 - norm['mat_mean'][1]) / norm['mat_std'][1]]], dtype=torch.float32))[0].numpy()
    print('properties of the pattern code, z-scores (level 500, nu 0.33, aniso 0.8): ' + fmt(z))

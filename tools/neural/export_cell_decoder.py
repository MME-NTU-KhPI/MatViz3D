"""Export the decoder of the cell autoencoder (cellgen) for MatViz3D's Neural tab.

    python3 tools/neural/export_cell_decoder.py --cellgen ~/claude-code/cellgen --run ae1
        -> models/cell_decoder.bin (read by celldecoder.cpp, embedded through models.qrc)

The file holds the averaged decoder weights of runs/<run>/best.pt, the layer sizes, the rule
"share of solid >= threshold is solid" and the mean and spread of every code channel over
--cells training cells (what a random code is drawn from until the diffusion network exists).

Layout, little-endian: "MVCELLD1"; int32 zc, code size, stem, groups, levels; int32 channels[levels];
int32 blocks[levels]; float32 threshold; float32 mean[zc]; float32 std[zc]; int32 training step;
char name[32]; then every tensor in the order of the forward pass as int32 count + float32 values.

--reference prints the numbers tests/test_celldecoder.cpp compares with (PyTorch on the same code).
"""
import argparse, pathlib, struct, sys
import numpy as np, torch

ap = argparse.ArgumentParser()
ap.add_argument('--cellgen', default=str(pathlib.Path.home() / 'claude-code/cellgen'), help='folder with model.py, data/, runs/')
ap.add_argument('--run', default='ae1'); ap.add_argument('--ckpt', default='best.pt')
ap.add_argument('--out', default=str(pathlib.Path(__file__).resolve().parents[2] / 'models/cell_decoder.bin'))
ap.add_argument('--cells', type=int, default=1024, help='training cells the code statistics are taken over')
ap.add_argument('--threshold', type=float, default=0.4375, help='share of solid from which a voxel is solid (3.5 of 8 sub-voxels)')
ap.add_argument('--reference', action='store_true'); ap.add_argument('--dump', default='', help='with --reference: folder for the test code and the PyTorch logits (.npy)')
a = ap.parse_args()
root = pathlib.Path(a.cellgen).expanduser(); sys.path.insert(0, str(root))
from model import AutoEncoder  # noqa: E402

ck = torch.load(root / 'runs' / a.run / a.ckpt, map_location='cpu'); cfg = ck['cfg']
net = AutoEncoder(**cfg); net.load_state_dict(ck['avg']); net.eval()

import pandas as pd  # noqa: E402
meta = pd.read_parquet(root / 'data/meta.parquet'); occ = np.load(root / 'data/occ8.npy', mmap_mode='r')
train = np.flatnonzero(meta.ok & ~meta.test); pick = train[np.linspace(0, len(train) - 1, min(a.cells, len(train))).astype(int)]
codes = []
with torch.no_grad():
    for j in range(0, len(pick), 16):
        x = torch.from_numpy(np.stack([occ[i] for i in pick[j:j + 16]])).float().div_(8.0).unsqueeze(1)
        codes.append(net.encode(x)[0].numpy())
codes = np.concatenate(codes); mean = codes.mean(axis=(0, 2, 3, 4)); std = codes.std(axis=(0, 2, 3, 4))

dec = [(k, v.numpy().astype('<f4')) for k, v in net.state_dict().items() if k.startswith('dec.')]
levels = len(cfg['ch']); name = f'{a.run} step {ck["step"]}'.encode()[:31]
with open(a.out, 'wb') as f:
    f.write(b'MVCELLD1'); f.write(struct.pack('<5i', cfg['zc'], 16, cfg['stem'], 8, levels))
    f.write(struct.pack(f'<{levels}i', *cfg['ch'])); f.write(struct.pack(f'<{levels}i', *cfg['blocks']))
    f.write(struct.pack('<f', a.threshold)); f.write(mean.astype('<f4').tobytes()); f.write(std.astype('<f4').tobytes())
    f.write(struct.pack('<i', int(ck['step']))); f.write(name.ljust(32, b'\0'))
    for _, v in dec:
        f.write(struct.pack('<i', v.size)); f.write(v.tobytes())
n = sum(v.size for _, v in dec)
print(f'{a.out}: {pathlib.Path(a.out).stat().st_size / 1e6:.2f} MB, {len(dec)} tensors, {n} weights, run {a.run}, step {ck["step"]}')
print('code mean per channel', np.round(mean, 4).tolist(), 'spread', np.round(std, 4).tolist(), f'({len(pick)} training cells)')

if a.reference:
    # The test's code: a smooth periodic pattern, the same formula in C++ (float32 there, so compare with a tolerance).
    zc = cfg['zc']; i, j, k = np.meshgrid(np.arange(16), np.arange(16), np.arange(16), indexing='ij'); w = 2 * np.pi / 16
    z = np.stack([mean[c] + std[c] * np.sin(w * ((c + 1) * i + 2 * j + 3 * k) + 0.5 * c) * np.cos(w * (i - (c + 2) * j + k)) for c in range(zc)])
    with torch.no_grad():
        logits = net.dec(torch.from_numpy(z[None].astype('f4')))[0, 0].numpy()
    p = 1 / (1 + np.exp(-logits)); probes = [(0, 0, 0), (1, 2, 3), (17, 40, 5), (31, 31, 31), (32, 0, 63), (63, 63, 63), (10, 50, 30), (45, 7, 22)]
    print('reference: solid voxels', int((p >= a.threshold).sum()), 'mean share', float(p.mean()))
    print('reference probes (i, j, k, logit):', ', '.join(f'{{{i}, {j}, {k}, {logits[i, j, k]:.5f}f}}' for i, j, k in probes))
    if a.dump:
        np.save(pathlib.Path(a.dump) / 'ref_code.npy', z.astype('f4')); np.save(pathlib.Path(a.dump) / 'ref_logits.npy', logits)

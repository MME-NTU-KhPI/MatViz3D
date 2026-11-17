"""Compare the C++ diffusion (CellDiffusion) with PyTorch on the files written by tools/neural/diffusion_check.cpp.

    /opt/anaconda3/bin/python3 tools/neural/check_cell_diffusion.py <folder of diffusion_check> [--run dif6 --ckpt gen_step005000.pt]

Same request, same starting noise: one pass of the network (predicted.bin), the code after the whole sampling (code.bin) and
the decoded cell (cell.bin) against PyTorch on the CPU. Rounding differs between the two (order of the sums in a convolution),
so the codes agree to about 1e-4 and a few voxels at the threshold may flip.
"""
import argparse, pathlib, sys
import numpy as np, torch

ap = argparse.ArgumentParser(); ap.add_argument('folder')
ap.add_argument('--cellgen', default=str(pathlib.Path.home() / 'claude-code/cellgen'))
ap.add_argument('--run', default='dif6'); ap.add_argument('--ckpt', default='best.pt'); ap.add_argument('--ae', default='ae1')
a = ap.parse_args(); sys.path.insert(0, a.cellgen)
from diffusion import UNet, cosine_schedule, sample                            # noqa: E402
from model import AutoEncoder                                                    # noqa: E402

gen = pathlib.Path(a.cellgen); d = pathlib.Path(a.folder)
load = lambda name, shape: torch.from_numpy(np.fromfile(d / name, np.float32).reshape(shape).copy())
ck = torch.load(gen / 'runs' / a.run / a.ckpt, map_location='cpu'); net = UNet(**ck['cfg']); net.load_state_dict(ck['avg']); net.eval(); norm = ck['norm']
req, x, noise = load('request.bin', (1, -1)), load('input.bin', (1, 4, 16, 16, 16)), load('noise.bin', (1, 4, 16, 16, 16))
with torch.no_grad(): e = net(x, torch.tensor([500]), req)
diff = (e - load('predicted.bin', e.shape)).abs()
print(f'one pass (level 500): largest difference {diff.max():.2e}, mean {diff.mean():.2e}; the values are within +-{e.abs().max():.2f}')
code = sample(net, req, 50, float(ck['args'].get('guidance', 1.0)), norm['code_absmax'], noise=noise, schedule=cosine_schedule())
mine = load('code.bin', code.shape); diff = (code - mine).abs()
print(f'code after 50 steps: largest difference {diff.max():.2e}, mean {diff.mean():.2e}; the values are within +-{code.abs().max():.2f}')
ak = torch.load(gen / 'runs' / a.ae / 'best.pt', map_location='cpu'); ae = AutoEncoder(**ak['cfg']); ae.load_state_dict(ak['avg']); ae.eval()
cm = torch.tensor(norm['code_mean']).view(1, -1, 1, 1, 1); cs = torch.tensor(norm['code_std']).view(1, -1, 1, 1, 1)
with torch.no_grad(): cell = (torch.sigmoid(ae.dec(code * cs + cm))[0, 0] >= 0.4375).numpy()
got = np.fromfile(d / 'cell.bin', np.uint8).reshape(cell.shape).astype(bool)
print(f'cell: density {cell.mean():.4f} in PyTorch, {got.mean():.4f} in C++; {int((cell != got).sum())} of {cell.size} voxels differ '
      f'(guidance {float(ck["args"].get("guidance", 1.0)):g}, {"v" if ck["cfg"].get("pred") == "v" else "noise"} recipe)')

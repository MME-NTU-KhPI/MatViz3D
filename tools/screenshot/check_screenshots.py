#!/usr/bin/env python3
"""Reference-image and PDF checks of the screenshot pipeline (stage 8).

Runs `MatViz3D --nogui --autostart ... --screenshot <file>` on a few fixed
structures and checks what comes out:

* PNG: same size as the reference in `reference/`, and close to it pixel by
  pixel. GPUs and drivers anti-alias edges a little differently, so the test
  is statistical: mean absolute difference per channel below MEAN_TOL and at
  most OUTLIER_SHARE of the pixels off by more than OUTLIER_LEVEL. A change of
  shading, colours, outlines, crop or legend layout moves both far beyond.
* DPI and size: the PNG's pHYs chunk carries the DPI asked for, and a figure
  asked for at W cm is W cm wide at that DPI.
* PDF: read from the bytes, no tools needed: one page whose MediaBox is the
  figure's size in points; the image model is stored losslessly (no
  /DCTDecode, i.e. no JPEG); the vector model has no image at all; the legend
  and axis letters are outlines (no embedded font).

Usage:
    python tools/screenshot/check_screenshots.py --exe <MatViz3D executable>
    python tools/screenshot/check_screenshots.py --exe <...> --update   # rewrite the references

Exit code 0 when every check passes, 1 otherwise. Needs Pillow and numpy.
The references were made on Windows (Qt 6.12, MinGW); on another platform
run once with --update and compare by eye before trusting them.
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile


import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, 'reference')

MEAN_TOL = 0.6          # mean |difference| per channel, 0..255
OUTLIER_LEVEL = 48      # a pixel this far off (max over channels) is an outlier
OUTLIER_SHARE = 0.004   # at most this share of outliers (outlines alone are ~1.5 %)

VORONOI = ['--algorithm', 'Voronoi', '--size', '32', '--points', '20', '--seed', '7']
GYROID = ['--algorithm', 'Lattice', '--lattice_family', 'gyroid_sheet', '--size', '48',
          '--cells_per_edge', '2', '--relative_density', '0.3', '--seed', '1']

# name, structure, screenshot options, width in cm (None = view size), dpi
CASES = [
    ('voronoi_boundaries.png', VORONOI, ['--outlines', 'boundaries', '--background', 'white'], 8.5, 150),
    ('voronoi_theme.png',      VORONOI, ['--background', 'theme'], None, 96),
    ('gyroid_transparent.png', GYROID,  ['--outlines', 'boundaries', '--background', 'transparent'], 8.5, 150),
    ('gyroid_image.pdf',       GYROID,  ['--background', 'white'], 8.5, 300),
    ('gyroid_vector.pdf',      GYROID,  ['--background', 'white', '--pdf_vector', '--outlines', 'boundaries'], 8.5, 300),
]


def run(exe, structure, opts, width_cm, dpi, out):
    cmd = [exe, '--nogui', '--autostart', '--nologo'] + structure + opts + \
          ['--dpi', str(dpi), '--screenshot', out]
    if width_cm:
        cmd += ['--width_cm', str(width_cm)]
    env = dict(os.environ)
    for k in list(env):
        if k.startswith('MATVIZ_'):
            del env[k]
    r = subprocess.run(cmd, capture_output=True, text=True, errors='replace', env=env,
                       cwd=os.path.dirname(os.path.abspath(exe)), timeout=600)
    return r.returncode, r.stdout + r.stderr


def png_dpi(path):
    with Image.open(path) as im:
        d = im.info.get('dpi')
    return None if d is None else round(float(d[0]))


def compare_png(path, ref):
    a = np.asarray(Image.open(path).convert('RGBA'), dtype=np.int16)
    b = np.asarray(Image.open(ref).convert('RGBA'), dtype=np.int16)
    if a.shape != b.shape:
        return False, 'size %s, reference %s' % (a.shape[1::-1], b.shape[1::-1])
    d = np.abs(a - b)
    mean = d.mean()
    outliers = (d.max(axis=2) > OUTLIER_LEVEL).mean()
    ok = mean <= MEAN_TOL and outliers <= OUTLIER_SHARE
    return ok, 'mean diff %.2f (max %.1f), outliers %.3f%% (max %.1f%%)' % (
        mean, MEAN_TOL, 100 * outliers, 100 * OUTLIER_SHARE)


def pdf_facts(path):
    data = open(path, 'rb').read()
    facts = {'pages': len(re.findall(rb'/Type\s*/Page\b', data)),
             'jpeg': b'/DCTDecode' in data,
             'images': len(re.findall(rb'/Subtype\s*/Image', data)),
             'fonts': len(re.findall(rb'/Type\s*/Font\b', data))}
    m = re.search(rb'/MediaBox\s*\[\s*([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)\s*\]', data)
    facts['size_pt'] = (float(m.group(3)) - float(m.group(1)), float(m.group(4)) - float(m.group(2))) if m else None
    return facts


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--exe', required=True, help='MatViz3D executable')
    ap.add_argument('--update', action='store_true', help='rewrite the PNG references')
    ap.add_argument('--keep', help='directory to keep the outputs in (default: a temporary one)')
    args = ap.parse_args()

    out_dir = args.keep or tempfile.mkdtemp(prefix='matviz_shots_')
    os.makedirs(out_dir, exist_ok=True)
    os.makedirs(REF, exist_ok=True)
    failures = 0

    def check(ok, name, what):
        nonlocal failures
        print('  %-4s %s' % ('ok' if ok else 'FAIL', what))
        if not ok:
            failures += 1

    for name, structure, opts, width_cm, dpi in CASES:
        out = os.path.join(out_dir, name)
        if os.path.exists(out):
            os.remove(out)
        print(name)
        code, log = run(args.exe, structure, opts, width_cm, dpi, out)
        check(code == 0 and os.path.exists(out), name, 'exit code %d, file written' % code)
        if code != 0 or not os.path.exists(out):
            print('\n'.join('      ' + l for l in log.strip().splitlines()[-8:]))
            continue

        if name.endswith('.png'):
            with Image.open(out) as im:
                w, h = im.size
            check(png_dpi(out) == dpi, name, 'DPI in the file %s, asked %d' % (png_dpi(out), dpi))
            if width_cm:
                want = round(width_cm / 2.54 * dpi)
                check(abs(w - want) <= 1, name, 'width %d px, %.1f cm at %d DPI = %d px' % (w, width_cm, dpi, want))
            if 'transparent' in name:
                with Image.open(out) as im:
                    alpha = np.asarray(im.convert('RGBA'))[:, :, 3]
                check(alpha.min() == 0 and alpha.max() == 255, name,
                      'alpha channel used (min %d, max %d)' % (alpha.min(), alpha.max()))
            ref = os.path.join(REF, name)
            if args.update or not os.path.exists(ref):
                Image.open(out).save(ref)
                print('  ref  written', os.path.relpath(ref))
            else:
                ok, what = compare_png(out, ref)
                check(ok, name, 'against the reference: ' + what)
        else:
            f = pdf_facts(out)
            check(f['pages'] == 1, name, '%d page(s)' % f['pages'])
            if width_cm and f['size_pt']:
                want = width_cm / 2.54 * 72
                check(abs(f['size_pt'][0] - want) < 1.0, name,
                      'page width %.1f pt, %.1f cm = %.1f pt' % (f['size_pt'][0], width_cm, want))
            check(not f['jpeg'], name, 'no JPEG (DCTDecode) in the file')
            if 'vector' in name:
                check(f['images'] == 0, name, 'vector model: %d image(s)' % f['images'])
            else:
                check(f['images'] >= 1, name, 'image model: %d image(s)' % f['images'])
            check(f['fonts'] == 0, name, 'labels as outlines: %d embedded font(s)' % f['fonts'])

    print('\n%s: %d failure(s); outputs in %s' % ('PASSED' if failures == 0 else 'FAILED', failures, out_dir))
    return 0 if failures == 0 else 1


if __name__ == '__main__':
    sys.exit(main())

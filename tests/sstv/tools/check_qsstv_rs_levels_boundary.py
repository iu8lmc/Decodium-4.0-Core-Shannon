#!/usr/bin/env python3
"""Compare QSSTV signed-padding behaviour for independently encoded RS levels.

Run after building export_hamdrm_rs_boundary_wave; pass baseline and corrected
QSSTV executable paths built from the same source. This synthesises audio and
does not exercise a sound device or establish EasyPal compatibility.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

if len(sys.argv) != 3:
    raise SystemExit('usage: check_qsstv_rs_levels_boundary.py BASELINE FIXED')
repo = Path(__file__).resolve().parents[3]
exporter = (repo / 'build/tests/sstv/export_hamdrm_rs_boundary_wave').resolve(strict=True)
binaries = [Path(arg).resolve(strict=True) for arg in sys.argv[1:]]
image = (repo / 'tests/sstv/fixtures/easypal-2014-live/reverse-b.jpg').read_bytes()
with tempfile.TemporaryDirectory(prefix='decodium-qsstv-rs-levels-') as tmp:
    root = Path(tmp)
    for level, data_size in ((1, 224), (3, 160), (4, 128)):
        for header in (127, 128):
            case = root / f'rs{level}-{header}'
            case.mkdir()
            padding = (data_size - header - len(image)) % data_size
            source = image + bytes(padding)
            src, raw, wav = case / 'source.jpg', case / 'signal.s16', case / 'signal.wav'
            src.write_bytes(source)
            subprocess.run([str(exporter), str(level), str(src), str(raw)], check=True, timeout=90)
            subprocess.run(['ffmpeg', '-v', 'error', '-f', 's16le', '-ar', '48000', '-ac', '1',
                            '-i', str(raw), '-ac', '2', str(wav)], check=True, timeout=30)
            for name, binary in zip(('baseline', 'fixed'), binaries):
                out = case / name
                out.mkdir()
                for attempt in range(3):
                    result = subprocess.run([str(binary), '--batch', '--drm', '--out-dir', str(out), str(wav)],
                                            env={**os.environ, 'QT_QPA_PLATFORM': 'cocoa'},
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
                    files = list(out.glob('*_drm.jpg'))
                    if result.returncode == 0 or files:
                        break
                if result.returncode or len(files) != 1:
                    raise RuntimeError(f'RS{level} header {header} {name}: no image')
                extra = 121 if header == 128 and name == 'baseline' else 0
                if files[0].read_bytes() != source + bytes(extra):
                    raise RuntimeError(f'RS{level} header {header} {name}: unexpected output')
                print(f'RS{level} header={header} {name} trailing_zero_bytes={extra} attempts={attempt + 1}',
                      flush=True)

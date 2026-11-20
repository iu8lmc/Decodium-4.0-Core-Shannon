#!/usr/bin/env python3
"""Compare two separately built QSSTV receivers on the same RS2 captures.

Arguments are the baseline executable and the executable built from the same
source with qsstv-rs2-unsigned-padding.patch applied. This is a comparison of
the named builds, not proof that another QSSTV release or EasyPal is fixed.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

if len(sys.argv) != 3:
    raise SystemExit('usage: check_qsstv_rs2_unsigned_fix.py BASELINE FIXED')
binaries = [Path(value).resolve(strict=True) for value in sys.argv[1:]]
fixtures = Path(__file__).resolve().parents[1] / 'fixtures' / 'easypal-2014-live'
cases = [('a', 'a', 0, 0), ('b', 'b', 0, 125),
         ('sign127', 'b', 5, 0), ('sign128', 'b', 4, 121)]
with tempfile.TemporaryDirectory(prefix='decodium-qsstv-rs2-unsigned-') as tmp:
    root = Path(tmp)
    for name, reference, source_padding, baseline_extra in cases:
        wav = root / (name + '.wav')
        subprocess.run(['ffmpeg', '-v', 'error', '-i', str(fixtures / ('decodium-rs2-' + name + '.flac')),
                        '-c:a', 'pcm_s16le', str(wav)], check=True, timeout=30)
        original = (fixtures / ('reverse-' + reference + '.jpg')).read_bytes() + bytes(source_padding)
        for mode, binary in zip(('baseline', 'fixed'), binaries):
            out = root / (name + '-' + mode)
            out.mkdir()
            for attempt in range(3):
                result = subprocess.run([str(binary), '--batch', '--drm', '--out-dir', str(out), str(wav)],
                                        env={**os.environ, 'QT_QPA_PLATFORM': 'cocoa'},
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
                images = list(out.glob('*_drm.jpg'))
                if result.returncode == 0 or images:
                    break
            if result.returncode or len(images) != 1:
                raise RuntimeError(f'{name} {mode}: no image after {attempt + 1} attempts')
            extra = baseline_extra if mode == 'baseline' else 0
            if images[0].read_bytes() != original + bytes(extra):
                raise RuntimeError(f'{name} {mode}: unexpected decoded bytes')
            print(name, mode, 'exact=', extra == 0, 'trailing_zero_bytes=', extra,
                  'acquisition_attempts=', attempt + 1, flush=True)

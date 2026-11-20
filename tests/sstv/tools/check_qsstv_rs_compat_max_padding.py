#!/usr/bin/env python3
"""Check maximum JPEG padding controls against pinned unmodified QSSTV.

Build export_hamdrm_rs_boundary_wave first. This uses synthesised waveforms;
separate application/audio tests exercise the production option itself.
"""
import hashlib
import io
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageChops

repo = Path(__file__).resolve().parents[3]
fixtures = repo / 'tests/sstv/fixtures/easypal-2014-live'
exporter = (repo / 'build/tests/sstv/export_hamdrm_rs_boundary_wave').resolve(strict=True)
qsstv = Path(os.environ.get('QSSTV_EXPERIMENTAL_BINARY',
    '/tmp/decodium-qsstv-experimental/QSSTV-Experimental.app/Contents/MacOS/qsstv-experimental'))
if hashlib.sha256(qsstv.read_bytes()).hexdigest() != 'b2de73b06d2308e8363864555336f70eb0ba7603e7d3734c091da3da1a0cf1fa':
    raise SystemExit('QSSTV executable does not match the pinned release')
with tempfile.TemporaryDirectory(prefix='decodium-qsstv-rs-max-padding-') as tmp:
    root = Path(tmp)
    for level in (1, 2, 3, 4):
        k = 256 - 32 * level
        for variant in 'ab':
            original = (fixtures / ('reverse-' + variant + '.jpg')).read_bytes()
            source = original + bytes((-len(original)) % k)
            for mode in ('standard', 'compatible'):
                case = root / f'rs{level}-{variant}-{mode}'
                case.mkdir()
                payload = source + (bytes(k - 127) if mode == 'compatible' else b'')
                image, raw, wav = case / 'input.jpg', case / 'audio.s16', case / 'audio.wav'
                image.write_bytes(payload)
                subprocess.run([str(exporter), str(level), str(image), str(raw)], check=True, timeout=90)
                subprocess.run(['ffmpeg', '-v', 'error', '-f', 's16le', '-ar', '48000', '-ac', '1',
                                '-i', str(raw), '-ac', '2', str(wav)], check=True, timeout=30)
                for attempt in range(3):
                    result = subprocess.run([str(qsstv), '--batch', '--drm', '--out-dir', str(case), str(wav)],
                                            env={**os.environ, 'QT_QPA_PLATFORM': 'cocoa'},
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
                    decoded = list(case.glob('*_drm.jpg'))
                    if result.returncode == 0 or decoded:
                        break
                expected = payload + (bytes(k - 7) if mode == 'standard' else b'')
                if result.returncode or len(decoded) != 1 or decoded[0].read_bytes() != expected:
                    raise RuntimeError(f'RS{level} {variant} {mode}: unexpected output')
                with Image.open(io.BytesIO(original)) as first, Image.open(decoded[0]) as second:
                    if ImageChops.difference(first.convert('RGB'), second.convert('RGB')).getbbox() is not None:
                        raise RuntimeError(f'RS{level} {variant} {mode}: pixels differ')
                print(f'RS{level} {variant} {mode} added={len(payload)-len(source)} '
                      f'receiver_extra={len(expected)-len(payload)} pixel_match=True '
                      f'attempts={attempt+1}', flush=True)

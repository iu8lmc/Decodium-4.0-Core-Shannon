#!/usr/bin/env python3
"""Replay actual Decodium compatibility-mode RS captures to pinned QSSTV."""
import hashlib
import io
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageChops

fixtures = Path(__file__).resolve().parents[1] / 'fixtures' / 'easypal-2014-live'
qsstv = Path(os.environ.get('QSSTV_EXPERIMENTAL_BINARY',
    '/tmp/decodium-qsstv-experimental/QSSTV-Experimental.app/Contents/MacOS/qsstv-experimental'))
if hashlib.sha256(qsstv.read_bytes()).hexdigest() != 'b2de73b06d2308e8363864555336f70eb0ba7603e7d3734c091da3da1a0cf1fa':
    raise SystemExit('QSSTV executable does not match the pinned release')
with tempfile.TemporaryDirectory(prefix='decodium-qsstv-rs-compat-') as tmp:
    root = Path(tmp)
    for level in (1, 2, 3, 4):
        data_size = 256 - 32 * level
        for variant in 'ab':
            original = (fixtures / ('reverse-' + variant + '.jpg')).read_bytes()
            # Source was adjusted to RS padding field 128; the opted-in
            # transmitter then appended one more zero to make the field 127.
            source_padding = (data_size - 128 - len(original) % data_size) % data_size
            expected = original + bytes(source_padding + 1)
            case = root / f'rs{level}-{variant}'
            case.mkdir()
            wav = case / 'capture.wav'
            recording = fixtures / f'decodium-rs{level}-compat-{variant}.flac'
            subprocess.run(['ffmpeg', '-v', 'error', '-i', str(recording),
                            '-c:a', 'pcm_s16le', str(wav)], check=True, timeout=30)
            for attempt in range(3):
                result = subprocess.run([str(qsstv), '--batch', '--drm', '--out-dir', str(case), str(wav)],
                                        env={**os.environ, 'QT_QPA_PLATFORM': 'cocoa'},
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
                images = list(case.glob('capture_*_drm.jpg'))
                if result.returncode == 0 or images:
                    break
            if result.returncode or len(images) != 1 or images[0].read_bytes() != expected:
                raise RuntimeError(f'QSSTV output differs from transmitted payload: RS{level} {variant}')
            with Image.open(io.BytesIO(original)) as first, Image.open(images[0]) as second:
                if ImageChops.difference(first.convert('RGB'), second.convert('RGB')).getbbox() is not None:
                    raise RuntimeError(f'image pixels differ: RS{level} {variant}')
            print(f'RS{level} {variant} exact_transmitted_bytes={len(expected)} '
                  f'added_by_compatibility=1 pixel_match=True acquisition_attempts={attempt + 1}', flush=True)

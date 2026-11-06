#!/usr/bin/env python3
"""Reproduce a known QSSTV 10.0 RS2 padding discrepancy, not a compatibility pass."""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1] / 'fixtures' / 'easypal-2014-live'
qsstv = Path(os.environ.get('QSSTV_EXPERIMENTAL_BINARY',
    '/tmp/decodium-qsstv-experimental/QSSTV-Experimental.app/Contents/MacOS/qsstv-experimental'))
if hashlib.sha256(qsstv.read_bytes()).hexdigest() != 'b2de73b06d2308e8363864555336f70eb0ba7603e7d3734c091da3da1a0cf1fa':
    raise SystemExit('QSSTV executable does not match the pinned release')
with tempfile.TemporaryDirectory(prefix='decodium-qsstv-rs2-padding-') as tmp:
    directory = Path(tmp)
    for variant, reference, source_padding, extra_zeros in [
            ('a', 'a', 0, 0), ('b', 'b', 0, 125),
            ('sign127', 'b', 5, 0), ('sign128', 'b', 4, 121)]:
        wav = directory / (variant + '.wav')
        subprocess.run(['ffmpeg', '-v', 'error', '-i', str(root / ('decodium-rs2-' + variant + '.flac')),
                        '-c:a', 'pcm_s16le', str(wav)], check=True, timeout=30)
        # The batch receiver sometimes fails to acquire this same recording.
        # Retry only no-image acquisition; never retry an incorrect output.
        for attempt in range(3):
            result = subprocess.run([str(qsstv), '--batch', '--drm', '--out-dir', str(directory), str(wav)],
                                    env={**os.environ, 'QT_QPA_PLATFORM': 'cocoa'},
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
            files = list(directory.glob(variant + '_*_drm.jpg'))
            if result.returncode == 0 or files:
                break
        if result.returncode != 0 or len(files) != 1:
            raise RuntimeError('QSSTV did not decode case ' + variant)
        original = (root / ('reverse-' + reference + '.jpg')).read_bytes() + bytes(source_padding)
        expected = original + bytes(extra_zeros)
        if files[0].read_bytes() != expected:
            raise RuntimeError('QSSTV result changed for case ' + variant)
        print('QSSTV', variant, 'exact=', extra_zeros == 0, 'trailing_zero_bytes=', extra_zeros,
              'acquisition_attempts=', attempt + 1)
print('Observed known padding discrepancy; non-exact cases are not interoperability passes.')

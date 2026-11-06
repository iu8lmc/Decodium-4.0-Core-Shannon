#!/usr/bin/env python3
"""Replay two 12 kHz PCM controls in the pinned QSSTV DRM batch receiver."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('pcm_a', type=Path)
parser.add_argument('pcm_b', type=Path)
parser.add_argument('reference_a', type=Path)
parser.add_argument('reference_b', type=Path)
args = parser.parse_args()
binary = Path(os.environ.get('QSSTV_EXPERIMENTAL_BINARY',
    '/tmp/decodium-qsstv-experimental/QSSTV-Experimental.app/Contents/MacOS/qsstv-experimental')).resolve(strict=True)
if hashlib.sha256(binary.read_bytes()).hexdigest() != 'b2de73b06d2308e8363864555336f70eb0ba7603e7d3734c091da3da1a0cf1fa':
    raise SystemExit('unexpected QSSTV receiver binary')
cases = [(args.pcm_a.resolve(strict=True), args.reference_a.resolve(strict=True)),
         (args.pcm_b.resolve(strict=True), args.reference_b.resolve(strict=True))]
if cases[0][0] == cases[1][0] or cases[0][1].read_bytes() == cases[1][1].read_bytes():
    raise SystemExit('A/B inputs or references are not distinct')
root = Path(tempfile.mkdtemp(prefix='decodium-qsstv-allpass-ab-'))
for label, (pcm, reference) in zip('ab', cases):
    if pcm.stat().st_size < 2 or pcm.stat().st_size % 2:
        raise RuntimeError('invalid s16 PCM input')
    directory = root / label
    directory.mkdir()
    wav = directory / 'capture.wav'
    subprocess.run(['ffmpeg', '-v', 'error', '-f', 's16le', '-ar', '12000', '-ac', '1',
                    '-i', str(pcm), '-ar', '48000', '-ac', '2', str(wav)], check=True, timeout=30)
    env = os.environ.copy()
    env['QT_QPA_PLATFORM'] = 'cocoa'
    with (directory / 'qsstv.log').open('wb') as output:
        result = subprocess.run([str(binary), '--batch', '--drm', '--out-dir', str(directory), str(wav)],
                                env=env, stdout=output, stderr=subprocess.STDOUT, timeout=90)
    images = list(directory.glob('capture_*_drm.*'))
    if result.returncode != 0 or len(images) != 1 or images[0].read_bytes() != reference.read_bytes():
        raise RuntimeError(f'{label}: QSSTV did not decode the exact reference; artifacts {directory}')
    print(f'PASS {label}: exact {reference.stat().st_size}-byte object; artifacts {directory}', flush=True)

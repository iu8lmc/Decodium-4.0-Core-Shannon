#!/usr/bin/env python3
"""Check retained EasyPal 2014 live-exchange evidence, including the captured audio.

Requires ffmpeg and a built analyze_hamdrm_capture. See the fixture README for
provenance, the operator-observed RX, and limits of this recorded regression.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--analyzer', type=Path, default=Path('build/tests/sstv/analyze_hamdrm_capture'))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1] / 'fixtures' / 'easypal-2014-live'
analyzer = args.analyzer.resolve(strict=True)
expected_audio = 'f00001ada624839ee6b3f66600b13c1a570e26afd2b88c6942e69ab11a970ec6'
if hashlib.sha256((root / 'easypal-b.flac').read_bytes()).hexdigest() != expected_audio:
    raise SystemExit('unexpected EasyPal B capture hash')

def pixels(path):
    return subprocess.run(['ffmpeg', '-v', 'error', '-i', str(path), '-frames:v', '1',
                           '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'],
                          capture_output=True, check=True, timeout=30).stdout

def compare(received, reference, limit):
    a, b = pixels(root / received), pixels(root / reference)
    if len(a) != 320 * 240 * 3 or len(a) != len(b):
        raise RuntimeError('unexpected decoded image dimensions')
    mae = sum(abs(x - y) for x, y in zip(a, b)) / len(a)
    if mae > limit:
        raise RuntimeError('%s differs from %s: MAE %.6f' % (received, reference, mae))
    return mae

for variant in 'ab':
    input_name = 'decodium-test-24bit-' + variant + '.bmp'
    actual_name = 'received-' + variant + '.jp2'
    print('EasyPal to Decodium', variant, 'MAE', compare(actual_name, input_name, 0.001))
    source_name = 'reverse-' + variant + '.jpg'
    output_name = 'received-easypal-reverse-' + variant + '.jpg'
    print('Decodium to EasyPal', variant, 'MAE', compare(output_name, source_name, 2.1))
    other = 'b' if variant == 'a' else 'a'
    a, b = pixels(root / output_name), pixels(root / ('reverse-' + other + '.jpg'))
    if sum(abs(x - y) for x, y in zip(a, b)) / len(a) < 100:
        raise RuntimeError('crossed reverse reference did not differ')
with tempfile.TemporaryDirectory(prefix='decodium-easypal-live-fixture-') as temp:
    pcm = Path(temp) / 'capture.s16'
    subprocess.run(['ffmpeg', '-v', 'error', '-i', str(root / 'easypal-b.flac'), '-ac', '1',
                    '-ar', '12000', '-f', 's16le', str(pcm)], check=True, timeout=30)
    cmd = [str(analyzer), str(pcm), 'B', '2500', '16', '0.12', '0.30',
           str(root / 'received-b.jp2'), 'long', 'flat-multistage', 'high']
    result = subprocess.run(cmd, text=True, capture_output=True, timeout=60)
    if result.returncode or 'exact=1' not in result.stdout or 'ingest_errors=0' not in result.stdout:
        raise RuntimeError('EasyPal audio did not recover expected object:\n' + result.stdout + result.stderr)
    cmd[7] = str(root / 'received-a.jp2')
    crossed = subprocess.run(cmd, text=True, capture_output=True, timeout=60)
    if crossed.returncode != 1 or 'exact=0' not in crossed.stdout:
        raise RuntimeError('EasyPal audio crossed-reference control failed:\n' + crossed.stdout + crossed.stderr)
    print('EasyPal B audio: exact object and crossed-reference negative passed')

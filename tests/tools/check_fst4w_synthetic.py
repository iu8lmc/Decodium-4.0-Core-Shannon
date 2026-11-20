#!/usr/bin/env python3
"""Run a synthetic FST4W PCM fixture and require its expected message."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import wave

parser = argparse.ArgumentParser()
parser.add_argument('--wav', type=Path, required=True)
parser.add_argument('--binary', type=Path, required=True)
parser.add_argument('--expected', required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
with wave.open(str(args.wav), 'rb') as wav:
    if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getnframes()) != (1, 2, 12000, 120 * 12000):
        raise ValueError('expected 120 seconds of mono 12 kHz 16-bit PCM')
    data = wav.readframes(wav.getnframes())
with tempfile.TemporaryDirectory(prefix='decodium-fst4w-check-') as tmp:
    pcm = Path(tmp) / 'input.pcm'
    pcm.write_bytes(data)
    result = subprocess.run([str(args.binary.resolve()), str(pcm), '120', '--fst4w'],
                            cwd=tmp, capture_output=True, text=True, timeout=180)
    result.check_returncode()
    measurements = json.loads(result.stdout)
    if len(measurements.get('runs', [])) != 3:
        raise RuntimeError('expected three measurement runs')
    for run in measurements['runs']:
        run['expectedPresent'] = any(args.expected in row for row in run['rows'])
    measurements['expected'] = args.expected
    measurements['wavSha256'] = hashlib.sha256(args.wav.read_bytes()).hexdigest()
    args.output.write_text(json.dumps(measurements, indent=2) + '\n')
    if not all(run['expectedPresent'] for run in measurements['runs']):
        raise SystemExit('expected message missing; inspect ' + str(args.output))
    print('expected message found in all three runs')

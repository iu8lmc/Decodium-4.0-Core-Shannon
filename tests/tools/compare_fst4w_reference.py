#!/usr/bin/env python3
"""Compare offline FST4W native worker and WSJT-X on a supplied WAV."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import tempfile
import time
import wave

parser = argparse.ArgumentParser()
parser.add_argument('--wav', type=Path, required=True)
parser.add_argument('--native', type=Path, required=True)
parser.add_argument('--reference', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--expected', default='K1ABC FN42 33')
args = parser.parse_args()
wav, native, reference = args.wav.resolve(), args.native.resolve(), args.reference.resolve()
with wave.open(str(wav), 'rb') as stream:
    if (stream.getnchannels(), stream.getsampwidth(), stream.getframerate(), stream.getnframes()) != (1, 2, 12000, 120*12000):
        raise ValueError('expected 120 seconds of mono 12 kHz 16-bit PCM')
    pcm = stream.readframes(stream.getnframes())

def run(command, cwd):
    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    start = time.monotonic()
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            env={**os.environ, 'OMP_NUM_THREADS': '2'}, timeout=180)
    wall = time.monotonic() - start
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    result.check_returncode()
    return result.stdout, wall, ((after.ru_utime + after.ru_stime)
                                - (before.ru_utime + before.ru_stime))

with tempfile.TemporaryDirectory(prefix='fst4w-reference-') as tmp:
    directory = Path(tmp)
    pcm_path = directory / 'input.pcm'
    pcm_path.write_bytes(pcm)
    native_out, native_wall, native_cpu = run([str(native), str(pcm_path), '120', '--fst4w'], directory)
    native_data = json.loads(native_out)
    references = []
    for i in range(3):
        subdir = directory / ('ref' + str(i))
        subdir.mkdir()
        output, wall, cpu = run([str(reference), '-W', '-p', '120', '-d', '3', '-f', '1500',
                                 '-F', '20', '-c', 'K1ABC', '-x', 'W9XYZ',
                                 '-a', str(subdir), '-t', str(subdir), str(wav)], subdir)
        rows = [line for line in output.splitlines() if ' ` ' in line]
        references.append({'wallSeconds': wall, 'cpuSeconds': cpu, 'rows': rows})
    report = {'wavSha256': hashlib.sha256(wav.read_bytes()).hexdigest(),
              'nativeSha256': hashlib.sha256(native.read_bytes()).hexdigest(),
              'referenceSha256': hashlib.sha256(reference.read_bytes()).hexdigest(),
              'expected': args.expected, 'native': native_data['runs'],
              'nativeProcessWallSeconds': native_wall, 'nativeProcessCpuSeconds': native_cpu,
              'reference': references}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    for entry in report['native'] + references:
        if not any(args.expected in row for row in entry['rows']):
            raise SystemExit('expected message missing; inspect ' + str(args.output))
    print(json.dumps({'nativeRows': [len(r['rows']) for r in report['native']],
                      'nativeElapsedMs': [r['elapsedMs'] for r in report['native']],
                      'referenceRows': [len(r['rows']) for r in references],
                      'referenceWallSeconds': [round(r['wallSeconds'], 3) for r in references],
                      'referenceCpuSeconds': [round(r['cpuSeconds'], 3) for r in references]}))

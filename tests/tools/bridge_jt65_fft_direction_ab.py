#!/usr/bin/env python3
"""Compare old/corrected JT65 filter directions on the verified reference WAV.

The nested lab also compares the two Bridge placements; CAT is disabled. This
script temporarily changes source and rebuilds. Do not run concurrently with
other builds or while editing the instrumented files.
"""
import json
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Detector/LegacyDspIoHelpers.cpp'
original = SOURCE.read_bytes()
correct = b'transform_complex (c4a, kFilbigNfft2, -1);'
previous = b'transform_complex (c4a, kFilbigNfft2, 1);'
if original.count(correct) != 1 or previous in original:
    raise SystemExit('unexpected JT65 filter source; refusing to modify it')
env = os.environ.copy()
results = {}
try:
    for label, content in [('previous', original.replace(correct, previous)), ('corrected', original)]:
        SOURCE.write_bytes(content)
        run = subprocess.run(['python3', 'tests/tools/bridge_feeddecoder_jt65_reference_ab.py'],
                             cwd=ROOT, env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True, timeout=240)
        matches = re.findall(r'^(before|after) (\{"displayedExpected"[^\n]+)', run.stdout, re.M)
        if len(matches) != 2 or {name for name, _ in matches} != {'before', 'after'}:
            raise RuntimeError(label + ' missing app results: ' + run.stdout[-5000:])
        rows = {name: json.loads(value) for name, value in matches}
        expected = label == 'corrected'
        if any(row != {'displayedExpected': expected, 'expectedMessage': expected}
               for row in rows.values()) or (run.returncode == 0) != expected:
            raise RuntimeError(label + ' unexpected result: ' + str(rows) + '\n' + run.stdout[-5000:])
        results[label] = {'exit': run.returncode, 'bridge': rows}
        print(label, json.dumps(results[label], sort_keys=True), flush=True)
finally:
    SOURCE.write_bytes(original)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'],
                   cwd=ROOT, check=True)

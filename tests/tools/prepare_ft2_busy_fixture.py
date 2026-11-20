#!/usr/bin/env python3
"""Freeze an eight-station FT2 scene using the production TX fixture generator."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import wave
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--generator', type=Path, default=Path('build/tests/ft2_make_test_wav'))
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
stations = list(zip(('K1ABC', 'W2XYZ', 'N3ABC', 'K4ABC', 'W5ABC', 'N6ABC', 'K7ABC', 'W8ABC'),
                    (1500, 400, 700, 1000, 1800, 2100, 2400, 2800)))
scene = np.random.default_rng(20261114).normal(0, .03, 45000)
messages = []
for call, frequency in stations:
    message = f'CQ {call} FN42'
    target = a.output / f'{call}.wav'
    subprocess.run([str(a.generator), '--message', message, '--freq', str(frequency),
                    '--gain', '.025', str(target)], check=True, stdout=subprocess.DEVNULL)
    with wave.open(str(target)) as w:
        assert w.getnchannels() == 1 and w.getsampwidth() == 2 and w.getframerate() == 12000
        samples = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2')
    assert len(samples) == len(scene)
    scene += samples.astype(float) / 32767
    messages.append(message)
assert np.max(np.abs(scene)) < 1, 'Refuse clipped corpus'
output = a.output / 'busy-ft2.wav'
with wave.open(str(output), 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(12000)
    w.writeframes(np.rint(scene * 32767).astype('<i2').tobytes())
manifest = {'file': output.name, 'sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
            'generatorSha256': hashlib.sha256(a.generator.read_bytes()).hexdigest(),
            'messages': messages, 'frequencies': [f for _, f in stations],
            'seed': 20261114, 'noiseRms': .03, 'signalGain': .025,
            'peakAbs': float(np.max(np.abs(scene))), 'frames': len(scene),
            'scope': 'Frozen synthetic separated signals; not calibrated RF sensitivity'}
(a.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps(manifest))

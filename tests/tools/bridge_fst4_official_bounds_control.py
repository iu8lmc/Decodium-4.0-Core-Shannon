#!/usr/bin/env python3
"""Compare FST4 clean-signal frequency bounds in a supplied WSJT-X jt9 binary."""
import argparse
import hashlib
import json
import subprocess
import tempfile
import wave
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--jt9', type=Path, required=True)
parser.add_argument('--wav', type=Path, required=True)
args = parser.parse_args()
with wave.open(str(args.wav), 'rb') as source:
    if (source.getnchannels(), source.getsampwidth(), source.getframerate(), source.getnframes()) != (1, 2, 12000, 180000):
        raise SystemExit('unexpected WAV format')
    digest = hashlib.sha256(source.readframes(source.getnframes())).hexdigest()
if digest != '5eeb50dd0c35ed35c2c3df42d719b8478d02051a082259507c80fea5af6608d4':
    raise SystemExit('unexpected PCM fixture hash')
with tempfile.TemporaryDirectory(prefix='decodium-fst4-official-') as temp:
    results = []
    for upper in (1520, 1530, 1550):
        for tolerance in (20, 150):
            command = [str(args.jt9.resolve()), '-7', '-p', '15', '-d', '2', '-f', '1500',
                       '-F', str(tolerance), '-L', '1480', '-H', str(upper),
                       '-a', temp, '-t', temp, str(args.wav.resolve())]
            run = subprocess.run(command, cwd=temp, text=True, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, timeout=30)
            if run.returncode or '<DecodeFinished>' not in run.stdout:
                raise RuntimeError(run.stdout)
            decoded = 'CQ K1ABC FN42' in run.stdout
            results.append({'high': upper, 'tolerance': tolerance, 'decoded': decoded})
            if decoded != (upper != 1530):
                raise RuntimeError('control outcome changed: ' + run.stdout)
    print(json.dumps({'pcm_sha256': digest, 'cases': results}, sort_keys=True))

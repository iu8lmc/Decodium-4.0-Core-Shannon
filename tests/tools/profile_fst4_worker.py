#!/usr/bin/env python3
"""Measure an existing native FST4 probe using a frozen 60-second recording.

No downloads, builds, GUI, radio or network. Keeps complete rows (including
possible false positives); expected-message presence is not sensitivity proof.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import wave


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wav', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--binary', type=Path, default=Path('build/tests/fst4_performance_probe'))
    args = parser.parse_args()
    binary, wav, out = args.binary.resolve(), args.wav.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    with wave.open(str(wav)) as stream:
        if (stream.getnchannels(), stream.getsampwidth(), stream.getframerate(), stream.getnframes()) != (1, 2, 12000, 720000):
            raise ValueError('Expected exactly 60 seconds of mono 12 kHz 16-bit PCM')
        pcm = stream.readframes(stream.getnframes())
    pcm_path = out / 'input.pcm'
    pcm_path.write_bytes(pcm)
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('DECODIUM_', 'OMP_'))}
    env['OMP_NUM_THREADS'] = '2'
    with (out / 'stderr.log').open('w') as errors:
        result = subprocess.run([str(binary), str(pcm_path), '60'], cwd=out,
                                env=env, capture_output=False, stdout=subprocess.PIPE,
                                stderr=errors, text=True, timeout=180)
    (out / 'result.json').write_text(result.stdout)
    record = {'exitCode': result.returncode,
              'wavSha256': hashlib.sha256(wav.read_bytes()).hexdigest(),
              'binarySha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
              'OMP_NUM_THREADS': 2}
    (out / 'manifest.json').write_text(json.dumps(record, indent=2) + '\n')
    result.check_returncode()
    runs = json.loads(result.stdout)['runs']
    assert len(runs) == 3
    assert all(run['rows'] == runs[0]['rows'] for run in runs)
    assert all(any('CQ K9KFR EN71' in row for row in run['rows']) for run in runs)
    print(json.dumps({'repeatRowsEqual': True, 'expectedMessagePresent': True,
                      'elapsedMs': [run['elapsedMs'] for run in runs]}, indent=2))


if __name__ == '__main__':
    main()

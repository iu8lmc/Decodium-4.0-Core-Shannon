#!/usr/bin/env python3
"""Generate and freeze multistation FT8 fixtures for paired app-level tests.

ft8sim output is not assumed deterministic. Generate once, retain hashes and feed
the identical WAVs to both builds. Nominal SNR levels are synthetic targets, not
an independently calibrated receiver sensitivity measurement.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import wave

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
MESSAGES = ['CQ K1ABC FN42', 'CQ W1AW FN31', 'CQ G4ABC IO91', 'CQ DL1ABC JO62',
            'CQ JA1ABC PM95', 'CQ VK2ABC QF56', 'CQ I2ABC JN45', 'CQ EA3ABC JN11',
            'CQ F5ABC JN18', 'CQ OH2ABC KP20', 'CQ VE3ABC FN03', 'CQ PY2ABC GG66']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--simulator', type=Path, default=ROOT / 'build/ft8sim')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    simulator = args.simulator.resolve(strict=True)
    args.output.mkdir(parents=True, exist_ok=False)
    signals = []
    frequencies = [400 + 250 * i for i in range(len(MESSAGES))]
    for message, frequency in zip(MESSAGES, frequencies):
        with tempfile.TemporaryDirectory(prefix='decodium-ft8-fixture-') as directory:
            result = subprocess.run([str(simulator), message, str(frequency), '0.2', '0', '0', '1', '40'],
                                    cwd=directory, capture_output=True, text=True, timeout=30, check=True)
            paths = list(Path(directory).glob('*.wav'))
            if len(paths) != 1:
                raise RuntimeError('Expected one ft8sim WAV: ' + result.stdout)
            with wave.open(str(paths[0]), 'rb') as wav:
                assert wav.getframerate() == 12000 and wav.getnchannels() == 1 and wav.getsampwidth() == 2
                samples = np.frombuffer(wav.readframes(wav.getnframes()), dtype='<i2').astype(np.float64)
            samples = np.pad(samples[:180000], (0, max(0, 180000 - len(samples))))
            rms = np.sqrt(np.mean(samples ** 2))
            assert rms > 0
            signals.append(samples / rms)
    levels = {'busy': [-5, -6, -7, -8, -9, -10, -11, -12, -7, -8, -9, -10],
              'mixed_weak': [-5, -8, -11, -14, -17, -20, -22, -24, 0, 3, -10, -18]}
    manifest = {'simulator_sha256': hashlib.sha256(simulator.read_bytes()).hexdigest(),
                'messages': MESSAGES, 'frequencies_hz': frequencies, 'files': [],
                'caveat': 'Nominal synthetic SNR targets; freeze these exact WAVs for A/B; not calibrated RF sensitivity'}
    for scene, snrs in levels.items():
        for seed in (901, 902):
            rng = np.random.default_rng(seed)
            mixed = rng.normal(0, 1, 180000)
            for signal, snr in zip(signals, snrs):
                mixed += signal * (10 ** (snr / 20)) / np.sqrt(6000 / 2500)
            pcm = np.round(mixed / max(1.0, np.max(np.abs(mixed))) * 28000).astype('<i2')
            path = args.output / f'{scene}-{seed}.wav'
            with wave.open(str(path), 'wb') as wav:
                wav.setnchannels(1)
                wav.setsampwidth(2)
                wav.setframerate(12000)
                wav.writeframes(pcm.tobytes())
            manifest['files'].append({'name': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                                      'seed': seed, 'nominal_snr_db': snrs})
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()

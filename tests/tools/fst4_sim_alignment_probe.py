#!/usr/bin/env python3
"""Verify fst4sim DT shifts clean PCM later, for FST4 and FST4W."""
import argparse
from array import array
import math
from pathlib import Path
import subprocess
import tempfile
import wave

parser = argparse.ArgumentParser()
parser.add_argument('--binary', type=Path, required=True)
args = parser.parse_args()
binary = args.binary.resolve()
for period, message, hint in [(15, 'CQ K1ABC FN42', 'F'), (120, 'K1ABC FN42 33', 'T')]:
    with tempfile.TemporaryDirectory(prefix='fst4sim-alignment-') as tmp:
        samples = []
        for dt in ('-0.5', '0', '0.5'):
            result = subprocess.run([str(binary), message, str(period), '1500', dt,
                                     '0', '0', '1', '99', hint], cwd=tmp,
                                    capture_output=True, text=True, check=True)
            paths = list(Path(tmp).glob('*.wav'))
            if len(paths) != 1:
                raise RuntimeError('unexpected simulator outputs: ' + result.stdout)
            with wave.open(str(paths[0]), 'rb') as wav:
                if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) != (1, 2, 12000):
                    raise RuntimeError('unexpected WAV format')
                pcm = array('h')
                pcm.frombytes(wav.readframes(wav.getnframes()))
                samples.append(pcm)
            paths[0].unlink()
        for left, right in zip(samples, samples[1:]):
            xy = xx = yy = 0
            for i in range(36000, 120000, 17):
                x, y = left[i], right[i + 6000]
                xy += x*y
                xx += x*x
                yy += y*y
            correlation = xy / math.sqrt(xx*yy) if xx and yy else 0
            if correlation < 0.999:
                raise RuntimeError(f'{period}s DT alignment failed: {correlation:.6f}')
        print(f'{period}s {hint}: -0.5s, 0s and +0.5s shifts aligned')

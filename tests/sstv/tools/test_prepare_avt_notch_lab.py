#!/usr/bin/env python3
"""Verify both channels and gating of the offline notch preparation."""
import array
import math
from pathlib import Path
import subprocess
import sys
import tempfile

script = Path(__file__).with_name('prepare_avt_notch_lab.py')
with tempfile.TemporaryDirectory(prefix='decodium-notch-test-') as directory:
    root = Path(directory)
    silence = root / 'silence.f32'
    silence.write_bytes(bytes(3 * 48000 * 8))
    for frequency, quality in ((1700, 100), (1900, 100), (1700, 300)):
        output = root / f'{frequency}-{quality}.f32'
        subprocess.run([sys.executable, str(script), str(silence), str(output),
                        '--frequency', str(frequency), '--gate-seconds', '1',
                        '--tone-peak', '0.1', '--quality', str(quality)], check=True, timeout=30)
        samples = array.array('f'); samples.frombytes(output.read_bytes())
        if sys.byteorder != 'little': samples.byteswap()
        for channel in (0, 1):
            def rms(start, end):
                window = samples[2 * start + channel:2 * end:2]
                return math.sqrt(sum(x*x for x in window) / len(window))
            before = rms(12000, 36000)
            after = rms(96000, 120000)
            if not 0.05 < before < 0.065 or after >= before * 0.01:
                raise RuntimeError(f'notch gating or channel mismatch: {frequency} {channel} {before} {after}')
        # A second invocation must not overwrite an existing capture.
        result = subprocess.run([sys.executable, str(script), str(silence), str(output),
                                 '--frequency', str(frequency), '--gate-seconds', '1'],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
        if result.returncode == 0: raise RuntimeError('existing output was overwritten')
        print('PASS gated stereo notch', frequency)

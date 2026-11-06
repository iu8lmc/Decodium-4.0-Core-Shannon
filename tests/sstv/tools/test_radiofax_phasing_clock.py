#!/usr/bin/env python3
"""Check phasing clock measurements with two known synthetic clocks."""
from array import array
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

analyzer = Path(__file__).with_name('analyze_radiofax_phasing_clock.py')
with tempfile.TemporaryDirectory(prefix='decodium-fax-clock-') as directory:
    capture = Path(directory) / 'capture.f32'
    for period in (.5, .49990):
        phase = 0.0
        values = array('f')
        for n in range(48000 * 6):
            within = (n / 48000) % period
            frequency = 2300 if within < .05 * period else 1500
            phase += 2 * math.pi * frequency / 48000
            sample = .5 * math.sin(phase)
            values.extend((sample, sample))
        if sys.byteorder != 'little':
            values.byteswap()
        capture.write_bytes(values.tobytes())
        cmd = [sys.executable, str(analyzer), str(capture), '--window', '0', '6']
        result = json.loads(subprocess.check_output(cmd))
        assert abs(result['line_period_s'] - period) < .00003, result
        print('PASS phasing clock', period)
    bad = subprocess.run(cmd[:-2] + ['0', '1'], stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL)
    assert bad.returncode != 0, 'insufficient phasing edges must fail'

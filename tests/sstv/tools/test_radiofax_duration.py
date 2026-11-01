#!/usr/bin/env python3
"""Exercise measured transition intervals against two known PCM timelines."""
from array import array
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

analyzer = Path(__file__).with_name('analyze_radiofax_duration.py')
with tempfile.TemporaryDirectory(prefix='decodium-fax-duration-') as directory:
    capture = Path(directory) / 'capture.f32'
    for start, end in ((.25, .75), (.21, .691)):
        phase = 0.0
        values = array('f')
        for n in range(48000):
            now = n / 48000
            frequency = 2300 if now < start else (1500 if now < end else 450)
            phase += 2 * math.pi * frequency / 48000
            sample = .5 * math.sin(phase)
            values.extend((sample, sample))
        if sys.byteorder != 'little':
            values.byteswap()
        capture.write_bytes(values.tobytes())
        cmd = [sys.executable, str(analyzer), str(capture), '--start-window',
               str(start - .05), str(start + .05), '--end-window',
               str(end - .05), str(end + .05)]
        result = json.loads(subprocess.check_output(cmd))
        assert abs(result['duration_estimate_s'] - (end - start)) < .001, result
        assert result['duration_min_s'] <= end - start <= result['duration_max_s'], result
        print('PASS known duration', end - start)
    bad = subprocess.run(cmd[:-2] + ['0.8', '0.9'], stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL)
    assert bad.returncode != 0, 'missing transition must fail'

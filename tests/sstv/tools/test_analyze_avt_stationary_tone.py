#!/usr/bin/env python3
"""A/B controls and a deliberate ambiguity for the offline AVT survey."""
from array import array
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

analyzer = Path(__file__).with_name('analyze_avt_stationary_tone.py')
with tempfile.TemporaryDirectory(prefix='decodium-avt-tone-') as directory:
    capture = Path(directory) / 'signal.f32'
    for frequency in (1700, 1900):
        values = array('f')
        for index in range(96000):
            sample = 0.15 * math.sin(2 * math.pi * frequency * index / 48000)
            values.extend((sample, sample))
        if sys.byteorder != 'little': values.byteswap()
        capture.write_bytes(values.tobytes())
        result = json.loads(subprocess.check_output([sys.executable, str(analyzer), str(capture), '--start', '0', '--seconds', '2']))
        assert result['persistent'][0]['frequency_hz'] == frequency, result
        assert abs(result['persistent'][0]['minimum_amplitude'] - 0.15) < 0.001, result
        print('PASS persistent tone', frequency)
    result = json.loads(subprocess.check_output([sys.executable, str(analyzer), str(capture), '--start', '0', '--seconds', '2', '--tone-hz', '1700', '--tone-peak', '0.1']))
    assert result['persistent'][0]['frequency_hz'] == 1900, result
    assert result['persistent'][1]['frequency_hz'] == 1700, result
    bad = subprocess.run([sys.executable, str(analyzer), str(capture), '--start', '1', '--seconds', '2'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    assert bad.returncode != 0
    print('PASS stronger legitimate tone ambiguity and short-input rejection')

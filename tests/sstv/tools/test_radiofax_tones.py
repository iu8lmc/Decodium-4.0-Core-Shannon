#!/usr/bin/env python3
"""Check the diagnostic tone measurements on two known signals."""
from array import array
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

analyzer = Path(__file__).with_name('analyze_radiofax_tones.py')
with tempfile.TemporaryDirectory(prefix='decodium-fax-tone-') as directory:
    capture = Path(directory) / 'capture.f32'
    for frequency in (450, 1900):
        values = array('f')
        for n in range(48000):
            sample = .5 * math.sin(2 * math.pi * frequency * n / 48000)
            values.extend((sample, sample))
        if sys.byteorder != 'little': values.byteswap()
        capture.write_bytes(values.tobytes())
        result = json.loads(subprocess.check_output([sys.executable, str(analyzer), str(capture), '--start', '0']))
        amplitudes = result['coherent_amplitude']
        assert abs(amplitudes[str(frequency)] - .25) < .001, result
        assert max(value for key, value in amplitudes.items() if key != str(frequency)) < .001, result
        print('PASS diagnostic tone', frequency, 'Hz')

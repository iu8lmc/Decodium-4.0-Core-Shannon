#!/usr/bin/env python3
"""A/B controls for the captured Radiofax phasing impairment helper."""
from array import array
import math
from pathlib import Path
import re
import subprocess
import sys
import tempfile

tool = Path(__file__).with_name('prepare_radiofax_phasing_glitch.py')
with tempfile.TemporaryDirectory(prefix='decodium-radiofax-glitch-test-') as directory:
    root = Path(directory)
    pattern = array('f')
    for i in range(32):
        value = 0.35 * math.sin(2 * math.pi * 1500 * i / 48000)
        pattern.extend((value, value))
    original = pattern.tobytes() * (45 * 48000 // 32)
    source = root / 'source.f32'
    source.write_bytes(original)
    for variant in ('a', 'b'):
        output = root / (variant + '.f32')
        result = subprocess.run([sys.executable, str(tool), str(source), str(output), variant],
                                check=True, capture_output=True, text=True)
        start = int(re.search(r'pulse_start=(\d+)', result.stdout).group(1))
        altered = output.read_bytes()
        left, right = start * 8, (start + 600) * 8
        assert len(altered) == len(original)
        assert altered[:left] == original[:left] and altered[right:] == original[right:]
        assert altered[left:right] != original[left:right]
        assert subprocess.run([sys.executable, str(tool), str(source), str(output), variant],
                              capture_output=True).returncode != 0
        double = root / (variant + '-double.f32')
        result = subprocess.run([sys.executable, str(tool), str(source), str(double), variant, '--two-pulses'],
                                check=True, capture_output=True, text=True)
        start = int(re.search(r'pulse_start=(\d+)', result.stdout).group(1))
        assert 'pulse_count=2' in result.stdout
        altered = double.read_bytes()
        left, right = start * 8, (start + 600) * 8
        next_left, next_right = (start + 2880) * 8, (start + 3480) * 8
        assert altered[:left] == original[:left] and altered[right:next_left] == original[right:next_left]
        assert altered[next_right:] == original[next_right:]
        assert altered[left:right] != original[left:right] and altered[next_left:next_right] != original[next_left:next_right]
        four = root / (variant + '-four.f32')
        result = subprocess.run([sys.executable, str(tool), str(source), str(four), variant, '--four-pairs'],
                                check=True, capture_output=True, text=True)
        start = int(re.search(r'pulse_start=(\d+)', result.stdout).group(1))
        assert 'pulse_count=8' in result.stdout
        altered = four.read_bytes()
        intervals = [(start + line * 12000 + offset, start + line * 12000 + offset + 600)
                     for line in (0, 18, 36, 54) for offset in (0, 2880)]
        previous = 0
        for left, right in intervals:
            assert altered[previous * 8:left * 8] == original[previous * 8:left * 8]
            assert altered[left * 8:right * 8] != original[left * 8:right * 8]
            previous = right
        assert altered[previous * 8:] == original[previous * 8:]
        broad = root / (variant + '-broad.f32')
        result = subprocess.run([sys.executable, str(tool), str(source), str(broad), variant, '--broad-white'],
                                check=True, capture_output=True, text=True)
        start = int(re.search(r'pulse_start=(\d+)', result.stdout).group(1))
        assert 'pulse_count=8' in result.stdout and 'pulse_frames=7800' in result.stdout
        altered = broad.read_bytes()
        previous = 0
        for line in range(8):
            left, right = start + line * 12000, start + line * 12000 + 7800
            assert altered[previous * 8:left * 8] == original[previous * 8:left * 8]
            assert altered[left * 8:right * 8] != original[left * 8:right * 8]
            previous = right
        assert altered[previous * 8:] == original[previous * 8:]
        four = root / (variant + '-close.f32')
        result = subprocess.run([sys.executable, str(tool), str(source), str(four), variant, '--close-pairs'],
                                check=True, capture_output=True, text=True)
        start = int(re.search(r'pulse_start=(\d+)', result.stdout).group(1))
        assert 'pulse_count=8' in result.stdout
        altered = four.read_bytes()
        intervals = [(start + line * 12000 + offset, start + line * 12000 + offset + 600)
                     for line in (0, 2, 4, 6) for offset in (0, 2880)]
        previous = 0
        for left, right in intervals:
            assert altered[previous * 8:left * 8] == original[previous * 8:left * 8]
            assert altered[left * 8:right * 8] != original[left * 8:right * 8]
            previous = right
        assert altered[previous * 8:] == original[previous * 8:]
        four = root / (variant + '-persistent.f32')
        result = subprocess.run([sys.executable, str(tool), str(source), str(four), variant, '--persistent-pairs'],
                                check=True, capture_output=True, text=True)
        start = int(re.search(r'pulse_start=(\d+)', result.stdout).group(1))
        assert 'pulse_count=204' in result.stdout
        altered = four.read_bytes()
        intervals = [(start + line * (11964 if variant == 'a' else 12036) + offset, start + line * (11964 if variant == 'a' else 12036) + offset + 600)
                     for line in range(102) for offset in (0, 2880)]
        previous = 0
        for left, right in intervals:
            assert altered[previous * 8:left * 8] == original[previous * 8:left * 8]
            assert altered[left * 8:right * 8] != original[left * 8:right * 8]
            previous = right
        assert altered[previous * 8:] == original[previous * 8:]
    silent = root / 'silent.f32'
    silent.write_bytes(bytes(len(original)))
    assert subprocess.run([sys.executable, str(tool), str(silent), str(root / 'no.f32'), 'a'],
                          capture_output=True).returncode != 0
print('PASS phasing glitch A/B, exact bounds, overwrite and silence controls')

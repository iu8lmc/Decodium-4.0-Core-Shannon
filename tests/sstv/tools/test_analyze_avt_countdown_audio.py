#!/usr/bin/env python3
"""A/B controls for the offline AVT countdown audio diagnostic."""
import array
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import wave

tool = Path(__file__).with_name('analyze_avt_countdown_audio.py')
with tempfile.TemporaryDirectory(prefix='decodium-avt-countdown-diagnostic-') as directory:
    root = Path(directory)
    for variant, amplitude, narrow in (('a', 0.45, False), ('b', 0.78, False),
                                       ('narrow-a', 0.45, True), ('narrow-b', 0.78, True)):
        symbols = []
        for counter in range(32):
            word = 0x40 | counter
            symbols.append(1900)
            for value in (word, word ^ 255):
                symbols.extend((2100 if narrow else 2200) if value & (1 << bit)
                               else (1700 if narrow else 1600) for bit in range(7, -1, -1))
        samples = array.array('h')
        phase = 0.0
        for index in range(round(len(symbols) * 12000 / 102.4)):
            symbol = min(int(index * 102.4 / 12000), len(symbols) - 1)
            phase += 2 * math.pi * symbols[symbol] / 12000
            samples.append(round(amplitude * 32767 * math.sin(phase)))
        if sys.byteorder != 'little': samples.byteswap()
        path = root / (variant + '.wav')
        with wave.open(str(path), 'wb') as output:
            output.setnchannels(1); output.setsampwidth(2); output.setframerate(12000)
            output.writeframes(samples.tobytes())
        args = [sys.executable, str(tool), str(path)] + (['--narrow'] if narrow else [])
        result = json.loads(subprocess.check_output(args, text=True))
        if result['longest_run_allowing_single_gaps'] != 32:
            raise RuntimeError('countdown not detected: ' + repr(result))
    silence = root / 'silence.wav'
    with wave.open(str(silence), 'wb') as output:
        output.setnchannels(1); output.setsampwidth(2); output.setframerate(12000)
        output.writeframes(bytes(12000 * 6 * 2))
    result = json.loads(subprocess.check_output([sys.executable, str(tool), str(silence)], text=True))
    if result['matching_frames'] != 0:
        raise RuntimeError('silence produced a false countdown: ' + repr(result))
    steady = root / 'steady.wav'
    tone = array.array('h', (round(22000 * math.sin(2 * math.pi * 1900 * i / 12000))
                       for i in range(12000 * 6)))
    if sys.byteorder != 'little': tone.byteswap()
    with wave.open(str(steady), 'wb') as output:
        output.setnchannels(1); output.setsampwidth(2); output.setframerate(12000)
        output.writeframes(tone.tobytes())
    result = json.loads(subprocess.check_output([sys.executable, str(tool), str(steady)], text=True))
    if result['matching_frames'] != 0:
        raise RuntimeError('steady tone produced a false countdown: ' + repr(result))
    broken_symbols = []
    for counter in range(32):
        word = 0x40 | counter
        broken_symbols.append(1900)
        for value in (word, word ^ 254):
            broken_symbols.extend(2200 if value & (1 << bit) else 1600 for bit in range(7, -1, -1))
    broken_samples = array.array('h')
    phase = 0.0
    for index in range(round(len(broken_symbols) * 12000 / 102.4)):
        symbol = min(int(index * 102.4 / 12000), len(broken_symbols) - 1)
        phase += 2 * math.pi * broken_symbols[symbol] / 12000
        broken_samples.append(round(18000 * math.sin(phase)))
    if sys.byteorder != 'little': broken_samples.byteswap()
    broken = root / 'broken-inverse.wav'
    with wave.open(str(broken), 'wb') as output:
        output.setnchannels(1); output.setsampwidth(2); output.setframerate(12000)
        output.writeframes(broken_samples.tobytes())
    result = json.loads(subprocess.check_output([sys.executable, str(tool), str(broken)], text=True))
    if result['longest_run_allowing_single_gaps'] >= 3:
        raise RuntimeError('broken inverse produced a valid countdown: ' + repr(result))
print('PASS AVT countdown audio diagnostic: A/B and negative controls')

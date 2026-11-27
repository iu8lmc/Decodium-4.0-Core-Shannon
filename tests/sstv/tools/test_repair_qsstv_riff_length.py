#!/usr/bin/env python3
"""Check two PCM inputs and fail-closed behaviour for the diagnostic repair."""
from array import array
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import wave

tool = Path(__file__).with_name('repair_qsstv_riff_length.py')
with tempfile.TemporaryDirectory(prefix='decodium-riff-repair-') as directory:
    root = Path(directory)
    for variant in (0, 1):
        source, result = root / f'{variant}.wav', root / f'{variant}-fixed.wav'
        samples = array('h', [0, -1000 - variant, 1000 + variant, 32767, -32768] * 20)
        if sys.byteorder != 'little':
            samples.byteswap()
        with wave.open(str(source), 'wb') as output:
            output.setnchannels(1 + variant)
            output.setsampwidth(2)
            output.setframerate(48000)
            output.writeframes(samples.tobytes())
        original = bytearray(source.read_bytes())
        expected = original[:]
        original[4:8] = (36).to_bytes(4, 'little')
        source.write_bytes(original)
        command = [sys.executable, str(tool), str(source), str(result)]
        report = json.loads(subprocess.check_output(command))
        assert result.read_bytes() == expected
        assert report['pcm_sha256'] == hashlib.sha256(expected[44:]).hexdigest()
        assert subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0
        print('PASS RIFF repair', variant)
    broken = root / 'broken.wav'
    bad = bytearray(original)
    bad[40:44] = (1).to_bytes(4, 'little')
    broken.write_bytes(bad)
    assert subprocess.run([sys.executable, str(tool), str(broken), str(root / 'bad-output.wav')],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0
    assert not (root / 'bad-output.wav').exists()

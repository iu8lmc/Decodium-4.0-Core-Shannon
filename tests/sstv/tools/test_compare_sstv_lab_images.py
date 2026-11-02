#!/usr/bin/env python3
"""Check that a single bad terminal component cannot hide in image averages."""
from pathlib import Path
import subprocess
import sys
import tempfile

tool = Path(__file__).with_name('compare_sstv_lab_images.py')
with tempfile.TemporaryDirectory(prefix='decodium-sstv-comparison-') as directory:
    root = Path(directory)
    def image(name, value):
        path = root / (name + '.ppm')
        path.write_bytes(b'P6\n20 20\n255\n' + bytes([40, 80, 100] * 399)
                         + bytes([40, 80, value]))
        return path
    def compare(reference, received, limit):
        return subprocess.run([sys.executable, str(tool), str(reference), str(received),
                               '--max-mae', '1', '--max-p95', '1',
                               '--max-last-blue-error', str(limit)],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode
    for name, value in (('a', 0), ('b', 255)):
        ref = image(name + '-reference', value)
        near = image(name + '-near', value + (4 if value == 0 else -4))
        bad = image(name + '-bad', 255 - value)
        assert compare(ref, near, 10) == 0
        assert compare(ref, bad, 10) == 1
        assert compare(ref, near, -1) == 2

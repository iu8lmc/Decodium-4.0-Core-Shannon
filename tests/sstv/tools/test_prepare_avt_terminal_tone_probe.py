#!/usr/bin/env python3
"""A/B checks for the post-capture AVT terminal-tone diagnostic."""
import array
from pathlib import Path
import subprocess
import sys
import tempfile

tool = Path(__file__).with_name('prepare_avt_terminal_tone_probe.py')
with tempfile.TemporaryDirectory(prefix='decodium-avt-terminal-test-') as directory:
    root = Path(directory)
    for label, end in (('a', 120), ('b', 330)):
        original = array.array('f', [0.02, -0.02] * 1200)
        original[2 * (end - 1)] = 0.5
        src, dst = root / (label + '-in.f32'), root / (label + '-out.f32')
        if sys.byteorder != 'little': original.byteswap()
        with src.open('wb') as f: original.tofile(f)
        subprocess.run([sys.executable, str(tool), str(src), str(dst)], check=True)
        after = array.array('f'); after.frombytes(dst.read_bytes())
        before = array.array('f'); before.frombytes(src.read_bytes())
        assert len(before) == len(after)
        assert before[:2*end] == after[:2*end]
        assert before[2*(end+480):] == after[2*(end+480):]
        assert before[2*end:2*(end+480)] != after[2*end:2*(end+480)]
        assert subprocess.run([sys.executable, str(tool), str(src), str(dst)],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0
        print('PASS terminal tone probe', label)

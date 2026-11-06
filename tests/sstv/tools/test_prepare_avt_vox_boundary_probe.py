#!/usr/bin/env python3
"""Check that the AVT boundary probe changes only the intended interval."""
import array
import math
from pathlib import Path
import subprocess
import sys
import tempfile

tool = Path(__file__).with_name('prepare_avt_vox_boundary_probe.py')
with tempfile.TemporaryDirectory(prefix='decodium-avt-vox-probe-test-') as directory:
    root = Path(directory)
    for label, boundary in (('a', 960), ('b', 1920)):
        mono = [0.5 * math.sin(2 * math.pi * (1700 if i < boundary else 1900)
                               * i / 48000) for i in range(boundary + 24000)]
        samples = array.array('f', (v for sample in mono for v in (sample, sample)))
        if sys.byteorder != 'little': samples.byteswap()
        src, dst = root / (label + '-in.f32'), root / (label + '-out.f32')
        src.write_bytes(samples.tobytes())
        subprocess.run([sys.executable, str(tool), str(src), str(dst)], check=True)
        before, after = src.read_bytes(), dst.read_bytes()
        assert len(before) == len(after)
        assert before[:boundary * 8] == after[:boundary * 8]
        assert after[boundary * 8:(boundary + 480) * 8] == before[(boundary - 480) * 8:boundary * 8]
        assert before[(boundary + 480) * 8:] == after[(boundary + 480) * 8:]
        assert subprocess.run([sys.executable, str(tool), str(src), str(dst)],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0

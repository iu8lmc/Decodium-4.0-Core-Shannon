#!/usr/bin/env python3
"""A/B test known circular active areas and uncorrected controls."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

analyzer = Path(__file__).with_name('analyze_radiofax_active_sector.py')
with tempfile.TemporaryDirectory(prefix='radiofax-active-sector-') as directory:
    root = Path(directory)
    w, h, rw, rh, row, active = 16, 10, 160, 14, 3, 144
    for variant, col in enumerate((23, 141)):
        ref = bytes((x*31 + y*17 + variant*53) % 256 for y in range(h) for x in range(w))
        rx = bytearray([255] * (rw*rh))
        for y in range(h):
            for x in range(w):
                rx[(y+row)*rw+(col+(x*active+active//2)//w)%rw] = ref[y*w+x]
        src = root / ('ref%d.pgm' % variant)
        dst = root / ('rx%d.pgm' % variant)
        src.write_bytes(('P5\n%d %d\n255\n' % (w,h)).encode() + ref)
        dst.write_bytes(('P5\n%d %d\n255\n' % (rw,rh)).encode() + rx)
        def run(start, first_row, n):
            return json.loads(subprocess.check_output([sys.executable, str(analyzer), str(src), str(dst),
                '--first-row', str(first_row), '--first-column', str(start), '--active-columns', str(n)]))['errors']
        assert run(col, row, active)['mae'] == 0
        assert run(0, 0, rw)['mae'] > 0
        print('PASS active raster', variant, 'and unaligned control')

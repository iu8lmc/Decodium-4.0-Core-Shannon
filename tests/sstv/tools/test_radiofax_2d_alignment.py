#!/usr/bin/env python3
"""Check the diagnostic against two known raster shifts."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def pgm(path, width, height, pixels):
    path.write_bytes(('P5\n{} {}\n255\n'.format(width, height)).encode() + bytes(pixels))


with tempfile.TemporaryDirectory(prefix='decodium-fax-alignment-') as directory:
    root = Path(directory)
    analyzer = Path(__file__).with_name('analyze_radiofax_2d_alignment.py')
    width, height, received_height = 128, 40, 52
    for variant, (row, column) in enumerate(((4, 7), (2, -5))):
        reference = bytes(((x * 31 + y * 71 + x * y * 3 + variant * 19) & 255)
                          for y in range(height) for x in range(width))
        received = bytearray([127] * (width * received_height))
        for y in range(height):
            for x in range(width):
                if 0 <= x + column < width:
                    received[(y + row) * width + x + column] = reference[y * width + x]
        source, output = root / 'source.pgm', root / 'received.pgm'
        pgm(source, width, height, reference)
        pgm(output, width, received_height, received)
        result = json.loads(subprocess.check_output([sys.executable, str(analyzer), str(source), str(output),
            '--max-rows', '10', '--max-columns', '8']))
        best = result['best_posthoc']
        assert (best['row'], best['column'], best['mae'], best['p95']) == (row, column, 0, 0), result
        direct = result['direct_full_raster']
        assert direct['mae'] > 8 and direct['p95'] > 4, result
        # A perfect post-hoc shift must never override the unaligned limits.
        assert not (direct['mae'] <= 8 and direct['p95'] <= 4), result
        aligned = bytearray([127] * (width * received_height))
        aligned[:len(reference)] = reference
        pgm(output, width, received_height, aligned)
        control = json.loads(subprocess.check_output([sys.executable, str(analyzer), str(source), str(output),
            '--max-rows', '10', '--max-columns', '8']))['direct_full_raster']
        assert control['mae'] <= 8 and control['p95'] <= 4, control
        print('PASS alignment diagnostic', chr(ord('A') + variant), 'row', row, 'column', column)

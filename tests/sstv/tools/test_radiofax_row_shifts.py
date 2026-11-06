#!/usr/bin/env python3
"""A/B exact cyclic shifts and an ambiguous uniform-row control."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

analyzer = Path(__file__).with_name('analyze_radiofax_row_shifts.py')
with tempfile.TemporaryDirectory(prefix='decodium-fax-row-') as temp:
    root = Path(temp)
    for index, shift in enumerate((7, -11)):
        width, height = 64, 3
        row = bytes((x * 73 + x * x * 11) % 256 for x in range(width))
        reference = row + row + bytes([127] * width)
        moved = bytes(row[(x - shift) % width] for x in range(width))
        received = row + moved + bytes([127] * width)
        paths = [root / f'{index}-{name}.pgm' for name in ('ref','rx')]
        for path, pixels in zip(paths, (reference, received)):
            path.write_bytes(f'P5\n{width} {height}\n255\n'.encode() + pixels)
        result = json.loads(subprocess.check_output([sys.executable, str(analyzer), *map(str, paths), '--max-shift', '16']))
        assert result['rows'][0]['shift'] == 0 and result['rows'][0]['mae'] == 0, result
        assert result['rows'][1]['shift'] == shift and result['rows'][1]['mae'] == 0, result
        assert result['rows'][1]['margin'] > 0 and result['rows'][2]['margin'] == 0, result
        print('PASS row shift', shift, 'and ambiguous constant row')

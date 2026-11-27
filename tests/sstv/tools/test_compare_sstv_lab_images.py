#!/usr/bin/env python3
"""Check that a single bad terminal component cannot hide in image averages."""
from pathlib import Path
import json
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
        edge = root / (name + '-edge.ppm')
        edge_bytes = bytearray(ref.read_bytes())
        header = len(b'P6\n20 20\n255\n')
        edge_bytes[header + 19 * 3] = 200 if name == 'a' else 0
        edge.write_bytes(edge_bytes)
        base = [sys.executable, str(tool), str(ref), str(edge), '--max-mae', '1', '--max-p95', '1']
        unguarded = subprocess.run(base, capture_output=True, text=True)
        assert unguarded.returncode == 0
        assert json.loads(unguarded.stdout)['last_column_maximum_error'] > 20
        edge_data = json.loads(unguarded.stdout)
        assert edge_data['last_column_mean_absolute_error'] > 0
        assert edge_data['first_column_mean_absolute_error'] == 0
        assert edge_data['last_column_p95_error'] == 0
        assert subprocess.run(base + ['--max-last-column-error', '20'],
                              stdout=subprocess.DEVNULL).returncode == 1
        assert subprocess.run(base + ['--max-last-column-error', '255'],
                              stdout=subprocess.DEVNULL).returncode == 0
        assert subprocess.run(base + ['--max-last-column-error', '-1'],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 2
        first_edge = root / (name + '-first-edge.ppm')
        first_bytes = bytearray(ref.read_bytes())
        first_bytes[header + 7 * 20 * 3 + 1] = 200 if name == 'a' else 0
        first_edge.write_bytes(first_bytes)
        first_base = [sys.executable, str(tool), str(ref), str(first_edge), '--max-mae', '1', '--max-p95', '1']
        unchecked = subprocess.run(first_base, capture_output=True, text=True)
        assert unchecked.returncode == 0
        data = json.loads(unchecked.stdout)
        assert data['first_column_maximum_error'] > 20
        assert data['last_column_maximum_error'] == 0
        assert data['first_column_mean_absolute_error'] > 0
        assert data['last_column_mean_absolute_error'] == 0
        assert data['first_column_p95_error'] == 0
        assert subprocess.run(first_base + ['--max-first-column-error', '20'],
                              stdout=subprocess.DEVNULL).returncode == 1
        assert subprocess.run(first_base + ['--max-first-column-error', '255'],
                              stdout=subprocess.DEVNULL).returncode == 0
        assert subprocess.run(first_base + ['--max-first-column-error', '-1'],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 2
print('PASS image comparator A/B, first/last column guards and negative controls')

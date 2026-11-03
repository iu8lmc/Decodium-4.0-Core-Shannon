#!/usr/bin/env python3
"""A/B synthetic controls for the Isobar diagnostic; no radio required."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import numpy as np

analyzer = Path(__file__).with_name('analyze_isobar_radiofax.py')
with tempfile.TemporaryDirectory(prefix='decodium-isobar-analyzer-') as tmp:
    root = Path(tmp)
    audio = root / 'capture.f32'
    np.full(960, 0.2, dtype='<f4').tofile(audio)
    for variant in (0, 1):
        rows, cols = np.indices((220, 1500))
        pixels = ((cols // (15 + variant) + rows * (2 + variant)) % 256).astype(np.uint8)
        raster = root / f'output-{variant}.pgm'
        raster.write_bytes(b'P5\n1500 220\n255\n' + pixels.tobytes())
        reference = root / f'reference-{variant}.pgm'
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(raster), '-vf',
                        'crop=1500:120:0:70,scale=128:120:flags=area',
                        '-frames:v', '1', str(reference)], check=True, timeout=30)
        result = subprocess.run([sys.executable, str(analyzer), str(audio), str(reference), str(raster)],
                                check=True, capture_output=True, text=True, timeout=30)
        data = json.loads(result.stdout)
        if data['estimated_first_row'] != 70 or data['timing_anchored']['mae'] != 0:
            raise RuntimeError('timing-anchored A/B self-control failed')
        if data['best_posthoc']['mae'] != 0:
            raise RuntimeError('post-hoc A/B self-control failed')
        # Check the 95.5% default and an explicitly selected 95% alternative.
        ref = ((np.indices((120, 128))[0] * (2 + variant)
                + np.indices((120, 128))[1]) % 254 + 1).astype(np.uint8)
        for columns, arguments in ((1433, []), (1425, ['--active-columns', '1425'])):
            active = (np.arange(128) * columns + columns // 2) // 128
            pixels = np.zeros((220, 1500), dtype=np.uint8)
            pixels[70:190, (active + 75) % 1500] = ref
            raster.write_bytes(b'P5\n1500 220\n255\n' + pixels.tobytes())
            reference.write_bytes(b'P5\n128 120\n255\n' + ref.tobytes())
            result = subprocess.run([sys.executable, str(analyzer), str(audio), str(reference), str(raster), *arguments],
                                    check=True, capture_output=True, text=True, timeout=30)
            data = json.loads(result.stdout)
            if data['best_posthoc_active']['mae'] != 0 or data['best_posthoc_active']['active_columns'] != columns:
                raise RuntimeError('active-sector A/B self-control failed')
    np.zeros(960, dtype='<f4').tofile(audio)
    rejected = subprocess.run([sys.executable, str(analyzer), str(audio), str(reference), str(raster)],
                              capture_output=True, text=True, timeout=30)
    if rejected.returncode == 0 or 'no strong audio onset' not in rejected.stderr:
        raise RuntimeError('silent-input negative control was not rejected')
print('PASS Isobar raster diagnostic A/B and silent-input negative control')

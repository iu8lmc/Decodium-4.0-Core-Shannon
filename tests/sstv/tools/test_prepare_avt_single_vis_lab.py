#!/usr/bin/env python3
"""A/B exact-copy and overwrite controls for the AVT VIS diagnostic."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

tool = Path(__file__).with_name('prepare_avt_single_vis_lab.py')
with tempfile.TemporaryDirectory(prefix='decodium-avt-single-vis-test-') as directory:
    root = Path(directory)
    for variant, countdown in (('a', 1.51), ('b', 1.81)):
        data = b''.join(struct.pack('<II', i, i ^ 0x12345678) for i in range(48000 * 4))
        source, destination = root / (variant + '.f32'), root / (variant + '-out.f32')
        source.write_bytes(data)
        command = [sys.executable, str(tool), str(source), str(destination),
                   '--countdown-seconds', str(countdown), '--duration-seconds', '2']
        result = json.loads(subprocess.check_output(command, text=True))
        start = round((countdown - 1.01) * 48000)
        if result['start_frame'] != start or destination.read_bytes() != data[start * 8:(start + 96000) * 8]:
            raise RuntimeError('diagnostic changed PCM samples')
        denied = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if denied.returncode == 0 or destination.read_bytes() != data[start * 8:(start + 96000) * 8]:
            raise RuntimeError('overwrite was not rejected')
    invalid = subprocess.run([sys.executable, str(tool), str(source), str(root / 'invalid.f32'),
                              '--countdown-seconds', '0.5'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if invalid.returncode == 0 or (root / 'invalid.f32').exists():
        raise RuntimeError('invalid timing accepted')
print('PASS AVT single-VIS diagnostic: A/B exact PCM copy and negative controls')

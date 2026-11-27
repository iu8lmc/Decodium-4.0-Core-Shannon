#!/usr/bin/env python3
"""A/B and negative checks for the FLdigi timing-anchored diagnostic."""
import datetime
import json
from pathlib import Path
import subprocess
import sys
import tempfile

analyzer = Path(__file__).with_name('analyze_fldigi_phasing_anchor.py')

with tempfile.TemporaryDirectory(prefix='decodium-fldigi-anchor-test-') as directory:
    root = Path(directory)
    width, height = 64, 16
    start = datetime.datetime(2026, 1, 2, 12, 0, 0)
    for variant, row in enumerate((2, 4)):
        run = root / ('run' + str(variant))
        (run / 'config/debug').mkdir(parents=True)
        (run / 'config/images').mkdir()
        (run / 'playback-clock.json').write_text(json.dumps({'wall_ns': int(start.timestamp() * 1e9)}))
        # 120 LPM: 1.025 + 2 * elapsed predicts the chosen row.
        boundary = 4.0
        event = boundary - (row - 1.025) / 2
        stamp = (start + datetime.timedelta(seconds=event)).strftime('%H:%M:%S.%f')[:-3]
        log = run / 'config/debug/status_log.txt'
        log.write_text('V: [{}] wefax : skip_phasing_to_image_save\n'
                       '    Detected phasing when in image state.\n'.format(stamp))
        image = bytes((x * 31 + y * 71 + x * y * 3 + variant * 19) & 255
                      for y in range(height) for x in range(width))
        reference = root / ('reference' + str(variant) + '.pgm')
        reference.write_bytes(('P5\n{} {}\n255\n'.format(width, height)).encode() + image)
        received = bytearray([127] * (width * (height + 8)))
        received[row * width:(row + height) * width] = image
        raw = root / ('received' + str(variant) + '.pgm')
        raw.write_bytes(('P5\n{} {}\n255\n'.format(width, height + 8)).encode() + received)
        png = run / 'config/images/result.png'
        subprocess.run(['ffmpeg', '-v', 'error', '-i', str(raw), str(png)], check=True)
        command = [sys.executable, str(analyzer), str(run), str(reference), '--image-frame', '192000']
        result = json.loads(subprocess.check_output(command))
        assert result['predicted_row'] == row and result['full_width_predicted_row']['mae'] == 0, result
        # A later rephase must not override the event before the image start.
        log.write_text(log.read_text() + 'V: [12:00:05.000] wefax : skip_phasing_to_image_save\n'
                       '    Detected phasing when in image state.\n')
        assert json.loads(subprocess.check_output(command))['predicted_row'] == row
        log.write_text('V: [12:00:05.000] wefax : skip_phasing_to_image_save\n'
                       '    Detected phasing when in image state.\n')
        assert subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0
        print('PASS timing anchor', chr(ord('A') + variant), 'and negative event controls')

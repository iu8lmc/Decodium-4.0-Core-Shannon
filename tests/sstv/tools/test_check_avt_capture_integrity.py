#!/usr/bin/env python3
"""Checks the capture diagnostic against synthetic stereo controls."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).with_name('check_avt_capture_integrity.py')


class CaptureIntegrityTest(unittest.TestCase):
    def check_capture(self, frames, expected_runs, expected_exit):
        with tempfile.TemporaryDirectory(prefix='decodium-avt-integrity-test-') as tmp:
            path = Path(tmp) / 'capture.f32'
            with path.open('wb') as out:
                for left, right in frames:
                    out.write(struct.pack('<ff', left, right))
            result = subprocess.run([sys.executable, str(SCRIPT), str(path), '--fail-on-gap'],
                                    text=True, capture_output=True, check=False)
            self.assertEqual(result.returncode, expected_exit, result.stderr)
            self.assertEqual(json.loads(result.stdout)['internal_zero_runs'], expected_runs)

    def test_internal_gap_ab_and_chunk_boundary(self):
        for gap in (480, 512):
            with self.subTest(gap=gap):
                # The zero run crosses the diagnostic's 65,536-frame read boundary.
                self.check_capture([(0.0, 0.0)] * 10 + [(0.5, 0.5)] * 65520
                                   + [(0.0, 0.0)] * gap + [(0.5, 0.5)] * 10,
                                   [{'start_frame': 65530, 'frames': gap}], 1)

    def test_leading_trailing_and_single_channel_silence(self):
        for channel in ((0.5, 0.0), (0.0, 0.5)):
            with self.subTest(channel=channel):
                self.check_capture([(0.0, 0.0)] * 600 + [channel] * 1000
                                   + [(0.0, 0.0)] * 600, [], 0)


if __name__ == '__main__':
    unittest.main()

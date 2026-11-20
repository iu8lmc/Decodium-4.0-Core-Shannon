#!/usr/bin/env python3
"""A/B and negative controls for the AVT prefix diagnostic."""
from pathlib import Path
import struct
import tempfile
import unittest
from prepare_avt_qsstv_prefix_probe import FRAME_BYTES, FRAMES, make_probe


class PrefixProbeTest(unittest.TestCase):
    def test_tone_and_silence_preserve_later_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            suffix = bytes(range(256)) * 9
            original = bytes(FRAMES * FRAME_BYTES) + suffix
            source = root / 'input.f32'
            source.write_bytes(original)
            for frequency in (0, 1100, 1500):
                destination = root / f'{frequency}.f32'
                make_probe(source, destination, frequency)
                result = destination.read_bytes()
                self.assertEqual(len(result), len(original))
                self.assertEqual(result[FRAMES * FRAME_BYTES:], suffix)
                left, right = struct.unpack_from('<ff', result, 8)
                self.assertEqual(left, right)
                self.assertEqual(left == 0.0, frequency == 0)
                with self.assertRaises(FileExistsError):
                    make_probe(source, destination, frequency)
                self.assertEqual(result, destination.read_bytes())
            with self.assertRaises(ValueError):
                make_probe(source, root / 'invalid.f32', 1900)
            self.assertFalse((root / 'invalid.f32').exists())


if __name__ == '__main__':
    unittest.main()

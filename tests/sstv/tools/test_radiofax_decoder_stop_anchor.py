#!/usr/bin/env python3
import unittest

from analyze_radiofax_decoder_stop_anchor import (aligned_segment, compare, output_rows_before_line,
                                                  stop_line, symmetric_phasing_offset)


class StopAnchorTest(unittest.TestCase):
    def test_ab_and_crossed_reference(self):
        log = '\n'.join(f'FAX L{188+i} STOP cnt={i} prepare={int(i == 6)}' for i in range(7))
        self.assertEqual(stop_line(log), 188)
        self.assertEqual(output_rows_before_line(188, 1809), 186)
        a, b = bytes([1, 2, 3, 4]), bytes([9, 8, 7, 6])
        for first, other in ((a, b), (b, a)):
            received = bytes([0] * 6) + first + bytes([0] * 2)
            self.assertEqual(compare(first, received, 2, 2, 6, 3)['stop_anchored_full_width']['mae'], 0)
            self.assertGreater(compare(other, received, 2, 2, 6, 3)['stop_anchored_full_width']['mae'], 0)

    def test_rejects_missing_or_broken_stop(self):
        for log in ('', 'FAX L188 STOP cnt=0 prepare=0',
                    '\n'.join(f'FAX L{188+2*i} STOP cnt={i} prepare={int(i == 6)}' for i in range(7))):
            with self.assertRaises(ValueError):
                stop_line(log)
        with self.assertRaises(ValueError):
            compare(bytes(4), bytes(6), 2, 2, 3, 2)

    def test_symmetric_phase_ab_and_invalid(self):
        width, height, image_row = 20, 40, 35
        for expected in (-2, 2):
            stripe = bytes(255 if (x - expected) % width < width // 2 else 0 for x in range(width))
            received = stripe * height
            self.assertEqual(symmetric_phasing_offset(received, width, height, image_row), expected)
            segment = aligned_segment(received, width, height, image_row, 2, expected)
            self.assertEqual(segment, bytes([255] * 10 + [0] * 10) * 2)
        alternating = bytes(255 if x % 2 else 0 for x in range(width)) * height
        narrow = bytes([255] * 3 + [0] * 17) * height
        for bad in (bytes(width * height), bytes([255] * width * height), alternating, narrow):
            with self.assertRaises(ValueError):
                symmetric_phasing_offset(bad, width, height, image_row)
        with self.assertRaises(ValueError):
            symmetric_phasing_offset(bytes(width * height), width, height, 20)


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env python3
"""Synthetic A/B and negative controls for retained VIS diagnostics."""
import array
import math
import unittest

from analyze_fax480_retained_vis import vis85_present


def fixture(offset, *, wrong_parity=False, wrong_data=False):
    frequencies = [1200, 1100, 1300, 1100, 1300, 1100, 1300, 1100, 1300, 1200]
    if wrong_parity:
        frequencies[8] = 1100
    if wrong_data:
        frequencies[3] = 1300
    audio = array.array('h', [0] * offset)
    position = offset
    for frequency, duration in [(1900, .3)] + [(f, .03) for f in frequencies]:
        length = round(12000 * duration)
        for i in range(length):
            audio.append(round(12000 * math.sin(2 * math.pi * frequency * (position + i) / 12000)))
        position += length
    return audio


class TestRetainedVis(unittest.TestCase):
    def test_a_and_b_offsets(self):
        for offset in (0, 37):
            with self.subTest(offset=offset):
                self.assertTrue(vis85_present(fixture(offset)))

    def test_wrong_payload_parity_and_silence(self):
        self.assertFalse(vis85_present(fixture(11, wrong_parity=True)))
        self.assertFalse(vis85_present(fixture(17, wrong_data=True)))
        self.assertFalse(vis85_present(array.array('h', [0] * 24000)))


if __name__ == '__main__':
    unittest.main()

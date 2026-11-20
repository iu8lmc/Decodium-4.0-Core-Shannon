#!/usr/bin/env python3
import unittest

from analyze_radiofax_level_scaling import analyze


class LevelScalingTest(unittest.TestCase):
    def test_opposite_measured_changes(self):
        a = analyze(2991, 1504.49, 2306.92, 1495.49, 2293.10)
        b = analyze(3009, 1495.49, 2293.13, 1504.49, 2306.89)
        self.assertAlmostEqual(a['implied_period'], 3009, delta=0.2)
        self.assertAlmostEqual(b['implied_period'], 2991, delta=0.2)
        self.assertLess(a['ratio_disagreement'], 0.0001)
        self.assertLess(b['ratio_disagreement'], 0.0001)

    def test_stable_and_frequency_offset_controls(self):
        for period in (2991, 3009):
            stable = analyze(period, 1500, 2300, 1500, 2300)
            shifted = analyze(period, 1500, 2300, 1510, 2310)
            self.assertEqual(stable['implied_period'], period)
            self.assertGreater(shifted['ratio_disagreement'], 0.002)

    def test_invalid_values(self):
        for bad in (float('nan'), float('inf'), -1, 0):
            with self.assertRaises(ValueError):
                analyze(3000, 1500, 2300, bad, 2300)
        with self.assertRaises(ValueError):
            analyze(3000, 2300, 1500, 1500, 2300)


if __name__ == '__main__':
    unittest.main()

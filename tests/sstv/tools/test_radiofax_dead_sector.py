#!/usr/bin/env python3
import unittest

from analyze_radiofax_dead_sector import compare


class DeadSectorDiagnosticTest(unittest.TestCase):
    def test_ab_and_wrong_reference(self):
        # 20 received samples per line: zero-length dead sector after integer
        # rounding; a start near the end crosses into the following line.
        received = bytes(range(80))
        a = bytes([18, 28, 38, 48])
        b = bytes([19, 29, 39, 49])
        self.assertEqual(compare(a, received, 2, 2, 20, 0, 13)['mae'], 0)
        self.assertEqual(compare(b, received, 2, 2, 20, 0, 14)['mae'], 0)
        self.assertEqual(compare(a, received, 2, 2, 20, 0, 14)['mae'], 1)

    def test_invalid_and_short_inputs(self):
        with self.assertRaises(ValueError):
            compare(b'1234', b'1', 2, 2, 20, 0, 0)
        with self.assertRaises(ValueError):
            compare(b'1234', bytes(80), 2, 2, 20, 0, 20)

    def test_nonzero_dead_sector(self):
        received = bytes(range(200))
        self.assertEqual(compare(bytes([22, 46, 70, 94]), received,
                                 4, 1, 100, 0, 10)['mae'], 0)
        self.assertGreater(compare(bytes([22, 46, 70, 94]), received,
                                   4, 1, 100, 0, 11)['mae'], 0)


if __name__ == '__main__':
    unittest.main()

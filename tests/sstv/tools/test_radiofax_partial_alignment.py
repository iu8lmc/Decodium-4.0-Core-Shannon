#!/usr/bin/env python3
"""A/B recovery, crossed-image and flat-input controls for partial alignment."""
import numpy as np
from analyze_radiofax_partial_alignment import compare

rng = np.random.default_rng(828)
a = rng.integers(0, 256, (24, 32), dtype=np.uint8)
b = rng.integers(0, 256, (24, 32), dtype=np.uint8)
for image, other, offset, shift in ((a, b, 3, 4), (b, a, 5, 12)):
    candidate = np.full((36, 32), 127, dtype=np.uint8)
    candidate[offset:offset + 24] = np.roll(image, -shift, axis=1)
    fit = compare(image, candidate, 2, 18, 4)
    assert fit['best_posthoc']['candidate_first_row'] == offset + 2, fit
    assert fit['best_posthoc']['circular_shift'] == shift, fit
    assert fit['best_posthoc']['mae'] == 0, fit
    assert abs(fit['best_posthoc']['correlation'] - 1) < 1e-12, fit
    crossed = compare(other, candidate, 2, 18, 4)
    assert crossed['best_posthoc']['mae'] > 50, crossed
    indices = (np.arange(32) * 28 + 14) // 32
    active_ref = candidate[offset:offset + 24][:, (indices - shift) % 32]
    active_fit = compare(active_ref, candidate, 2, 18, 4, 28)
    assert active_fit['best_posthoc']['candidate_first_row'] == offset + 2, active_fit
    assert active_fit['best_posthoc']['circular_shift'] == shift, active_fit
    assert active_fit['best_posthoc']['mae'] == 0, active_fit
    smooth = ((image.astype(np.uint16) + np.roll(image, 1, axis=1).astype(np.uint16)
               + np.roll(image, 2, axis=1).astype(np.uint16)) // 3).astype(np.uint8)
    candidate[offset:offset + 24] = np.roll(smooth, -(shift + 1), axis=1)
    refined = compare(smooth, candidate, 2, 18, 4, refine=True)
    assert refined['winning_row_refined'] and refined['best_posthoc']['candidate_first_row'] == offset + 2, refined
    assert refined['best_posthoc']['circular_shift'] == shift + 1 and refined['best_posthoc']['mae'] == 0, refined
try:
    compare(np.full((24, 32), 127, np.uint8), np.zeros((36, 32), np.uint8), 0, 18, 4)
except ValueError:
    pass
else:
    raise AssertionError('flat input must be rejected')
flat_candidate = np.full((36, 32), 127, np.uint8)
assert compare(a, flat_candidate, 2, 18, 4)['best_posthoc']['correlation'] is None
try:
    compare(np.zeros((100, 512), np.uint8), np.zeros((300, 512), np.uint8), 0, 100, 4)
except ValueError as error:
    assert 'work bound' in str(error)
else:
    raise AssertionError('excessive diagnostic search must be rejected')
print('PASS partial Radiofax alignment A/B, crossed-image and flat-input controls')

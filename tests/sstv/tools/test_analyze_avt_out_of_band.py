#!/usr/bin/env python3
"""A/B controls for the offline out-of-band spectral measurement."""
import numpy as np
from analyze_avt_out_of_band import measure

time = np.arange(48000) / 48000.0
for phase, frequency in ((0.2, 410.37), (1.1, 3320.37)):
    signal = 0.6 * np.sin(2 * np.pi * 1900 * time + phase)
    clean = measure(signal)
    mixed = measure(signal, injected_hz=frequency, injected_peak=0.15)
    assert clean['outside_to_inside_peak'] < 0.001
    assert abs(mixed['frequency_hz'] - frequency) <= 0.5
    assert 0.22 < mixed['outside_to_inside_peak'] < 0.26
    noise = np.random.default_rng(int(frequency)).normal(0.0, 0.08, time.size)
    broadband = measure(signal + noise)
    assert broadband['outside_to_inside_peak'] < 0.02
    assert broadband['outside_to_inside_power'] > 0.003
for phase, frequency in ((0.3, 900.37), (1.4, 2600.37)):
    signal = 0.6 * np.sin(2 * np.pi * 1900 * time + phase)
    clean = measure(signal)
    mixed = measure(signal, injected_hz=frequency, injected_peak=0.15)
    assert clean['guard_to_inside_peak'] < 0.001
    assert abs(mixed['guard_frequency_hz'] - frequency) <= 0.5
    assert 0.22 < mixed['guard_to_inside_peak'] < 0.26
    assert mixed['guard_to_inside_power'] > 0.05
    # The old distant-band measurement alone is blind to these tones.
    assert mixed['outside_to_inside_peak'] < 0.001
print('PASS spectral measurements A/B, including adjacent guard bands')

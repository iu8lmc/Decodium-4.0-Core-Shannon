#!/usr/bin/env python3
"""Design exploratory FIR equalizers; not a production filter designer."""
import argparse
import json
import numpy as np

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('taps', type=int, choices=(9, 13, 17))
args = parser.parse_args()
delay = {9: 6, 13: 8, 17: 10}[args.taps]
sample_rate = 12000.0
frequency = np.linspace(50.0, 5950.0, 2000)
omega = 2 * np.pi * frequency / sample_rate
z = np.exp(-1j * omega)
response = np.ones(frequency.size, dtype=np.complex128)
for cutoff, highpass in ((900.0, True), (2500.0, False)):
    cosine = np.cos(2 * np.pi * cutoff / sample_rate)
    alpha = np.sin(2 * np.pi * cutoff / sample_rate) / np.sqrt(2)
    taps = ([(1 + cosine) / 2, -(1 + cosine), (1 + cosine) / 2] if highpass
            else [(1 - cosine) / 2, 1 - cosine, (1 - cosine) / 2])
    response *= (taps[0] + taps[1] * z + taps[2] * z * z) / (1 + alpha - 2 * cosine * z + (1 - alpha) * z * z)
matrix = response[:, None] * np.exp(-1j * omega[:, None] * np.arange(args.taps))
passband = (frequency >= 1500) & (frequency <= 2300)
stopband = (frequency <= 800) | (frequency >= 3000)
weights = np.where(passband, 1.0, np.where(stopband, 0.5, 0.15))
target = np.where(passband, np.exp(-1j * omega * delay), 0.0)
weighted = matrix * weights[:, None]
system = np.vstack((weighted.real, weighted.imag, np.eye(args.taps) * 0.05))
rhs = np.concatenate(((target * weights).real, (target * weights).imag, np.zeros(args.taps)))
coefficients = np.linalg.lstsq(system, rhs, rcond=None)[0]
print(json.dumps({'taps': args.taps, 'delay': delay,
                  'coefficients': [float(f'{value:.8g}') for value in coefficients]}))

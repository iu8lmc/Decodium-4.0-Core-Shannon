#!/usr/bin/env python3
"""Diagnostic timing-anchored and post-hoc Isobar raster comparisons.

The first strong captured audio estimates the TX onset. The 35-second offset
is specific to the selected 120-LPM laboratory WMO preamble and can include
capture/VOX latency. This is not a standards or interoperability verdict.
"""
import argparse
import json
from pathlib import Path
import numpy as np
from analyze_radiofax_2d_alignment import dimensions, gray

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('capture', type=Path, help='48 kHz stereo float32 LE capture')
p.add_argument('reference', type=Path)
p.add_argument('isobar', type=Path)
p.add_argument('--preamble-seconds', type=float, default=35.0)
p.add_argument('--row-window', type=int, default=12)
p.add_argument('--rms-threshold', type=float, default=0.1)
p.add_argument('--active-columns', type=int, default=1433,
               help='WMO active sector at Isobar width, nearest to 95.5%% after the 4.5%% dead sector')
p.add_argument('--column-step', type=int, default=5)
a = p.parse_args()
if not (0 <= a.preamble_seconds <= 180 and 0 <= a.row_window <= 100
        and 0 < a.rms_threshold <= 1 and 128 <= a.active_columns <= 1500
        and 1 <= a.column_step <= 1500):
    p.error('invalid timing or search parameter')
raw = np.memmap(a.capture, mode='r', dtype='<f4')
if len(raw) < 960 or len(raw) % 2:
    p.error('capture must contain stereo float samples')
blocks = raw[:len(raw) // 960 * 960].reshape(-1, 480, 2)[:, :, 0]
rms = np.sqrt(np.mean(blocks * blocks, axis=1))
strong = np.flatnonzero(rms > a.rms_threshold)
if not len(strong):
    p.error('no strong audio onset')
onset = int(strong[0]) / 100.0
width, height = dimensions(a.reference)
output_width, output_height = dimensions(a.isobar)
if width != 128 or height != 120 or output_width != 1500:
    p.error('expected 128x120 reference and 1500-pixel Isobar raster')
row = int(round((onset + a.preamble_seconds) * 2))
if row - a.row_window < 0 or row + a.row_window + height > output_height:
    p.error('predicted row or search window outside output raster')
reference = np.frombuffer(gray(a.reference, width, height), dtype=np.uint8).reshape(height, width).astype(np.int16)
received = np.frombuffer(gray(a.isobar, width, output_height), dtype=np.uint8).reshape(output_height, width).astype(np.int16)
full = np.frombuffer(gray(a.isobar, output_width, output_height), dtype=np.uint8).reshape(output_height, output_width).astype(np.int16)

def errors(patch):
    values = np.sort(np.abs(reference - patch).ravel())
    return {'mae': float(np.mean(values)), 'p95': int(values[int(len(values) * 0.95)]),
            'maximum': int(values[-1])}

direct = errors(received[row:row + height])
best = None
for y in range(row - a.row_window, row + a.row_window + 1):
    for x in range(-width // 2, width // 2 + 1):
        metrics = errors(np.roll(received[y:y + height], x, axis=1))
        candidate = (metrics['mae'], y, x, metrics)
        if best is None or candidate[:3] < best[:3]:
            best = candidate
active = (np.arange(width) * a.active_columns + a.active_columns // 2) // width
best_active = None
for y in range(row - a.row_window, row + a.row_window + 1):
    for x in range(0, output_width, a.column_step):
        patch = full[y:y + height][:, (active + x) % output_width]
        candidate = (float(np.mean(np.abs(reference - patch))), y, x)
        if best_active is None or candidate < best_active:
            best_active = candidate
active_patch = full[best_active[1]:best_active[1] + height][:,
                    (active + best_active[2]) % output_width]
print(json.dumps({'onset_seconds': onset, 'preamble_seconds': a.preamble_seconds,
                  'estimated_first_row': row, 'resized_width': width,
                  'timing_anchored': direct,
                  'best_posthoc': {'row': best[1], 'circular_shift': best[2], **best[3]},
                  'best_posthoc_active': {'row': best_active[1], 'first_column': best_active[2],
                                          'active_columns': a.active_columns, **errors(active_patch)}}, sort_keys=True))

#!/usr/bin/env python3
"""Exploratory comparison of partial rasters; a post-hoc fit is not a pass."""
import argparse
import json
import numpy as np
from analyze_radiofax_2d_alignment import dimensions, gray


def compare(reference, candidate, first_row, rows, column_step, active_columns=None, refine=False):
    """Compare reference rows with all candidate rows and circular shifts."""
    if reference.ndim != 2 or candidate.ndim != 2 or reference.shape[1] != candidate.shape[1]:
        raise ValueError('expected two equal-width grayscale rasters')
    h, w = reference.shape
    ch = candidate.shape[0]
    if active_columns is None:
        active_columns = w
    if not (0 <= first_row and 1 <= rows <= h - first_row and rows <= ch
            and 1 <= column_step <= w and 1 <= active_columns <= w
            and w <= 4096 and ch <= 5000):
        raise ValueError('invalid search bounds')
    if rows * w * (ch - rows + 1) * ((w + column_step - 1) // column_step) > 1_000_000_000:
        raise ValueError('search exceeds diagnostic work bound')
    ref = reference[first_row:first_row + rows].astype(np.int16)
    if np.ptp(ref) < 16 or np.std(ref) < 3:
        raise ValueError('reference lacks image contrast')
    target = candidate.astype(np.int16)
    indices = (np.arange(w) * active_columns + active_columns // 2) // w
    best = None
    for y in range(ch - rows + 1):
        block = target[y:y + rows]
        for shift in range(0, w, column_step):
            errors = np.abs(ref - block[:, (indices - shift) % w])
            cost = float(np.mean(errors))
            if best is None or cost < best[0]:
                best = (cost, y, shift)
    coarse = best
    if refine and column_step > 1:
        # Refine the winning coarse row only; this does not prove the global
        # optimum over all sub-step shifts at every other row.
        block = target[coarse[1]:coarse[1] + rows]
        for shift in {(coarse[2] + d) % w for d in range(-column_step + 1, column_step)}:
            cost = float(np.mean(np.abs(ref - block[:, (indices - shift) % w])))
            if cost < best[0]:
                best = (cost, coarse[1], shift)
    aligned = target[best[1]:best[1] + rows][:, (indices - best[2]) % w]
    errors = np.abs(ref - aligned)
    candidate_std = float(np.std(aligned))
    correlation = (float(np.corrcoef(ref.ravel(), aligned.ravel())[0, 1])
                   if candidate_std > 0 else None)
    result = {'reference_first_row': first_row, 'rows': rows, 'column_step': column_step,
              'candidate_active_columns': active_columns,
              'winning_row_refined': bool(refine and column_step > 1),
              'best_posthoc': {'candidate_first_row': best[1], 'circular_shift': best[2],
                               'mae': best[0], 'p95': int(np.percentile(errors, 95)),
                               'max': int(np.max(errors)), 'correlation': correlation}}
    if first_row + rows <= ch:
        direct = np.abs(ref - target[first_row:first_row + rows][:, indices])
        result['direct_same_row'] = {'mae': float(np.mean(direct)), 'p95': int(np.percentile(direct, 95))}
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('reference'); p.add_argument('candidate')
    p.add_argument('--first-row', type=int, required=True)
    p.add_argument('--rows', type=int, required=True)
    p.add_argument('--column-step', type=int, default=4)
    p.add_argument('--active-columns', type=int, help='candidate columns mapped across the reference width')
    p.add_argument('--refine', action='store_true', help='refine columns only at the winning coarse row')
    a = p.parse_args()
    width, height = dimensions(a.reference)
    _, candidate_height = dimensions(a.candidate)
    if not 1 <= width <= 4096 or not 1 <= height <= 5000 or not 1 <= candidate_height <= 5000:
        p.error('image dimensions outside analysis limits')
    ref = np.frombuffer(gray(a.reference, width, height), dtype=np.uint8).reshape(height, width)
    candidate = np.frombuffer(gray(a.candidate, width, candidate_height), dtype=np.uint8).reshape(candidate_height, width)
    print(json.dumps(compare(ref, candidate, a.first_row, a.rows, a.column_step, a.active_columns, a.refine), sort_keys=True))


if __name__ == '__main__':
    main()

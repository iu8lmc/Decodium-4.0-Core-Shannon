#!/usr/bin/env python3
"""Measure an explicitly positioned active raster in a circular FAX canvas.

Diagnostic only: supplied row and column offsets are not inferred or applied
by Decodium and this tool is not an interoperability acceptance test.
"""
import argparse
import json
from analyze_radiofax_2d_alignment import dimensions, gray, measure

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('reference')
p.add_argument('received')
p.add_argument('--first-row', type=int, required=True)
p.add_argument('--first-column', type=int, required=True)
p.add_argument('--active-columns', type=int, required=True)
a = p.parse_args()
w, h = dimensions(a.reference)
rw, rh = dimensions(a.received)
if not (0 <= a.first_row and a.first_row + h <= rh and 0 <= a.first_column < rw
        and w <= a.active_columns <= rw and w*h <= 16777216 and rw*rh <= 16777216):
    p.error('active raster does not fit')
ref = gray(a.reference, w, h)
image = gray(a.received, rw, rh)
cols = [(a.first_column + (x * a.active_columns + a.active_columns // 2) // w) % rw
        for x in range(w)]
errors = [abs(ref[y*w+x] - image[(y+a.first_row)*rw+cols[x]])
          for y in range(h) for x in range(w)]
print(json.dumps({'reference_size': [w, h], 'received_size': [rw, rh],
                  'first_row': a.first_row, 'first_column': a.first_column,
                  'active_columns': a.active_columns, 'errors': measure(errors)}, sort_keys=True))

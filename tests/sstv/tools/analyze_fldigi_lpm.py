#!/usr/bin/env python3
"""Summarise detected phasing and applied raster speed in a FLdigi log.

Diagnostic only: an applied speed does not prove image quality or successful
interoperability, and absence of a log entry does not prove it never occurred.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('log', type=Path)
parser.add_argument('--expected-lpm', type=int, required=True)
args = parser.parse_args()
if args.expected_lpm <= 0 or args.expected_lpm > 1000:
    parser.error('expected LPM out of bounds')
if args.log.stat().st_size > 64 * 1024 * 1024:
    parser.error('log exceeds diagnostic size bound')
contents = args.log.read_text(errors='replace')
detected = Counter(int(value) for value in re.findall(
    r'Decoding phasing line, lpm =\s*(\d+)', contents))
applied = Counter(float(value) for value in re.findall(
    r'm_phase_lines\s*=\s*4,.*?m_lpm_img\s*=\s*([0-9.]+)', contents))
result = {'expected_lpm': args.expected_lpm, 'detected': dict(sorted(detected.items())),
          'applied_on_four_line_transition': dict(sorted(applied.items())),
          'discarded_low_correlation': contents.count('Do not save image with correlation <'),
          'apt_stop': 'Receiving Apt stop frequency: 450 Hz' in contents}
print(json.dumps(result, sort_keys=True))
if not detected or not applied:
    raise SystemExit(2)
if args.expected_lpm not in detected or any(speed != args.expected_lpm for speed in applied):
    raise SystemExit(1)

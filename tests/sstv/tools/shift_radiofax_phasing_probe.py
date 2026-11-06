#!/usr/bin/env python3
"""Shift a selected stereo float32 phasing interval; diagnostic only."""
import argparse
import shutil
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--start', type=int, required=True, help='capture seconds')
p.add_argument('--end', type=int, required=True, help='capture seconds')
p.add_argument('--shift-samples', type=int, required=True, help='positive reads later input')
a = p.parse_args()
frame = 8
rate = 48000
start, end = a.start * rate * frame, a.end * rate * frame
shift = a.shift_samples * frame
size = a.source.stat().st_size
if size % frame or not (0 <= start < end <= size and 0 <= start + shift < end + shift <= size):
    p.error('invalid capture or selected interval')
def copy_exact(src, dst, amount):
    while amount:
        block = src.read(min(amount, 1024 * 1024))
        if not block:
            raise ValueError('truncated capture')
        dst.write(block)
        amount -= len(block)
with a.source.open('rb') as src, a.output.open('xb') as dst:
    copy_exact(src, dst, start)
    src.seek(start + shift)
    copy_exact(src, dst, end - start)
    src.seek(end)
    shutil.copyfileobj(src, dst, 1024 * 1024)
print('diagnostic output:', a.output)

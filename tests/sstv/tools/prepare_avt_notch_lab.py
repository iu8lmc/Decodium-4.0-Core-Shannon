#!/usr/bin/env python3
"""Prepare a bounded, gated notch experiment; not an application filter."""
import argparse
import math
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--frequency', type=float, required=True)
parser.add_argument('--gate-seconds', type=float, required=True)
parser.add_argument('--tone-peak', type=float, default=0.0)
parser.add_argument('--quality', type=float, default=100.0)
args = parser.parse_args()
source = args.source.resolve(strict=True)
if not source.is_file() or not 0 < source.stat().st_size <= 90 * 48000 * 8 or source.stat().st_size % 8:
    raise SystemExit('invalid 48 kHz stereo float capture')
if args.output.exists(): raise SystemExit('output already exists')
if (not all(math.isfinite(value) for value in (args.frequency, args.gate_seconds, args.tone_peak, args.quality))
    or not 100 <= args.frequency <= 4000 or not 0 <= args.gate_seconds <= 90
    or not 0 <= args.tone_peak <= 0.2 or not 2 <= args.quality <= 500):
    raise SystemExit('invalid filter parameters')
# aeval loses the channel layout on some FFmpeg versions. Explicitly restore
# stereo or bandreject may silently pass both channels unchanged.
expression = (f'aeval=0.8*(val(0)+{args.tone_peak}*sin(2*PI*{args.frequency}*t))|'
              f'0.8*(val(1)+{args.tone_peak}*sin(2*PI*{args.frequency}*t)),'
              f'aformat=channel_layouts=stereo,bandreject=f={args.frequency}:t=q:w={args.quality}:'
              f"enable='gte(t,{args.gate_seconds})'")
subprocess.run(['ffmpeg', '-v', 'error', '-n', '-f', 'f32le', '-ar', '48000', '-ac', '2',
                '-i', str(source), '-af', expression, '-f', 'f32le', str(args.output)], check=True)
if args.output.stat().st_size != source.stat().st_size:
    raise SystemExit('unexpected output length')
print('prepared', args.output, 'frequency', args.frequency, 'gate', args.gate_seconds,
      'tone_peak', args.tone_peak, 'Q', args.quality, flush=True)

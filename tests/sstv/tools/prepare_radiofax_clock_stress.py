#!/usr/bin/env python3
"""Extend a selected phasing line and simulate clock drift in a lab capture.

Input and output are stereo 48 kHz little-endian float32. This deliberately
modifies a recording and does not establish the behaviour of a real radio.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--edge-seconds', type=float, required=True)
    parser.add_argument('--line-seconds', type=float, required=True)
    parser.add_argument('--copies', type=int, required=True)
    parser.add_argument('--asetrate', type=int, required=True)
    args = parser.parse_args()
    if not (0 <= args.edge_seconds <= 120 and 0.24 <= args.line_seconds <= 1.01
            and 1 <= args.copies <= 120 and 47520 <= args.asetrate <= 48480):
        parser.error('invalid edge, period, copy count or simulated sample rate')
    size = args.source.stat().st_size
    if size == 0 or size % 8 or size > 120 * 48000 * 8:
        parser.error('input must be bounded stereo float32 PCM')
    if args.output.exists() or args.output.is_symlink():
        parser.error('refusing to overwrite output')
    start = round(args.edge_seconds * 48000) * 8
    line_bytes = round(args.line_seconds * 48000) * 8
    if start + line_bytes > size:
        parser.error('selected line falls outside the capture')
    with tempfile.TemporaryDirectory(prefix='decodium-fax-clock-') as temp:
        expanded = Path(temp) / 'expanded.f32'
        with args.source.open('rb') as source, expanded.open('xb') as target:
            target.write(source.read(start))
            line = source.read(line_bytes)
            if len(line) != line_bytes:
                raise RuntimeError('short input while reading the selected line')
            for _ in range(args.copies):
                target.write(line)
            target.write(line)
            while block := source.read(1024 * 1024):
                target.write(block)
        subprocess.run(['ffmpeg', '-v', 'error', '-n', '-f', 'f32le', '-ar', '48000',
                        '-ac', '2', '-i', str(expanded), '-af',
                        f'asetrate={args.asetrate},aresample=48000', '-f', 'f32le',
                        str(args.output)], check=True, timeout=120)
    print(json.dumps({'added_seconds': line_bytes // 8 * args.copies / 48000,
                      'asetrate': args.asetrate,
                      'output_seconds': args.output.stat().st_size / (8 * 48000)}))


if __name__ == '__main__':
    main()

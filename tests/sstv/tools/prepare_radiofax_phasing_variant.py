#!/usr/bin/env python3
"""Create a diagnostic stereo PCM copy with selected whole phasing lines removed."""
import argparse
from pathlib import Path

FRAME_BYTES = 8


def prepare(source: Path, output: Path, first_frame: int, removed_lines: int,
            end_line_frame: int, line_frames: int) -> None:
    source = source.resolve(strict=True)
    size = source.stat().st_size
    if not source.is_file() or size <= 0 or size % FRAME_BYTES or size > 240 * 48000 * FRAME_BYTES:
        raise ValueError('invalid bounded float-stereo input')
    total_frames = size // FRAME_BYTES
    first_end = first_frame + removed_lines * line_frames
    second_end = end_line_frame + line_frames
    if (line_frames < 1 or removed_lines < 1 or first_frame < 0
            or first_end > end_line_frame or second_end > total_frames):
        raise ValueError('phasing cuts are invalid or overlap')
    with source.open('rb') as incoming, output.open('xb') as dest:
        previous = 0
        for start, end in ((first_frame, first_end), (end_line_frame, second_end)):
            remaining = (start - previous) * FRAME_BYTES
            while remaining:
                block = incoming.read(min(remaining, 1024 * 1024))
                if not block:
                    raise ValueError('source truncated while copying')
                dest.write(block)
                remaining -= len(block)
            incoming.seek((end - start) * FRAME_BYTES, 1)
            previous = end
        while block := incoming.read(1024 * 1024):
            dest.write(block)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--first-frame', required=True, type=int)
    parser.add_argument('--removed-lines', required=True, type=int)
    parser.add_argument('--end-line-frame', required=True, type=int)
    parser.add_argument('--line-frames', required=True, type=int)
    args = parser.parse_args()
    prepare(args.source, args.output, args.first_frame, args.removed_lines,
            args.end_line_frame, args.line_frames)


if __name__ == '__main__':
    main()

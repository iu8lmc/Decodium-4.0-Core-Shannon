#!/usr/bin/env python3
"""Diagnostic FLdigi raster comparison anchored by audio/log timing.

The image boundary must be measured independently from the capture, not
selected by inspecting the decoded image. This is not a protocol conformance
test; audio latency and log timestamp precision limit the predicted row.
"""
import argparse
import datetime
import json
import math
from pathlib import Path

from analyze_radiofax_2d_alignment import dimensions, gray, measure


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('run', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('--image-frame', required=True, type=int)
    parser.add_argument('--sample-rate', type=int, default=48000)
    parser.add_argument('--lpm', type=int, default=120)
    args = parser.parse_args()
    if args.image_frame < 0 or args.sample_rate <= 0 or args.lpm <= 0:
        raise ValueError('invalid audio boundary or rate')
    clock = json.loads((args.run / 'playback-clock.json').read_text())
    wall_start = datetime.datetime.fromtimestamp(clock['wall_ns'] / 1e9)
    lines = (args.run / 'config/debug/status_log.txt').read_text(errors='replace').splitlines()
    events = []
    for index, line in enumerate(lines):
        if 'Detected phasing when in image state.' not in line or index == 0:
            continue
        header = lines[index - 1]
        event_time = datetime.time.fromisoformat(header.split('[')[1].split(']')[0])
        event = datetime.datetime.combine(wall_start.date(), event_time)
        # Permit an ordinary midnight rollover, but reject earlier events.
        if event < wall_start - datetime.timedelta(hours=12):
            event += datetime.timedelta(days=1)
        elapsed = (event - wall_start).total_seconds()
        if 0 <= elapsed <= args.image_frame / args.sample_rate:
            events.append(elapsed)
    if not events:
        raise ValueError('no logged rephase before the image boundary')
    seconds = args.image_frame / args.sample_rate - max(events)
    # FLdigi 4.2.11 sets m_img_sample to 1.025 lines at rephase.
    row = math.floor(1.025 + seconds * args.lpm / 60 + 0.5)
    images = list((args.run / 'config/images').glob('*.png'))
    if len(images) != 1:
        raise ValueError('expected exactly one FLdigi PNG')
    width, height = dimensions(args.reference)
    received_width, received_height = dimensions(images[0])
    if row < 0 or row + height > received_height:
        raise ValueError('predicted image extends outside received raster')
    ref = gray(args.reference, width, height)
    received = gray(images[0], width, received_height)
    errors = [abs(ref[y * width + x] - received[(y + row) * width + x])
              for y in range(height) for x in range(width)]
    print(json.dumps({'predicted_row': row, 'last_rephase_seconds': max(events),
                      'audio_image_boundary_seconds': args.image_frame / args.sample_rate,
                      'reference_size': [width, height],
                      'received_size': [received_width, received_height],
                      'full_width_predicted_row': measure(errors)}, sort_keys=True))


if __name__ == '__main__':
    main()

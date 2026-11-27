#!/usr/bin/env python3
"""Compare independently decoded Radiofax rows anchored by its logged stop.

This diagnostic assumes radio_fax_decoder's default header-inclusive output,
IOC 576 line blending and an externally supplied transmitted image height.
It never searches image pixels for an offset.
"""
import argparse
import json
import math
from pathlib import Path
import re
import statistics
import subprocess

from analyze_radiofax_2d_alignment import dimensions, gray, measure


def stop_line(log):
    events = [(int(line), int(count), int(prepare)) for line, count, prepare in
              re.findall(r'FAX L(\d+) STOP cnt=(\d+) prepare=(\d+)', log)]
    for index, (line, count, _) in enumerate(events):
        if count != 0:
            continue
        run = events[index:index + 7]
        if (len(run) == 7 and all(item[0] == line + offset and item[1] == offset
                                  for offset, item in enumerate(run))
                and run[-1][2] == 1):
            return line
    raise ValueError('no sustained consecutive stop sequence in decoder log')


def output_rows_before_line(line, width):
    if not 0 <= line <= 100000 or not 1 <= width <= 10000:
        raise ValueError('invalid source line or decoder width')
    increment = width / (math.pi * 576)
    accumulator = 0.0
    emitted = 0
    for _ in range(line):
        if accumulator >= 1.0:
            accumulator -= 1.0
            emitted += 1
        accumulator += increment
    return emitted


def compare(reference, received, width, height, received_height, row):
    if (width <= 0 or height <= 0 or row < 0 or row + height > received_height
            or len(reference) != width * height or len(received) != width * received_height):
        raise ValueError('invalid reference, received raster or anchored row')
    direct = measure([abs(reference[i] - received[i]) for i in range(width * height)])
    anchored = measure([abs(reference[i] - received[row * width + i]) for i in range(width * height)])
    return {'direct_full_raster': direct, 'stop_anchored_full_width': anchored}


def symmetric_phasing_offset(received, width, received_height, image_row):
    """Find the leading white edge from phasing rows, without reference pixels."""
    if (width < 4 or image_row < 30 or image_row > received_height
            or len(received) != width * received_height):
        raise ValueError('insufficient phasing raster')
    rows = [received[y * width:(y + 1) * width] for y in range(image_row - 30, image_row - 5)]
    white = [statistics.median(row[x] for row in rows) > 128 for x in range(width)]
    fraction = sum(white) / width
    rising = [x for x in range(width) if white[x] and not white[x - 1]]
    falling = [x for x in range(width) if not white[x] and white[x - 1]]
    if not 0.4 <= fraction <= 0.6 or len(rising) != 1 or len(falling) != 1:
        raise ValueError('phasing rows do not contain one stable symmetric stripe')
    edge = rising[0]
    return edge if edge <= width // 2 else edge - width


def aligned_segment(received, width, received_height, row, height, offset):
    start = row * width + offset
    end = start + width * height
    if start < 0 or end > width * received_height or len(received) != width * received_height:
        raise ValueError('phase-aligned segment is outside the received raster')
    return received[start:end]


def resize_raw_gray(pixels, original_width, height, width):
    result = subprocess.check_output(['ffmpeg', '-v', 'error', '-f', 'rawvideo',
        '-pixel_format', 'gray', '-video_size', f'{original_width}x{height}',
        '-i', 'pipe:0', '-frames:v', '1', '-vf', f'scale={width}:{height}:flags=area',
        '-f', 'rawvideo', '-pix_fmt', 'gray', 'pipe:1'], input=pixels)
    if len(result) != width * height:
        raise ValueError('unexpected resized segment length')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference')
    parser.add_argument('received')
    parser.add_argument('decoder_log', type=Path)
    parser.add_argument('--symmetric-phasing', action='store_true',
                        help='derive horizontal alignment from preceding symmetric phasing rows')
    args = parser.parse_args()
    width, height = dimensions(args.reference)
    decoded_width, decoded_height = dimensions(args.received)
    # This matches the pinned CLI's default 1809-pixel IOC 576 setting and
    # avoids claiming the same row accounting for other decoder settings.
    if decoded_width != 1809:
        parser.error('expected the pinned decoder default 1809-pixel raster')
    try:
        line = stop_line(args.decoder_log.read_text(errors='replace'))
        emitted = output_rows_before_line(line, decoded_width)
        row = emitted - height
        reference = gray(args.reference, width, height)
        result = compare(reference, gray(args.received, width, decoded_height), width, height,
                         decoded_height, row)
        if args.symmetric_phasing:
            native = gray(args.received, decoded_width, decoded_height)
            offset = symmetric_phasing_offset(native, decoded_width, decoded_height, row)
            segment = aligned_segment(native, decoded_width, decoded_height, row, height, offset)
            pixels = resize_raw_gray(segment, decoded_width, height, width)
            result['phasing_offset_native_pixels'] = offset
            result['stop_and_phasing_anchored_full_width'] = measure(
                [abs(reference[i] - pixels[i]) for i in range(width * height)])
    except ValueError as error:
        parser.error(str(error))
    print(json.dumps({'reference': args.reference, 'received': args.received,
                      'stop_source_line': line, 'rows_before_stop': emitted,
                      'transmitted_height_assumed': height, 'anchored_row': row,
                      **result}, sort_keys=True))


if __name__ == '__main__':
    main()

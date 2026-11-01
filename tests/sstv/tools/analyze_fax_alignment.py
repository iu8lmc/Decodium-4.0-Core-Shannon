#!/usr/bin/env python3
"""Report exploratory horizontal FAX alignment; never changes acceptance tests."""
import argparse
import json
import subprocess


def gray(path):
    info = json.loads(subprocess.check_output([
        'ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries',
        'stream=width,height', '-of', 'json', path]))['streams'][0]
    width, height = info['width'], info['height']
    pixels = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', path,
        '-frames:v', '1', '-f', 'rawvideo', '-pix_fmt', 'gray', '-'])
    if len(pixels) != width * height:
        raise ValueError('unexpected grayscale buffer length')
    return width, height, pixels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference')
    parser.add_argument('received')
    parser.add_argument('--max-shift', type=int, default=3)
    args = parser.parse_args()
    width, height, reference = gray(args.reference)
    other_width, other_height, received = gray(args.received)
    if (width, height) != (other_width, other_height):
        raise ValueError('image dimensions differ')
    border = args.max_shift + 1
    if not 0 <= args.max_shift <= 16 or width <= 2 * border:
        raise ValueError('invalid shift bound')
    results = []
    for shift in range(-args.max_shift, args.max_shift + 1):
        errors = sorted(abs(reference[y * width + x] - received[y * width + x + shift])
                        for y in range(height) for x in range(border, width - border))
        results.append({'shift': shift, 'mae': sum(errors) / len(errors),
                        'p95': errors[int(len(errors) * .95)]})
    print(json.dumps({'width': width, 'height': height, 'excluded_border': border,
                      'best': min(results, key=lambda row: row['mae']),
                      'unshifted': results[args.max_shift], 'results': results}, sort_keys=True))


if __name__ == '__main__':
    main()

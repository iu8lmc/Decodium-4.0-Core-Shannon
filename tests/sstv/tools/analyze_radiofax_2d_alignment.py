#!/usr/bin/env python3
"""Diagnostic raster offsets; the direct metric is never shifted or cropped."""
import argparse
import json
import subprocess


def dimensions(path):
    data = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-select_streams',
        'v:0', '-show_entries', 'stream=width,height', '-of', 'json', str(path)]))
    return data['streams'][0]['width'], data['streams'][0]['height']


def gray(path, width, height):
    pixels = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', str(path), '-frames:v', '1',
        '-vf', 'scale={}:{}:flags=area'.format(width, height), '-pix_fmt', 'gray',
        '-f', 'rawvideo', '-'])
    if len(pixels) != width * height:
        raise ValueError('unexpected decoded grayscale buffer size')
    return pixels


def measure(errors):
    errors.sort()
    return {'mae': sum(errors) / len(errors), 'p95': errors[int(len(errors) * .95)],
            'maximum': errors[-1]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('reference')
    parser.add_argument('received')
    parser.add_argument('--max-rows', type=int, default=20)
    parser.add_argument('--max-columns', type=int, default=32)
    args = parser.parse_args()
    width, height = dimensions(args.reference)
    received_width, received_height = dimensions(args.received)
    if (not 0 <= args.max_rows <= 100 or not 0 <= args.max_columns <= 64
            or width <= 2 * args.max_columns or received_height < height + args.max_rows):
        raise ValueError('invalid search bounds or insufficient received raster')
    reference = gray(args.reference, width, height)
    received = gray(args.received, width, received_height)
    direct = measure([abs(reference[y * width + x] - received[y * width + x])
                      for y in range(height) for x in range(width)])
    border = args.max_columns
    def errors(row, column):
        return [abs(reference[y * width + x] - received[(y + row) * width + x + column])
                for y in range(height) for x in range(border, width - border)]
    candidates = []
    for row in range(args.max_rows + 1):
        for column in range(-args.max_columns, args.max_columns + 1):
            values = errors(row, column)
            candidates.append((sum(values), row, column))
    _, row, column = min(candidates)
    print(json.dumps({'reference': args.reference, 'received': args.received,
        'reference_size': [width, height], 'received_size': [received_width, received_height],
        'resized_received_width': width, 'direct_full_raster': direct,
        'search_excluded_border': border, 'best_posthoc': {'row': row, 'column': column,
            **measure(errors(row, column))}}, sort_keys=True))


if __name__ == '__main__':
    main()

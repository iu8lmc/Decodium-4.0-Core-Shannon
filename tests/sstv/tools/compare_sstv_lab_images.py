#!/usr/bin/env python3
"""Compare two lossless lab PNGs using ffmpeg; report component errors."""
import argparse
import json
import statistics
import subprocess
import sys


def read_rgb(path):
    metadata = json.loads(subprocess.check_output([
        "ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
        "stream=width,height", "-of", "json", path,
    ]))
    stream = metadata["streams"][0]
    shape = (stream["width"], stream["height"])
    pixels = subprocess.check_output([
        "ffmpeg", "-v", "error", "-i", path, "-frames:v", "1", "-f",
        "rawvideo", "-pix_fmt", "rgb24", "-",
    ])
    if len(pixels) != shape[0] * shape[1] * 3:
        raise ValueError("unexpected RGB buffer length")
    return shape, pixels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference")
    parser.add_argument("received")
    parser.add_argument("--max-mae", type=float, required=True)
    parser.add_argument("--max-p95", type=int, required=True)
    parser.add_argument("--max-last-blue-error", type=int)
    parser.add_argument("--max-last-column-error", type=int)
    parser.add_argument("--max-first-column-error", type=int)
    args = parser.parse_args()
    if args.max_last_blue_error is not None and not 0 <= args.max_last_blue_error <= 255:
        parser.error("--max-last-blue-error must be between 0 and 255")
    if args.max_last_column_error is not None and not 0 <= args.max_last_column_error <= 255:
        parser.error("--max-last-column-error must be between 0 and 255")
    if args.max_first_column_error is not None and not 0 <= args.max_first_column_error <= 255:
        parser.error("--max-first-column-error must be between 0 and 255")
    first_shape, first = read_rgb(args.reference)
    second_shape, second = read_rgb(args.received)
    if first_shape != second_shape:
        raise ValueError(f"dimensions differ: {first_shape} versus {second_shape}")
    errors = sorted(abs(a - b) for a, b in zip(first, second))
    mae = statistics.mean(errors)
    p95 = errors[int(len(errors) * 0.95)]
    row_bytes = first_shape[0] * 3
    first_column_errors = [abs(first[y * row_bytes + channel]
                               - second[y * row_bytes + channel])
                           for y in range(first_shape[1]) for channel in range(3)]
    last_column_errors = [abs(first[y * row_bytes + row_bytes - 3 + channel]
                              - second[y * row_bytes + row_bytes - 3 + channel])
                          for y in range(first_shape[1]) for channel in range(3)]
    first_column_errors.sort()
    last_column_errors.sort()
    result = {"width": first_shape[0], "height": first_shape[1],
              "mean_absolute_error": mae, "median_error": errors[len(errors) // 2],
              "p95_error": p95, "maximum_error": errors[-1],
              "first_column_maximum_error": max(first_column_errors),
              "last_column_maximum_error": max(last_column_errors),
              "first_column_mean_absolute_error": statistics.mean(first_column_errors),
              "last_column_mean_absolute_error": statistics.mean(last_column_errors),
              "first_column_p95_error": first_column_errors[int(len(first_column_errors) * 0.95)],
              "last_column_p95_error": last_column_errors[int(len(last_column_errors) * 0.95)],
              "max_mae": args.max_mae, "max_p95": args.max_p95}
    if args.max_last_blue_error is not None:
        result.update(last_blue_reference=first[-1], last_blue_received=second[-1],
                      last_blue_error=abs(first[-1] - second[-1]),
                      max_last_blue_error=args.max_last_blue_error)
    if args.max_last_column_error is not None:
        result["max_last_column_error"] = args.max_last_column_error
    if args.max_first_column_error is not None:
        result["max_first_column_error"] = args.max_first_column_error
    print(json.dumps(result, sort_keys=True))
    return 0 if (mae <= args.max_mae and p95 <= args.max_p95
                 and (args.max_last_blue_error is None
                      or result["last_blue_error"] <= args.max_last_blue_error)
                 and (args.max_last_column_error is None
                      or result["last_column_maximum_error"] <= args.max_last_column_error)
                 and (args.max_first_column_error is None
                      or result["first_column_maximum_error"] <= args.max_first_column_error)) else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, IndexError, subprocess.CalledProcessError) as exc:
        print(f"Image comparison failed: {exc}", file=sys.stderr)
        sys.exit(2)

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
    args = parser.parse_args()
    first_shape, first = read_rgb(args.reference)
    second_shape, second = read_rgb(args.received)
    if first_shape != second_shape:
        raise ValueError(f"dimensions differ: {first_shape} versus {second_shape}")
    errors = sorted(abs(a - b) for a, b in zip(first, second))
    mae = statistics.mean(errors)
    p95 = errors[int(len(errors) * 0.95)]
    result = {"width": first_shape[0], "height": first_shape[1],
              "mean_absolute_error": mae, "median_error": errors[len(errors) // 2],
              "p95_error": p95, "maximum_error": errors[-1],
              "max_mae": args.max_mae, "max_p95": args.max_p95}
    print(json.dumps(result, sort_keys=True))
    return 0 if mae <= args.max_mae and p95 <= args.max_p95 else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, IndexError, subprocess.CalledProcessError) as exc:
        print(f"Image comparison failed: {exc}", file=sys.stderr)
        sys.exit(2)

#!/usr/bin/env python3
"""Create a diagnostic capture with shortened FAX480 sync; not a TX implementation."""
import argparse
import array
import math
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('header_sample', type=int, help='estimated 48 kHz header start')
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    if not source.is_file() or not 0 < source.stat().st_size <= 200 * 48000 * 8 or source.stat().st_size % 8:
        raise ValueError('expected bounded 48 kHz stereo float32 capture')
    if args.header_sample < 0:
        raise ValueError('invalid header sample')
    samples = array.array('f')
    samples.frombytes(source.read_bytes())
    if sys.byteorder != 'little':
        samples.byteswap()
    omega = 2 * math.pi * 1200 / 48000
    cosines = {hz: 2 * math.cos(2 * math.pi * hz / 48000)
               for hz in (1200, 1500, 1900, 2300)}
    peak = 0.0
    for line in range(500):
        end = args.header_sample + round((5.002 + (line + 1) * 0.267266) * 48000)
        begin = end - 48
        if begin < 2 or end >= len(samples) // 2:
            raise ValueError('predicted sync outside capture')
        errors = {hz: sum((samples[2*n] + samples[2*(n-2)]
                           - factor * samples[2*(n-1)]) ** 2
                          for n in range(begin + 2, end))
                  for hz, factor in cosines.items()}
        if not all(math.isfinite(value) for value in errors.values()) or min(errors, key=errors.get) != 1200:
            raise ValueError('predicted window is not closest to 1200 Hz')
        for channel in range(2):
            previous = samples[(begin-1)*2+channel]
            before = samples[(begin-2)*2+channel]
            quadrature = (previous * math.cos(omega) - before) / math.sin(omega)
            phase, amplitude = math.atan2(previous, quadrature), math.hypot(previous, quadrature)
            if not math.isfinite(amplitude) or not 0.05 < amplitude < 1:
                raise ValueError('invalid signal amplitude at sync')
            for index in range(48):
                value = amplitude * math.sin(phase + (index+1) * 2 * math.pi * 1500 / 48000)
                samples[(begin+index)*2+channel] = value
                peak = max(peak, abs(value))
    if sys.byteorder != 'little':
        samples.byteswap()
    with args.output.open('xb') as output:
        output.write(samples.tobytes())
    print('prepared', args.output, 'modified sync windows', 500, 'peak', peak)


if __name__ == '__main__':
    main()

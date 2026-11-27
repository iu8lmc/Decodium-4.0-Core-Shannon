#!/usr/bin/env python3
"""Find a VIS 85 header in a 12 kHz mono receiver diagnostic WAV.

This is a coarse laboratory check, not a replacement for the receiver or an
exact reproduction of its floating-point input and streaming state.
"""
import array
import math
import sys
import wave


def vis85_present(samples, rate=12000):
    if rate != 12000:
        raise ValueError('expected 12 kHz')
    # Search only the latest 45 seconds, including the expected header.
    samples = samples[-rate * 45:]
    window = rate // 100
    coefficients = [2.0 * math.cos(2.0 * math.pi * hz / rate)
                    for hz in (1100, 1200, 1300, 1900)]
    tones = []
    for offset in range(0, len(samples) - window + 1, window):
        block = samples[offset:offset + window]
        scores = []
        for coefficient in coefficients:
            a = b = 0.0
            for value in block:
                current = value + coefficient * a - b
                b, a = a, current
            scores.append(a * a + b * b - coefficient * a * b)
        best = max(range(4), key=scores.__getitem__)
        tones.append(best if scores[best] > (window * 300) ** 2 else -1)
    # Symbols are 30 ms. A start bit of 1200 Hz precedes seven VIS bits,
    # parity and a 1200 Hz stop; permit one 10 ms boundary window per symbol.
    expected = (1, 0, 2, 0, 2, 0, 2, 0, 2, 1)
    for first in range(30, len(tones) - 29):
        if sum(t == 3 for t in tones[first-30:first]) < 25:
            continue
        if all(sum(t == expected[symbol] for t in tones[first+3*symbol:first+3*symbol+3]) >= 2
               for symbol in range(10)):
            return True
    return False


def read_pcm(path):
    with wave.open(str(path), 'rb') as wav:
        if (wav.getnchannels(), wav.getframerate(), wav.getsampwidth()) != (1, 12000, 2):
            raise ValueError('expected 12 kHz mono 16-bit PCM WAV')
        pcm = array.array('h')
        pcm.frombytes(wav.readframes(wav.getnframes()))
        if sys.byteorder != 'little':
            pcm.byteswap()
        return pcm


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: analyze_fax480_retained_vis.py recording.wav')
    found = vis85_present(read_pcm(sys.argv[1]))
    print('VIS 85 present' if found else 'VIS 85 not found')
    raise SystemExit(0 if found else 1)

#!/usr/bin/env python3
"""A/B and negative self-controls for the pinned external SSTV adapter."""
import subprocess
import sys
import tempfile
from pathlib import Path

if len(sys.argv) != 2:
    raise SystemExit('expected path to the built Revenant SSTV adapter')
program = Path(sys.argv[1]).resolve(strict=True)
with tempfile.TemporaryDirectory(prefix='decodium-revenant-selftest-') as tmp:
    root = Path(tmp)
    def make_image(variant):
        pixels = bytearray()
        for y in range(120):
            for x in range(128):
                pixels.extend(((x * 2 + variant * 53) % 256,
                               (y * 2 + variant * 71) % 256,
                               ((x // 8 + y // 7 + variant) % 2) * 255))
        return bytes(pixels)
    images = [make_image(0), make_image(1)]
    for variant, pixels in enumerate(images):
        source, audio, output = (root / f'{variant}.{ext}' for ext in ('rgb', 'f32', 'ppm'))
        source.write_bytes(pixels)
        subprocess.run([str(program), 'render', str(source), str(audio), 'AVT 24 QRM'],
                       check=True, timeout=30, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run([str(program), 'decode', str(audio), str(output), 'AVT 24 QRM'],
                       check=True, timeout=30, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        header = b'P6\n128 120\n255\n'
        content = output.read_bytes()
        if not content.startswith(header) or len(content) != len(header) + len(pixels):
            raise RuntimeError('incorrect PPM image boundary')
        received = content[len(header):]
        mean = sum(abs(a - b) for a, b in zip(received, pixels)) / len(pixels)
        if mean >= 5:
            raise RuntimeError(f'self-control image {variant} mean error {mean}')
        crossed = sum(abs(a - b) for a, b in zip(received, images[1 - variant])) / len(pixels)
        if crossed <= 5:
            raise RuntimeError('crossed-image negative control was accepted')
        wrong_mode = subprocess.run([str(program), 'decode', str(audio), str(root / 'wrong.ppm'),
                                     'AVT 24 Narrow QRM'], stdout=subprocess.DEVNULL,
                                    stderr=subprocess.DEVNULL, timeout=30)
        if wrong_mode.returncode != 8 or (root / 'wrong.ppm').exists():
            raise RuntimeError('wrong-mode negative control was accepted')
    for variant in range(2):
        gray = bytes(((x // 3 + y // 4 + variant * 73) % 256)
                     for y in range(480) for x in range(512))
        rgb = bytes(component for pixel in gray for component in (pixel, pixel, pixel))
        source, audio, output = (root / f'fax-{variant}.{ext}' for ext in ('rgb', 'f32', 'pgm'))
        source.write_bytes(rgb)
        subprocess.run([str(program), 'render', str(source), str(audio), 'FAX480'],
                       check=True, timeout=30, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run([str(program), 'decode', str(audio), str(output), 'FAX480'],
                       check=True, timeout=30, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        header = b'P5\n512 480\n255\n'
        content = output.read_bytes()
        if not content.startswith(header) or len(content) != len(header) + len(gray):
            raise RuntimeError('incorrect FAX480 PGM image boundary')
        mean = sum(abs(a - b) for a, b in zip(content[len(header):], gray)) / len(gray)
        if mean >= 8:
            raise RuntimeError(f'FAX480 self-control image {variant} mean error {mean}')
    print('PASS: Revenant AVT and FAX480 A/B self-controls and crossed-image / wrong-mode negatives')

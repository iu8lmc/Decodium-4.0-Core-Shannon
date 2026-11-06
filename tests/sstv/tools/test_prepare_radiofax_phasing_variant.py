#!/usr/bin/env python3
"""A/B byte-preservation and error controls for the phasing diagnostic."""
from pathlib import Path
import tempfile
from prepare_radiofax_phasing_variant import prepare

with tempfile.TemporaryDirectory(prefix='fax-phasing-variant-') as directory:
    root = Path(directory)
    for variant in (0, 1):
        frames = [bytes(((index * 17 + variant * 41 + k) % 256 for k in range(8)))
                  for index in range(80)]
        source, target = root / f'{variant}.f32', root / f'{variant}-out.f32'
        source.write_bytes(b''.join(frames))
        prepare(source, target, 10 + variant, 3, 55 + variant, 5)
        start = 10 + variant
        end = 55 + variant
        assert target.read_bytes() == b''.join(frames[:start] + frames[start + 15:end] + frames[end + 5:])
        try:
            prepare(source, target, start, 3, end, 5)
            raise AssertionError('existing output overwritten')
        except FileExistsError:
            pass
        try:
            prepare(source, root / f'invalid-{variant}', start, 20, end, 5)
            raise AssertionError('overlapping cuts accepted')
        except ValueError:
            pass
        assert not (root / f'invalid-{variant}').exists()
        print('PASS Radiofax phasing copy', variant)

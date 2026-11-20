#!/usr/bin/env python3
"""Check pinned QSSTV-source recordings; these are not live peer tests."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('analyzer', type=Path)
parser.add_argument('recording_directory', type=Path)
parser.add_argument('reference_directory', type=Path)
args = parser.parse_args()
analyzer = args.analyzer.resolve(strict=True)
recordings = args.recording_directory.resolve(strict=True)
references = args.reference_directory.resolve(strict=True)
hashes = {
    'a-baseline.s16': '16ec753e916c818b1536c2c2f48908554acbe9afcba42fb2d0402cf01db261cc',
    'b-baseline.s16': '19b3230426a815abb8186095b404b4083b752b67776f556a48f3d1239fc67620',
    'a-shifted.s16': 'a30c8d93d773197d1f9f6504cac0860dbee3883f5d40cca609fbaa6677cf664c',
    'b-shifted.s16': 'b568300851c7d02c9fbf712fa299a421aa1df32e34861fd8bfb30b56f3f0865a',
    'decoded-a/a_1_drm.jp2': '3568ee18b0530d1707e6c98f5087d4426d594cbb4537abc84c49fc36c15620bc',
    'decoded-b/b_1_drm.jp2': 'afb446f3dd381e7fd099655ccec9489e90c2688099c33edbfc5fbca692f265f6',
}
for name, digest in hashes.items():
    path = (references if name.startswith('decoded-') else recordings) / name
    if not path.is_file() or path.stat().st_size > 16_000_000 or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise SystemExit(f'missing or unexpected fixture: {name}')

for variant in ('a', 'b'):
    reference = references / f'decoded-{variant}/{variant}_1_drm.jp2'
    for capture, mode, tracking, must_match in (
        ('baseline', 'flat-multistage', True, True),
        ('shifted', 'flat-multistage', True, False),
        ('shifted', 'adaptive-early', False, True),
        ('shifted', 'adaptive-early', True, True),
    ):
        env = os.environ.copy()
        for name in ('DECODIUM_HAMDRM_FRAME_TRACKING', 'DECODIUM_HAMDRM_TEMPORAL_PILOTS',
                     'DECODIUM_HAMDRM_TEMPORAL_MERGE', 'DECODIUM_HAMDRM_OBJECT_OUTPUT',
                     'DECODIUM_HAMDRM_TRACE'):
            env.pop(name, None)
        if tracking:
            env['DECODIUM_HAMDRM_FRAME_TRACKING'] = '1'
        cmd = [str(analyzer), str(recordings / f'{variant}-{capture}.s16'), 'A', '2300', '16',
               '0.12', '0.30', str(reference), 'short', mode, 'normal']
        result = subprocess.run(cmd, env=env, capture_output=True, timeout=30)
        if result.returncode not in (0, 1) or b'ingest_errors=0' not in result.stdout:
            raise SystemExit(f'analysis failed: {variant} {capture} {mode}: '
                             + (result.stdout + result.stderr).decode(errors='replace'))
        exact = result.returncode == 0 and b'exact=1' in result.stdout
        if must_match and not exact:
            raise SystemExit(f'expected exact recovery: {variant} {capture} {mode}')
        if must_match:
            other = 'b' if variant == 'a' else 'a'
            crossed = list(cmd)
            crossed[7] = str(references / f'decoded-{other}/{other}_1_drm.jp2')
            negative = subprocess.run(crossed, env=env, capture_output=True, timeout=30)
            if negative.returncode != 1 or b'exact=0' not in negative.stdout or b'ingest_errors=0' not in negative.stdout:
                raise SystemExit(f'crossed-reference control failed: {variant} {capture} {mode}')
        print(f'{variant.upper()} {capture} {mode} tracking={int(tracking)} exact={int(exact)}', flush=True)

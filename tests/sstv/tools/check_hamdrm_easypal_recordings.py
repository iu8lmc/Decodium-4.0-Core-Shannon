#!/usr/bin/env python3
"""Check separately obtained EasyPal-labelled recordings against byte references.

The recordings and references are not distributed by Decodium. Their source
and original transmitter version remain unverified; this is not a live peer.
"""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('analyzer', type=Path)
parser.add_argument('fixture_directory', type=Path)
parser.add_argument('--pilot-sweep', action='store_true',
                    help='also report a diagnostic flat-receiver threshold sweep')
parser.add_argument('--frame-tracking', action='store_true',
                    help='also check experimental timing tracking against both references')
args = parser.parse_args()
binary = args.analyzer.resolve(strict=True)
root = args.fixture_directory.resolve(strict=True)
files = {
    'a.s16': '719635b2c0d8c7b61f70f0df6fe4e2aeedc5ea08e4bfcd2f40375d94d33e133b',
    'b.s16': '0460d077046d670f69700dd950bd5f7f2e127294d2c471e4eb737422c9cdc0b9',
    'easypal-a_1_drm.jp2': '28b0ceec19c4f068c6363fa34dbd896e16f830dbf52a51932d808ed7f03b51e6',
    'easypal-b-unpadded.jp2': 'da0e069f3ae3d11a95fe64cce8aa528c87c8b82f1b8b7dfea8d343ada8c8b667',
}
for name, expected_hash in files.items():
    path = root / name
    if (not path.is_file() or not 0 < path.stat().st_size <= 16_000_000
            or hashlib.sha256(path.read_bytes()).hexdigest() != expected_hash):
        raise SystemExit(f'missing or unexpected fixture: {name}')

profiles = {'a': ('B', '2500', '16'), 'b': ('E', '2300', '4')}
references = {'a': 'easypal-a_1_drm.jp2', 'b': 'easypal-b-unpadded.jp2'}
for variant in ('a', 'b'):
    base = [str(binary), str(root / f'{variant}.s16'), *profiles[variant], '0.12', '0.30']
    env = os.environ.copy()
    for name in ('DECODIUM_HAMDRM_TEMPORAL_MERGE', 'DECODIUM_HAMDRM_TEMPORAL_PILOTS',
                 'DECODIUM_HAMDRM_FRAME_TRACKING', 'DECODIUM_HAMDRM_TRACE',
                 'DECODIUM_HAMDRM_OBJECT_OUTPUT'):
        env.pop(name, None)
    standard = subprocess.run(base + [str(root / references[variant]), 'long', 'flat-multistage', 'high'],
                              env=env, capture_output=True, timeout=30)
    if standard.returncode not in (0, 1) or (variant == 'b' and standard.returncode != 0):
        raise SystemExit(f'standard {variant} failed unexpectedly: {standard.stderr.decode(errors="replace")}')
    print(f'{variant.upper()} standard exact={int(standard.returncode == 0)}', flush=True)
    if args.frame_tracking:
        tracked_env = env.copy()
        tracked_env['DECODIUM_HAMDRM_FRAME_TRACKING'] = '1'
        for reference_variant in (variant, 'b' if variant == 'a' else 'a'):
            result = subprocess.run(base + [str(root / references[reference_variant]),
                                             'long', 'flat-multistage', 'high'],
                                    env=tracked_env, capture_output=True, timeout=30)
            expected_code = 0 if reference_variant == variant else 1
            marker = b'exact=1' if expected_code == 0 else b'exact=0'
            if result.returncode != expected_code or marker not in result.stdout or b'ingest_errors=0' not in result.stdout:
                raise SystemExit(f'tracking {variant}/{reference_variant} failed: '
                                 + (result.stdout + result.stderr).decode(errors='replace'))
        print(f'{variant.upper()} tracking exact and crossed-reference negative passed', flush=True)
    if args.pilot_sweep:
        # Diagnostic only: these limits are not applied to production RX.
        for limit in ('0.12', '0.20', '0.30', '0.50', '1.0'):
            sweep = [*base]
            sweep[-2:] = [limit, '1.0']
            probe = subprocess.run(sweep + [str(root / references[variant]), 'long', 'flat-multistage', 'high'],
                                   env=env, capture_output=True, timeout=30)
            if probe.returncode not in (0, 1) or b'ingest_errors=0' not in probe.stdout:
                raise SystemExit(f'pilot diagnostic {variant}/{limit} did not complete')
            summary = probe.stdout.decode(errors='replace').splitlines()[-1]
            print(f'{variant.upper()} pilot_limit={limit} {summary}', flush=True)
    env['DECODIUM_HAMDRM_TEMPORAL_MERGE'] = '1'
    for reference_variant in (variant, 'b' if variant == 'a' else 'a'):
        result = subprocess.run(base + [str(root / references[reference_variant]),
                                       'long', 'adaptive-multistage', 'high'],
                                env=env, capture_output=True, timeout=30)
        expected_code = 0 if reference_variant == variant else 1
        marker = b'exact=1' if expected_code == 0 else b'exact=0'
        if result.returncode != expected_code or marker not in result.stdout or b'ingest_errors=0' not in result.stdout:
            raise SystemExit(f'adaptive {variant}/{reference_variant} failed: '
                             + (result.stdout + result.stderr).decode(errors='replace'))
    print(f'{variant.upper()} adaptive exact and crossed-reference negative passed', flush=True)

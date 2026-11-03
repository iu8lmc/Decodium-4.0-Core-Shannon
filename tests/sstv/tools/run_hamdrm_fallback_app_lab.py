#!/usr/bin/env python3
"""A/B CRC-gated HAMDRM packet retry in the macOS BlackHole application lab."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ('short', 'long'):
        raise SystemExit('expected short or long')
    depth = sys.argv[1]
    repo = Path.cwd()
    exporter = (repo / 'build/tests/sstv/export_hamdrm_fallback_fixture').resolve(strict=True)
    fixture = (repo / 'tests/sstv/fixtures/qsstv-image-a.jpg').resolve(strict=True)
    replay = repo / 'tests/sstv/tools/run_qsstv_exp_drm_app_lab.py'
    root = Path(tempfile.mkdtemp(prefix='decodium-hamdrm-fallback-ab-'))
    source, repeated, wave = root / 'source.s16', root / 'repeat.s16', root / 'repeat.wav'
    subprocess.run([str(exporter), depth, str(source), '48000', '0.27'], check=True, timeout=30)
    with repeated.open('xb') as output:
        for _ in range(8 if depth == 'short' else 5):
            output.write(source.read_bytes())
            output.write(bytes(48000))  # 0.5 s of 48 kHz mono s16 silence
    subprocess.run(['ffmpeg', '-v', 'error', '-f', 's16le', '-ar', '48000', '-ac', '1',
                    '-i', str(repeated), '-ac', '2', '-n', str(wave)], check=True, timeout=30)
    for enabled in (False, True):
        env = os.environ.copy()
        for key in ('DECODIUM_LAB_HAMDRM_FLAT_FALLBACK', 'DECODIUM_LAB_EXPECT_BODY_GROUPS',
                    'DECODIUM_LAB_EXPECT_FALLBACK_GROUPS'):
            env.pop(key, None)
        env.update(DECODIUM_LAB_HAMDRM_SELECTIVE='1', DECODIUM_HAMDRM_RX_TRACE='1',
                   DECODIUM_LAB_HAMDRM_DISABLE_MULTISTAGE='1',
                   DECODIUM_HAMDRM_LAB_PROFILE=f'hamdrm-A-2500-normal-16-QAM-{depth}',
                   DECODIUM_HAMDRM_LAB_DURATION_MS='30000' if depth == 'short' else '35000')
        if enabled:
            env['DECODIUM_LAB_HAMDRM_FLAT_FALLBACK'] = '1'
            env['DECODIUM_LAB_EXPECT_FALLBACK_GROUPS'] = '1'
        else:
            env['DECODIUM_LAB_EXPECT_BODY_GROUPS'] = '1'
        result = subprocess.run([sys.executable, str(replay), str(wave), str(fixture)],
                                env=env, capture_output=True, text=True, timeout=90)
        (root / ('enabled.log' if enabled else 'disabled.log')).write_text(result.stdout + result.stderr)
        if enabled:
            if result.returncode or 'fallback required True' not in result.stdout:
                raise RuntimeError('enabled receiver did not pass; artifacts ' + str(root))
            print('PASS enabled', result.stdout.strip(), flush=True)
        else:
            match = re.search(r'^RuntimeError: two synthetic body groups were not delivered; artifacts (\S+)$',
                              result.stderr, re.MULTILINE)
            if result.returncode == 0 or not match:
                raise RuntimeError('disabled control did not fail as expected; artifacts ' + str(root))
            rxlog = (Path(match.group(1)) / 'rx.log').read_text(errors='replace')
            rows = [json.loads(line.split(' RX diagnostics ', 1)[1]) for line in rxlog.splitlines()
                    if '[LAB][SSTV] RX diagnostics ' in line]
            if ('[LAB][HAMDRM] multistage packet retry disabled for comparison' not in rxlog
                or not rows or max(row.get('hamdrmDeliveredGroups', 0) for row in rows) != 0
                or 'HAMDRM channel CRC16 mismatch' not in rxlog):
                raise RuntimeError('disabled control failed for a different reason; artifacts ' + str(root))
            print('PASS disabled control; artifacts', match.group(1), flush=True)
    print('PASS fallback A/B', depth, 'artifacts', root, flush=True)


if __name__ == '__main__':
    main()

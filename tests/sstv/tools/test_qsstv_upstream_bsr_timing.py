#!/usr/bin/env python3
"""Check two independently captured upstream QSSTV BSR waveforms."""
from pathlib import Path
import re
import subprocess
import sys

binary = Path(sys.argv[1]).resolve(strict=True)
fixtures = Path(sys.argv[2]).resolve(strict=True)
for variant in ('a', 'b'):
    other = 'b' if variant == 'a' else 'a'
    pcm = fixtures / f'qsstv-upstream-live-bsr-{variant}.s16'
    expected = fixtures / f'qsstv-upstream-live-bsr-{variant}.txt'
    wrong = fixtures / f'qsstv-upstream-live-bsr-{other}.txt'
    args = [str(binary), str(pcm), 'E', '2500', '16', '0.12', '0.30']
    standard = subprocess.run(args + [str(expected), 'long', 'flat', 'high'],
                              capture_output=True, timeout=30)
    assert standard.returncode == 0 and b'exact=1' in standard.stdout, standard.stdout + standard.stderr
    assert b'ingest_errors=0' in standard.stdout, standard.stdout + standard.stderr
    wrong_standard = subprocess.run(args + [str(wrong), 'long', 'flat', 'high'],
                                    capture_output=True, timeout=30)
    assert wrong_standard.returncode == 1 and b'exact=0' in wrong_standard.stdout, wrong_standard.stdout + wrong_standard.stderr
    result = subprocess.run(args + [str(expected), 'long', 'flat-timing-hybrid', 'high'],
                            capture_output=True, timeout=30)
    assert result.returncode == 0 and b'exact=1' in result.stdout, result.stdout + result.stderr
    assert b'ingest_errors=0' in result.stdout, result.stdout + result.stderr
    match = re.search(rb'hybrid_secondary_groups=(\d+)', result.stdout)
    assert match and int(match.group(1)) > 0, result.stdout + result.stderr
    negative = subprocess.run(args + [str(wrong), 'long', 'flat-timing-hybrid', 'high'],
                              capture_output=True, timeout=30)
    assert negative.returncode == 1 and b'exact=0' in negative.stdout, negative.stdout + negative.stderr
    print(f'PASS upstream QSSTV {variant.upper()} timing and wrong-reference control')

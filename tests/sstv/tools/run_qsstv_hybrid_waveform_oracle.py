#!/usr/bin/env python3
"""Compare A/B Decodium WAVs with the instrumented QSSTV hybrid receiver.

Binary must be built from revision a845842 using the accompanying
qsstv-experimental-hybrid-oracle.patch. The patch disables FTP when the
test-only environment flag is set, and prints only a synthetic-data match.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

if len(sys.argv) != 4:
    raise SystemExit('usage: run_qsstv_hybrid_waveform_oracle.py PATCHED_QSSTV A.wav B.wav')
binary, a_wav, b_wav = (Path(arg).resolve(strict=True) for arg in sys.argv[1:])
env = os.environ.copy()
env.update(QT_QPA_PLATFORM='offscreen', DECODIUM_QSSTV_HYBRID_ORACLE='1')
with tempfile.TemporaryDirectory(prefix='decodium-qsstv-hybrid-oracle-') as temporary:
    for variant, wav in (('a', a_wav), ('b', b_wav)):
        process = subprocess.run([str(binary), '--batch', '--drm', '--timeout', '60',
                                  '--out-dir', temporary, str(wav)], env=env,
                                 text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 timeout=90)
        marker = '[LAB][QSSTV] hybrid decoded A=1 B=0' if variant == 'a' else '[LAB][QSSTV] hybrid decoded A=0 B=1'
        # The diagnostic deliberately does not fetch or create an image.
        if process.returncode != 1 or marker not in process.stdout or 'no image decoded' not in process.stdout:
            raise SystemExit('QSSTV hybrid comparison failed for ' + variant + ':\n' + process.stdout)
        print('PASS instrumented QSSTV hybrid waveform', variant, flush=True)

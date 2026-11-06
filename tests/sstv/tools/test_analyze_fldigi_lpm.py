#!/usr/bin/env python3
"""Exercise FLdigi speed diagnostics including mixed and missing evidence."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

tool = Path(__file__).with_name('analyze_fldigi_lpm.py')
with tempfile.TemporaryDirectory(prefix='decodium-fldigi-lpm-test-') as temporary:
    log = Path(temporary) / 'status.log'
    for speed in (90, 120):
        observed = f'Decoding phasing line, lpm = {speed} count=4\n'
        transition = f'm_phase_lines = 4, State = Receiving, m_lpm_img = {speed}.000000\n'
        log.write_text(observed + transition + 'Receiving Apt stop frequency: 450 Hz\n')
        command = [sys.executable, str(tool), str(log), '--expected-lpm', str(speed)]
        passed = subprocess.run(command, capture_output=True, text=True)
        assert passed.returncode == 0 and json.loads(passed.stdout)['apt_stop']
        mismatch = 120 if speed == 90 else 90
        log.write_text(observed + transition + f'm_phase_lines = 4, m_lpm_img = {mismatch}.0\n')
        assert subprocess.run(command, capture_output=True).returncode == 1
        log.write_text(observed)
        assert subprocess.run(command, capture_output=True).returncode == 2
print('PASS FLdigi LPM A/B, mixed-speed and missing-evidence controls')

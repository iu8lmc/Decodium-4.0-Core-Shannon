#!/usr/bin/env python3
"""Check invalid FAX480 lab invocations before Qt workers start."""
import subprocess
import sys

for args in (
    ['--lab-sstv-fax480-tx', 'dayton-480'],
    ['--lab-sstv-fax480-rx', 'dayton-480'],
    ['--test-mode', '--lab-sstv-fax480-rx=invalid'],
    ['--test-mode', '--lab-sstv-fax480-rx=qsstv-500', '--lab-sstv-fax-rx=576,512,500'],
    ['--test-mode', '--lab-sstv-fax480-tx=invalid'],
    ['--test-mode', '--lab-sstv-fax480-tx'],
    ['--test-mode', '--lab-sstv-fax480-tx=qsstv-500', '--lab-sstv-fax-tx=576,120'],
):
    result = subprocess.run([sys.argv[1], *args], capture_output=True, timeout=5)
    if result.returncode != 2 or b'FAX480 lab ' not in result.stderr:
        raise RuntimeError('invalid FAX480 lab profile was not rejected promptly')
print('PASS: invalid FAX480 lab options rejected before startup')

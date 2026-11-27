#!/usr/bin/env python3
"""Check lab CLI validation and clean application CLI shutdown."""
import os
import subprocess
import sys

env = os.environ.copy()
env['QT_QPA_PLATFORM'] = 'offscreen'
env['QT_QUICK_BACKEND'] = 'software'
for args in (
        ['--lab-hamdrm-profile', 'p'],
        ['--test-mode', '--lab-hamdrm-profile=p'],
        ['--test-mode', '--lab-hamdrm', '--lab-hamdrm-profile']):
    result = subprocess.run([sys.argv[1], *args], env=env, capture_output=True, timeout=5)
    if result.returncode != 2 or b'HAMDRM lab profile requires' not in result.stderr:
        raise RuntimeError('invalid profile invocation was not rejected promptly')
print('PASS: invalid HAMDRM lab profiles rejected before startup')

for option, expected in (
        ('--help', b'--lab-hamdrm'),
        ('--version', b'Decodium ')):
    result = subprocess.run([sys.argv[1], option], env=env, capture_output=True, timeout=10)
    if result.returncode != 0 or expected not in result.stdout or b'QThread: Destroyed' in result.stderr:
        raise RuntimeError(f'{option} did not finish cleanly with the expected output')
    if option == '--help':
        for flag in (b'--lab-no-cat', b'--lab-no-monitor', b'--lab-audio-input',
                     b'--lab-audio-output', b'--lab-sstv-rx-ms', b'--lab-sstv-tx-ms',
                     b'--lab-sstv-tx-wait-file', b'--lab-quit-ms'):
            if flag not in result.stdout:
                raise RuntimeError(f'help omits lab option {flag!r}')

result = subprocess.run([sys.argv[1], '--unknown-decodium-test-option'],
                        env=env, capture_output=True, timeout=10)
if result.returncode == 0 or b'Command line error:' not in result.stderr or b'QThread: Destroyed' in result.stderr:
    raise RuntimeError('invalid option did not finish cleanly with a command-line error')
print('PASS: help, version and invalid option exit cleanly')

result = subprocess.run([sys.argv[1], '--lab-hamdrm'], env=env,
                        capture_output=True, timeout=10)
if result.returncode == 0 or b'HAMDRM lab mode requires --test-mode' not in result.stderr or b'QThread: Destroyed' in result.stderr:
    raise RuntimeError('missing test mode did not exit cleanly')
print('PASS: post-parser validation exits cleanly')

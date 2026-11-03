#!/usr/bin/env python3
"""A/B inventory controls; extra tests allowed, missing gates rejected."""
import json
import re
import subprocess
import sys
from pathlib import Path
from check_windows_share_tests import REQUIRED

script = Path(__file__).with_name('check_windows_share_tests.py')
cmake = Path(__file__).resolve().parents[1].joinpath('CMakeLists.txt').read_text()
block = cmake.split('if (WIN32 AND TARGET decodium_sstv_share_aead)', 1)[1].split('unset (_share_windows_python)', 1)[0]
declared = set()
for name in re.findall(r'add_test\s*\(NAME\s+(test_sstv_share_windows_[^\s)]+)', block):
    if '${_share_variant}' in name:
        declared.update(name.replace('${_share_variant}', variant) for variant in ('a', 'b'))
    else:
        declared.add(name)
assert REQUIRED <= declared, 'inventory gate expects tests not declared in CMake: ' + repr(REQUIRED - declared)
for variant in ('a', 'b'):
    names = sorted(REQUIRED | {'test_sstv_share_windows_extra_' + variant})
    payload = {'tests': [{'name': name} for name in names]}
    good = subprocess.run([sys.executable, str(script)], input=json.dumps(payload), text=True, capture_output=True)
    assert good.returncode == 0, good.stderr
    removed = 'test_sstv_share_windows_replay_' + variant
    payload['tests'] = [item for item in payload['tests'] if item['name'] != removed]
    bad = subprocess.run([sys.executable, str(script)], input=json.dumps(payload), text=True, capture_output=True)
    assert bad.returncode != 0 and removed in bad.stderr, bad.stderr
for invalid in ('{}', '{"tests":[{}]}', 'not-json'):
    result = subprocess.run([sys.executable, str(script)], input=invalid, text=True, capture_output=True)
    assert result.returncode != 0, invalid
print('PASS Windows sharing inventory A/B, missing and malformed controls')

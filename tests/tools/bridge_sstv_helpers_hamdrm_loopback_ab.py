#!/usr/bin/env python3
"""Compare HAMDRM TX in two isolated apps over virtual audio."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
main = root / 'src/bridge/DecodiumBridgeSstv.cpp'
module = root / 'src/bridge/DecodiumBridgeSstvHelpers.cpp'
original_main, original_module = main.read_text(), module.read_text()
fixture = (root / 'tests/bridge_refactor_fixtures/sstv_helpers_before.inc').read_text()
assert original_module.endswith(fixture)
start=original_main.index('#if DECODIUM_HAS_SSTV\nusing decodium::sstv::SstvAudioSourceKind;'); end=original_main.index('void DecodiumBridge::initialiseSstvRuntime()',start)
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
env = os.environ.copy()
env['DECODIUM_FORCE_STANDALONE_UDP'] = '1'
try:
    for label in ('before', 'after'):
        if label == 'before':
            main.write_text(original_main[:start] + fixture + original_main[end:])
            module.write_text('#include "DecodiumBridge.h"\n')
        else:
            main.write_text(original_main)
            module.write_text(original_module)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        result = subprocess.run(['sandbox-exec', '-p', policy, sys.executable, 'tests/sstv/tools/run_hamdrm_two_app_lab.py', 'a'], cwd=root, env=env, capture_output=True, text=True, timeout=100)
        print(label, result.stdout[-2000:], result.stderr[-1000:], flush=True)
        match = re.search(r'artifacts\s+(/\S+)', result.stdout + result.stderr)
        if result.returncode or not match or 'PASS two Decodium apps' not in result.stdout:
            raise RuntimeError('HAMDRM two-session virtual-audio test failed')
        shutil.rmtree(Path(match.group(1)))
finally:
    main.write_text(original_main)
    module.write_text(original_module)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

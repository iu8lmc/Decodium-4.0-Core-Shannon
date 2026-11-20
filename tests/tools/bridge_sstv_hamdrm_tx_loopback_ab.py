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
module = root / 'src/bridge/DecodiumBridgeSstvHamDrmTx.cpp'
original_main, original_module = main.read_text(), module.read_text()
legacy = root / 'src/bridge/DecodiumBridgeLegacyInit.cpp'
legacy_original = legacy.read_text()
legacy_macro = '#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled'
assert legacy_original.count(legacy_macro) == 1
fixture = (root / 'tests/bridge_refactor_fixtures/sstv_hamdrm_tx_before.inc').read_text()
assert original_module.endswith(fixture.replace('sstvOutputMatchesConfiguredSelection(', 'decodiumSstvOutputMatchesConfiguredSelection('))
anchor = 'void DecodiumBridge::initialiseSstvRuntime()'
assert original_main.count(anchor) == 1
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
env = os.environ.copy()
env['DECODIUM_FORCE_STANDALONE_UDP'] = '1'
try:
    legacy.write_text(legacy_original.replace(legacy_macro, '#define embeddedLegacyRigControlEnabledForBackend(...) false', 1))
    for label in ('before', 'after'):
        if label == 'before':
            before = fixture.replace('sstvOutputMatchesConfiguredSelection(', 'decodiumSstvOutputMatchesConfiguredSelection(')
            decl = 'bool decodiumSstvOutputMatchesConfiguredSelection(const QString&, const QString&, bool, const QAudioDevice&);\n'
            main.write_text(original_main.replace(anchor, decl + before + anchor, 1))
            module.write_text('#include "DecodiumBridge.h"\n')
        else:
            main.write_text(original_main)
            module.write_text(original_module)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True)
        result = subprocess.run(['sandbox-exec', '-p', policy, sys.executable, 'tests/sstv/tools/run_hamdrm_two_app_lab.py', 'a'], cwd=root, env=env, capture_output=True, text=True, timeout=100)
        print(label, result.stdout[-2000:], result.stderr[-1000:], flush=True)
        match = re.search(r'artifacts\s+(/\S+)', result.stdout + result.stderr)
        if result.returncode or not match or 'PASS two Decodium apps' not in result.stdout:
            raise RuntimeError('HAMDRM two-session virtual-audio test failed')
        shutil.rmtree(Path(match.group(1)))
finally:
    legacy.write_text(legacy_original)
    main.write_text(original_main)
    module.write_text(original_module)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

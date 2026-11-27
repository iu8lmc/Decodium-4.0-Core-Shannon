#!/usr/bin/env python3
"""Compare startup timer order with matched legacy RX virtual-audio stimuli."""
import json
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
source = root / 'src/bridge/DecodiumBridgeLegacyInit.cpp'
original = source.read_text()
boot = root / 'build/qml/decodium/BootLoader.qml'
original_boot = boot.read_text()
boot_marker = '    Component.onCompleted: {'
assert original_boot.count(boot_marker) == 1
baseline = subprocess.check_output(['git', 'show', 'v1.0.737:src/bridge/DecodiumBridge.cpp'], cwd=root, text=True)
old_start = baseline.index('        QTimer::singleShot(3000, this, [this]() {', baseline.index('// Su Mac, crea il legacy backend'))
old_end = baseline.index('\n        });', old_start) + len('\n        });')
old_timer = baseline[old_start:old_end]
rig_macro = '#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled'
assert original.count(rig_macro) == 1
guarded = original.replace(rig_macro, '#define embeddedLegacyRigControlEnabledForBackend(...) false', 1)
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
try:
    results = []
    selected = os.environ.get('DECODIUM_LAB_STARTUP_VARIANT')
    if selected not in (None, 'previous-order', 'current-order'):
        raise ValueError('invalid startup variant')
    for label in ((selected,) if selected else ('previous-order', 'current-order')):
        variant = guarded
        if label == 'previous-order':
            start = guarded.index('        // The timer starts during construction')
            end = guarded.index('\n    }\n#endif', start)
            variant = guarded[:start] + old_timer + guarded[end:]
        source.write_text(variant)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        # The build may refresh QML files, so install the fixture after building.
        boot.write_text(original_boot.replace(boot_marker, boot_marker + '\n        console.log("[LAB-BOOT-STALL] begin"); var stallUntil=Date.now()+4000; while(Date.now()<stallUntil) {} console.log("[LAB-BOOT-STALL] end");\n', 1))
        env=os.environ.copy();env['DECODIUM_LAB_EXPECT_MAIN_FIRST']='1' if label=='current-order' else '0'
        run = subprocess.run(['sandbox-exec', '-p', policy, 'python3', 'tests/tools/bridge_legacydelta_pair_stall_lab.py'], cwd=root, env=env, text=True, capture_output=True, timeout=135)
        if run.returncode:
            raise RuntimeError(label + ': ' + run.stdout[-10000:] + run.stderr[-2000:])
        value = json.loads(run.stdout.strip().splitlines()[-1])
        if value != {'found': True, 'legacy': True, 'tx': False, 'main_first': label=='current-order'}:
            raise RuntimeError(label + ': unexpected ' + repr(value))
        results.append(value); print(label, json.dumps(value, sort_keys=True), flush=True)
    if not all(v['found'] and v['legacy'] and not v['tx'] for v in results): raise RuntimeError('decode not observed')
finally:
    source.write_text(original)
    boot.write_text(original_boot)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

#!/usr/bin/env python3
"""Compare actual virtual-device selection before and after identity extraction."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
main = root / 'src/bridge/DecodiumBridge.cpp'
module = root / 'src/bridge/DecodiumBridgeSettingsHelpers.cpp'
original_main, original_module = main.read_text(), module.read_text()
fixture = (root / 'tests/bridge_refactor_fixtures/settings_helpers_before.inc').read_text()
assert original_module.endswith(fixture)
marker = 'using decodium::decode_message::stripLegacyDecodeAppendage;'
assert original_main.count(marker) == 1
outputs = {}
try:
    for label in ('before', 'after'):
        if label == 'before':
            main.write_text(original_main.replace(marker, fixture + '\n' + marker, 1))
            module.write_text('#include <QString>\n')
        else:
            main.write_text(original_main)
            module.write_text(original_module)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        outputs[label] = []
        for variant in ('a', 'b'):
            result = subprocess.check_output([sys.executable, str(root / 'tests/tools/bridge_generic_settings_runtime_lab.py'), '--variant', variant], cwd=root, text=True, timeout=130)
            value = json.loads(result.strip().splitlines()[-1])
            outputs[label].append(value)
            print(label, variant, json.dumps(value, sort_keys=True), flush=True)
    assert outputs['before'] == outputs['after'], outputs
finally:
    main.write_text(original_main)
    module.write_text(original_module)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

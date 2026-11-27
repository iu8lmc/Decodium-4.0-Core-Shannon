#!/usr/bin/env python3
"""Compare actual virtual-device selection before and after identity extraction."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
main = root / 'src/bridge/DecodiumBridge.cpp'
module = root / 'src/bridge/DecodiumBridgeDestructor.cpp'
original_main, original_module = main.read_text(), module.read_text()
fixture = (root / 'tests/bridge_refactor_fixtures/bridge_destructor_before.inc').read_text()
assert original_module.endswith(fixture.replace('activeCatSetPtt(this, m_hamlibCat, m_catBackend, false,', 'decodiumBridgeSharedActiveCatSetPtt(this, m_hamlibCat, m_catBackend, false,'))
marker = '// BRIDGE_SERVICES_INSERTION_POINT'
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
        for variant in ('a',):
            result = subprocess.check_output(['sandbox-exec', '-p', '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))', sys.executable, str(root / 'tests/tools/bridge_active_decode_exit_lab.py')], cwd=root, text=True, timeout=130)
            value = json.loads(result.strip().splitlines()[-1])
            outputs[label].append(value)
            print(label, variant, json.dumps(value, sort_keys=True), flush=True)
    assert outputs['before'] == outputs['after'], outputs
finally:
    main.write_text(original_main)
    module.write_text(original_module)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

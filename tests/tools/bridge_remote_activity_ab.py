#!/usr/bin/env python3
"""Compare release/current remote activity methods with a local fake server."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
baseline = subprocess.check_output(['git', 'show', 'v1.0.737:src/bridge/DecodiumBridge.cpp'], cwd=root, text=True)
start = 'void DecodiumBridge::clearRemoteActivityCache('
end = 'QVariantMap DecodiumBridge::worldClockSnapshot('
old = baseline[baseline.index(start):baseline.index(end, baseline.index(start))]
module = (root / 'src/bridge/DecodiumBridgeRemoteActivity.cpp').read_text()
new = module[module.index(start):].replace('decodiumBridgeNormalizeRemoteActivityTime(', 'normalizeUtcDisplayToken(')
if old.strip() != new.strip():
    raise RuntimeError('remote activity bodies differ from release beyond normalizer forwarding')
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
outputs = []
with tempfile.TemporaryDirectory(prefix='decodium-remote-activity-ab-') as tmp:
    directory = Path(tmp)
    for label, body in (('release', old), ('current', new)):
        (directory / 'bridge_refactor_remote_activity.inc').write_text(body)
        binary = directory / label
        subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) + ['-std=c++17', '-I' + str(directory), str(root / 'tests/bridge_refactor_remote_activity_harness.cpp'), *flags, '-o', str(binary)], cwd=root, check=True)
        output = subprocess.check_output([str(binary)], cwd=root, text=True)
        outputs.append(output)
        print(label + '\n' + output, end='')
if outputs[0] != outputs[1]:
    raise RuntimeError('remote activity results differ')

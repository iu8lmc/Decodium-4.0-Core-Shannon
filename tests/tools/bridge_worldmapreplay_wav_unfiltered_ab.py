#!/usr/bin/env python3
"""Compare a WAV-derived world-map replay using release and current bodies."""
import os
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
module = root / 'src/bridge/DecodiumBridgeWorldMapReplay.cpp'
original = module.read_text()
start = 'void DecodiumBridge::replayWorldMapFeed()'
end = 'void DecodiumBridge::processMapContactClick('
baseline = subprocess.check_output(['git', 'show', 'v1.0.737:src/bridge/DecodiumBridge.cpp'], cwd=root, text=True)
old = baseline[baseline.index(start):baseline.index(end, baseline.index(start))]
old = old.replace('bridgeLog(', 'decodiumBridgeAudioLevelLog(')
prefix = original[:original.index(start)]
if (prefix + old).strip() != original.strip():
    raise RuntimeError('release and current replay code differ beyond logging forwarder')
environment = os.environ.copy()
environment.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
try:
    results = []
    for label, source in (('release', prefix + old), ('current', original)):
        module.write_text(source)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        result = subprocess.run(['sandbox-exec', '-p', policy, 'python3', 'tests/tools/bridge_worldmapreplay_wav_unfiltered_lab.py'], cwd=root, env=environment, capture_output=True, text=True, timeout=100)
        if result.returncode:
            raise RuntimeError(label + ': ' + result.stdout[-8000:] + result.stderr[-2000:])
        results.append(result.stdout.strip())
        print(label, result.stdout.strip(), flush=True)
    if results[0] != results[1]:
        raise RuntimeError('application replay results differ')
finally:
    module.write_text(original)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

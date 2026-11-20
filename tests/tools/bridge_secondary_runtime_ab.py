#!/usr/bin/env python3
"""Compare decoded secondary routing in isolated release/current applications."""
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
module = root / 'src/bridge/DecodiumBridgeDecodeSecondary.cpp'
original = module.read_text()
baseline = subprocess.check_output(['git', 'show', 'v1.0.737:src/bridge/DecodiumBridge.cpp'], cwd=root, text=True)
def section(text, start, end):
    i = text.index(start)
    return text[i:text.index(end, i)]
start = 'void DecodiumBridge::queueLegacyDecodeSecondaryWork('
old = (section(baseline, start, 'void DecodiumBridge::resetNativeDecodeDedupIndex(')
       + section(baseline, 'void DecodiumBridge::queueNativeDecodeSecondaryWork(', 'void DecodiumBridge::invalidateLegacyDecodeModelDeltaFastPath('))
for old_name, new_name in (('decodeMirrorEntryKey(', 'decodiumBridgeMirrorEntryKey('),
                           ('secondsFromUtcDisplayToken(', 'decodiumBridgeSecondsFromUtcDisplayToken('),
                           ('bridgeLog(', 'decodiumBridgeSecondaryDecodeLog(')):
    old = old.replace(old_name, new_name)
prefix = original[:original.index(start)]
if (prefix + old).strip() != original.strip():
    raise RuntimeError('secondary methods differ from release beyond helper forwards')
try:
    results = []
    for label, body in (('release', prefix + old), ('current', original)):
        module.write_text(body)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        result = subprocess.run(['python3', 'tests/tools/bridge_secondary_runtime_lab.py'], cwd=root, capture_output=True, text=True, timeout=110)
        if result.returncode:
            raise RuntimeError(label + ': ' + result.stdout[-10000:] + result.stderr[-3000:])
        results.append(result.stdout.strip()); print(label, results[-1], flush=True)
    if results[0] != results[1]:
        raise RuntimeError('secondary application outputs differ')
finally:
    module.write_text(original)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

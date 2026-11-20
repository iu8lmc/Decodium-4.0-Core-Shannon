#!/usr/bin/env python3
"""Exercise real bridge QML properties with isolated test settings and no CAT.

Temporarily instruments the *build copy* of Main.qml; never run alongside other
Decodium build/test processes, and rebuild the QML sync target after a crash.
"""
import argparse
import os
import secrets
import shutil
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CASES = {
    'A': {'ft2SignoffRetryCap': 1, 'ft4SignoffRetryCap': 2, 'ft8SignoffRetryCap': 3,
          'ftxImmediateClickTx': True, 'ft2LogRr73OnPartnerLeft': True,
          'ftxWeakSignoffBoost': True, 'ftxWeakSnrThreshold': -29, 'ftxWeakSignoffBonus': 1,
          'ft2PostLogReengageGuard': True, 'ft2PostLogReengageMax': 0,
          'txWatchdogLogOnClose': True, 'maxCallerRetries': 2},
    'B': {'ft2SignoffRetryCap': 8, 'ft4SignoffRetryCap': 7, 'ft8SignoffRetryCap': 6,
          'ftxImmediateClickTx': False, 'ft2LogRr73OnPartnerLeft': False,
          'ftxWeakSignoffBoost': False, 'ftxWeakSnrThreshold': -6, 'ftxWeakSignoffBonus': 6,
          'ft2PostLogReengageGuard': False, 'ft2PostLogReengageMax': 5,
          'txWatchdogLogOnClose': False, 'maxCallerRetries': 99},
}

# Additional FT2/FT8 toggles, including the caller-queue scoring switch.
for key in ['callerRetriesAlwaysHard', 'ft2TransitionCensus', 'ft2AdaptiveTxGates', 'ft2ManualOneShotEnabled', 'ft8FastSequence', 'ft2ConservativeTiming', 'ft2FullDecodeInAutoCq', 'callerScoring', 'ft2QuickGiveUpStrong', 'ft2AdaptiveDecode', 'ft2NarrowAsyncDecode', 'ft2ApHashCache', 'ft2AsyncSkipRedundantSyncDecode', 'ft8DeepDecodeInTx', 'ft8SubpassHarvest']:
    CASES['A'][key] = True
    CASES['B'][key] = False

def run_case(binary, qml_path, original, label, root, phase, rig):
    assertions = []
    for name, value in CASES[label].items():
        literal = str(value).lower()
        assignment = f'bridge.{name} = {literal}; ' if phase == 'write' else ''
        assertions.append(f'{assignment}if (bridge.{name} !== {literal}) failed.push("{name}=" + bridge.{name});')
    injected = '\n    Timer { interval: 12000; running: true; repeat: false; onTriggered: { var failed = [];\n        ' + '\n        '.join(assertions) + f'\n        console.log("[FTX-RUNTIME-{label}-{phase}] " + (failed.length ? "FAIL " + failed.join(",") : "PASS"));\n    }} }}\n'
    marker = 'GlassApplicationWindow {'
    if original.count(marker) != 1:
        raise RuntimeError('unexpected Main.qml layout')
    qml_path.write_text(original.replace(marker, marker + injected, 1))
    env = os.environ.copy()
    env.update(DECODIUM_LAB_SETTINGS_DIR=str(root), DECODIUM_DISABLE_QML_CACHE='1')
    cmd = [str(binary), '--test-mode', '--rig-name', rig,
           '--lab-no-cat', '--lab-no-monitor', '--lab-quit-ms', '27000']
    result = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=110)
    marker = f'[FTX-RUNTIME-{label}-{phase}] PASS'
    if result.returncode or marker not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
        print(result.stdout[-12000:])
        raise RuntimeError(f'{label}: real bridge QML probe failed (exit {result.returncode})')
    print(f'{label} {phase}: {len(CASES[label])} real bridge QML properties passed; CAT disabled', flush=True)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build')
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--qml', type=Path)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    binary = (args.binary or build / 'decodium').resolve()
    qml = (args.qml or build / 'qml/decodium/Main.qml').resolve()
    original = qml.read_text()
    rigs={label:'BridgeFtxRuntime'+label+secrets.token_hex(5) for label in CASES}
    try:
        with tempfile.TemporaryDirectory(prefix='decodium-ftx-runtime-') as td:
            for label in CASES:
                profile = Path(td) / label
                profile.mkdir()
                for phase in ('write', 'restart'):
                    run_case(binary, qml, original, label, profile, phase, rigs[label])
    finally:
        qml.write_text(original)
        for rig in rigs.values():
            for base in ('Application Support','Caches'):
                path=Path.home()/'.qttest/Library'/base/'IU8LMC'/('Decodium - '+rig+' - test')
                if path.is_dir(): shutil.rmtree(path)

if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Exercise real profile and AutoCQ settings with isolated settings and no CAT.

Temporarily instruments Main.qml; don't run another app from the same bundle.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--binary',type=Path,default=ROOT/'build/decodium')
    parser.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml')
    args=parser.parse_args();binary=args.binary.resolve();qml=args.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
    if original.count(marker)!=1:raise RuntimeError('unexpected Main.qml')
    try:
        with tempfile.TemporaryDirectory(prefix='decodium-profile-runtime-') as tmp:
            for phase in ('write','restart'):
                action='''
        bridge.applyReadyProfile("weak");
        if (bridge.activeReadyProfile !== "weak" || !bridge.ft2Conservative || bridge.ft2AdaptiveDecode || bridge.maxCallerRetries !== 7) failed.push("weak");
        bridge.applyReadyProfile("contest");
        if (bridge.activeReadyProfile !== "contest" || bridge.ft2Conservative || !bridge.mamMultiStream || bridge.mamMaxStreams !== 2 || bridge.maxCallerRetries !== 3) failed.push("contest");
        bridge.applyReadyProfile("cpu");
        if (bridge.activeReadyProfile !== "cpu" || bridge.ft2FullDecodeInAutoCq || bridge.maxCallerRetries !== 10) failed.push("cpu");
        bridge.applyReadyProfile("invalid");
        if (bridge.activeReadyProfile !== "balanced" || bridge.maxCallerRetries !== 5) failed.push("balanced");
        bridge.autoCqMaxCycles=9999; bridge.autoCqPauseSec=9999; bridge.autoCqBurstCalls=9999; bridge.autoCqListenCycles=9999;
''' if phase=='write' else ''
                probe='''\n    Timer { interval: 12000; running: true; repeat: false; onTriggered: {
        var failed=[];
        __ACTION__
        if (bridge.activeReadyProfile !== "balanced" || bridge.maxCallerRetries !== 5) failed.push("profile");
        if (bridge.autoCqMaxCycles !== 999 || bridge.autoCqPauseSec !== 300 || bridge.autoCqBurstCalls !== 999 || bridge.autoCqListenCycles !== 999) failed.push("autocq");
        console.log("[FTX-PROFILE-__PHASE__] " + (failed.length ? "FAIL " + failed.join(",") : "PASS"));
    } }\n'''.replace('__ACTION__',action).replace('__PHASE__',phase)
                qml.write_text(original.replace(marker,marker+probe,1))
                env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
                cmd=[str(binary),'--test-mode','--rig-name','BridgeProfileRuntime','--lab-no-cat','--lab-no-monitor','--lab-quit-ms','27000']
                result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=110)
                if result.returncode or f'[FTX-PROFILE-{phase}] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
                    print(result.stdout[-16000:]);raise RuntimeError(f'{phase}: probe failed (exit {result.returncode})')
                print(f'{phase}: ready profiles and AutoCQ values passed; CAT disabled',flush=True)
    finally:qml.write_text(original)
if __name__=='__main__':main()

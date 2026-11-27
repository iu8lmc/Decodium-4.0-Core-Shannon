#!/usr/bin/env python3
"""Exercise SSTV storage state on an isolated application without CAT/audio.

Temporarily instruments the chosen Main.qml; don't run another copy from that
bundle. Rebuild the QML sync target after an unclean exit.
"""
import argparse,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
 p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
 if original.count(marker)!=1:raise RuntimeError('unexpected Main.qml')
 rig='BridgeSstvStorage'+secrets.token_hex(5)
 probe='''\n    Timer { interval: 12000; running: true; repeat: false; onTriggered: {
        var failed=[];
        if (!bridge.sstvAvailable || !bridge.sstvStorageReady) failed.push("storage-unavailable");
        if (bridge.sstvRxAudioJobBusy || bridge.sstvWavReplayActive || bridge.sstvWavReplayProgress!==0) failed.push("job-defaults");
        if (bridge.sstvRxAudioJobError!=="" || bridge.sstvWavReplayError!=="") failed.push("initial-errors");
        bridge.sstvRxAutoSaveEnabled=true;
        if (!bridge.sstvRxAutoSaveEnabled) failed.push("enable");
        bridge.sstvRxAutoSaveEnabled=false;
        if (bridge.sstvRxAutoSaveEnabled) failed.push("disable");
        console.log("[SSTV-STORAGE-RUNTIME] " + (failed.length ? "FAIL " + failed.join(",") : "PASS"));
    } }\n'''
 try:
  with tempfile.TemporaryDirectory(prefix='decodium-sstv-storage-') as tmp:
   qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
   cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','27000']
   result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=110)
   if result.returncode or '[SSTV-STORAGE-RUNTIME] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
    print(result.stdout[-16000:]);raise RuntimeError(f'SSTV storage probe failed (exit {result.returncode})')
   print('SSTV storage state and autosave toggles passed; CAT/monitoring disabled')
 finally:qml.write_text(original)
if __name__=='__main__':main()

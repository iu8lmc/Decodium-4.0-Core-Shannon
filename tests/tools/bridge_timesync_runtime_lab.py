#!/usr/bin/env python3
"""Compare Bridge time-sync settings with a local-only NTP destination.

Temporarily instruments the selected Main.qml; don't run another app from the
same bundle. Rebuild the QML sync target after an unclean exit.
"""
import argparse,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
 p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
 if original.count(marker)!=1:raise RuntimeError('unexpected Main.qml')
 probe='''\n    Timer { interval: 12000; running: true; repeat: false; onTriggered: {
        var failed=[];
        bridge.setSetting("DecoSyncTimeEnabled", false);
        bridge.setSetting("NTPEnabled", false);
        bridge.setSetting("NTPCustomServer", "127.0.0.1");
        bridge.setSetting("NTPOffset_ms", 42.5);
        if (bridge.ntpEnabled || bridge.ntpSynced || Math.abs(bridge.ntpOffsetMs-42.5)>0.001) failed.push("disabled-offset");
        bridge.setSetting("DecoSyncTimeEnabled", true);
        bridge.setSetting("NTPEnabled", true);
        if (!bridge.ntpEnabled || Math.abs(bridge.ntpOffsetMs-42.5)>0.001) failed.push("enabled-offset");
        bridge.setSetting("DecoSyncTimeEnabled", false);
        bridge.setSetting("NTPEnabled", false);
        if (bridge.ntpEnabled || bridge.ntpSynced) failed.push("disabled-again");
        console.log("[TIMESYNC-RUNTIME] " + (failed.length ? "FAIL " + failed.join(",") : "PASS"));
    } }\n'''
 try:
  with tempfile.TemporaryDirectory(prefix='decodium-timesync-runtime-') as tmp:
   qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
   cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','BridgeTimeSync','--lab-no-cat','--lab-no-monitor','--lab-quit-ms','27000']
   result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=110)
   if result.returncode or '[TIMESYNC-RUNTIME] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
    print(result.stdout[-16000:]);raise RuntimeError(f'time sync probe failed (exit {result.returncode})')
   print('time-sync enable, disable and offset passed with local NTP destination; CAT disabled')
 finally:qml.write_text(original)
if __name__=='__main__':main()

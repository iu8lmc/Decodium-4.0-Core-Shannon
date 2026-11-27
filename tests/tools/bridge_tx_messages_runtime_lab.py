#!/usr/bin/env python3
"""Run real TX-message editing/validation without CAT or transmission.

Temporarily instruments the selected bundle's Main.qml; avoid concurrent runs
from the same bundle. Rebuild the QML sync target after an unclean exit.
"""
import argparse,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
 p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');p.add_argument('--require-legacy',action='store_true');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
 if original.count(marker)!=1:raise RuntimeError('unexpected Main.qml')
 rig='TxMessage'+secrets.token_hex(5)
 probe='''\n    Timer { interval: 12000; running: true; repeat: false; onTriggered: {
        var failed=[];
        if (bridge.transmitting || bridge.txEnabled) failed.push("unexpected-tx");
        __LEGACY_ASSERT__
        for (var i=1;i<=5;++i) { bridge.setTxMessage(i,"  cq test " + i + "  "); if (bridge.txMessage(i) !== "CQ TEST " + i) failed.push("slot"+i); }
        bridge.setTxMessage(-1,"  low  "); if (bridge.txMessage(1)!=="LOW") failed.push("lower-bound");
        bridge.setTxMessage(99," cq test 9h1sr "); if (bridge.txMessage(6)!=="CQ TEST 9H1SR") failed.push("custom-cq");
        if (bridge.validateTxMessage("") === "") failed.push("empty-validation");
        if (bridge.validateTxMessage("CQ 9H1SR JM75") !== "") failed.push("ft8-valid");
        bridge.resetStandardTxMessages(); if (bridge.txMessage(6)==="CQ TEST 9H1SR") failed.push("reset");
        console.log("[TX-MESSAGE-RUNTIME] " + (failed.length ? "FAIL " + failed.join(",") : "PASS"));
    } }\n'''
 probe=probe.replace('__LEGACY_ASSERT__', 'if (!bridge.legacyBackendActive) failed.push("legacy-inactive");' if a.require_legacy else '')
 try:
  with tempfile.TemporaryDirectory(prefix='decodium-tx-message-runtime-') as tmp:
   qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   policy='(version 1)(allow default)(deny network*)'
   sinks=[]
   if a.require_legacy:
    env.pop('DECODIUM_FORCE_STANDALONE_UDP',None)
    for port in (2237,2239):
     sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sinks.append(sock)
    guard=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);guard.bind(('127.0.0.1',2238));guard.close()
    policy+='(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','9H1SR','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms','27000']
   result=subprocess.run(['sandbox-exec','-p',policy]+cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=110)
   if result.returncode or '[TX-MESSAGE-RUNTIME] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
    print(result.stdout[-16000:]);raise RuntimeError(f'TX message probe failed (exit {result.returncode})')
   print('TX message slots, bounds, CQ, FT8 validation and reset passed; CAT disabled' + ('; legacy active' if a.require_legacy else ''))
 finally:
  qml.write_text(original)
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   path=parent/('Decodium - '+rig+' - test')
   if path.is_dir():shutil.rmtree(path)
if __name__=='__main__':main()

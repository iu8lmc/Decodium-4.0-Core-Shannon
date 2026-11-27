#!/usr/bin/env python3
"""Compare native WAV decode dispatch in isolated app sessions."""
import argparse,json,os,secrets,subprocess,tempfile,wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--variant',choices=('a','b'),required=True);p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML')
try:
 with tempfile.TemporaryDirectory(prefix='decodium-wav-decode-') as tmp:
  root=Path(tmp);batch=root/'batch';batch.mkdir();wpath=batch/'tone.wav'
  with wave.open(str(wpath),'wb') as w:
   w.setnchannels(1);w.setsampwidth(2 if a.variant=='a' else 1);w.setframerate(12000);w.writeframes(b'\0\0'*1200 if a.variant=='a' else b'\x80'*1200)
  probe='''\n    property int wavGood: 0\n    property int wavMissing: 0\n    property int wavBatch: 0\n    Connections {target:bridge;function onStatusMessage(message) { if(message.indexOf("WAV: 1200 campioni")>=0)wavGood++; if(message.indexOf("Batch decode: 1 file WAV")>=0)wavBatch++; } function onErrorMessage(message) {if(message.indexOf("File WAV non trovato")>=0)wavMissing++;} }\n    Timer { interval:12000;running:true;repeat:false;onTriggered:{bridge.openWavForDecode(__MISSING__);bridge.openWavForDecode(__WAV__);bridge.openWavFolderDecode(__DIR__);verifyWav.start();} }\n    Timer {id:verifyWav;interval:2000;repeat:false;onTriggered:{console.log("[WAV-DECODE] "+(wavMissing===1 && wavGood===2 && wavBatch===1 ? "PASS" : "FAIL "+wavMissing+","+wavGood+","+wavBatch));} }\n'''.replace('__MISSING__',json.dumps(str(root/'missing.wav'))).replace('__WAV__',json.dumps(str(wpath))).replace('__DIR__',json.dumps(str(batch)))
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','BridgeWavDecode'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','22000'];r=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  if r.returncode or '[WAV-DECODE] PASS' not in r.stdout or '[LAB] CAT auto-connect disabled' not in r.stdout:
   print(r.stdout[-16000:]);raise RuntimeError(f'WAV decode failed: {r.returncode}')
  print(f'{a.variant}: WAV single and batch dispatch and missing-file check passed; CAT disabled')
finally:qml.write_text(original)

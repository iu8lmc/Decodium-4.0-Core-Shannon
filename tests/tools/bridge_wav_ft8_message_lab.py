#!/usr/bin/env python3
"""Probe real FT8 message decoding from a generated WAV file."""
import argparse,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--wav',type=Path,required=True);p.add_argument('--message',required=True);p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML')
probe='''\n    Timer { interval:12000;running:true;repeat:false;onTriggered:{console.log("[FT8-WAV-MESSAGE] legacyTX="+bridge.legacyBackendActive);bridge.openWavForDecode(__WAV__);checkWav.start();} }\n    Timer { id:checkWav;interval:250;running:false;repeat:true;property int ticks:0;onTriggered:{\n      var list=bridge.decodeList;for(var i=0;i<list.length;i++){if(String(list[i].message).indexOf(__MSG__)>=0){console.log("[FT8-WAV-MESSAGE] PASS");stop();return;}}\n      if(++ticks>=96){console.log("[FT8-WAV-MESSAGE] FAIL no matching decode");stop();}\n    } }\n'''.replace('__WAV__',json.dumps(str(a.wav.resolve()))).replace('__MSG__',json.dumps(a.message))
try:
 with tempfile.TemporaryDirectory(prefix='decodium-ft8-wav-message-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','BridgeWavFt8'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','65000'];r=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  for line in r.stdout.splitlines():
   if '[WAV-DIAG]' in line or '[FT8-WAV-MESSAGE]' in line: print(line)
  if r.returncode or '[FT8-WAV-MESSAGE] PASS' not in r.stdout or '[LAB] CAT auto-connect disabled' not in r.stdout:
   print(r.stdout[-20000:]);raise RuntimeError(f'FT8 WAV message not received: {r.returncode}')
  print(f'{a.message}: WAV decoded in app; CAT and monitoring disabled')
finally:qml.write_text(original)

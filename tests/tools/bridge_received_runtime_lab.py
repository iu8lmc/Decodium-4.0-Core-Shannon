#!/usr/bin/env python3
"""Exercise asynchronous received-file saves in an isolated Decodium session."""
import argparse, json, os, secrets, subprocess, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--binary', type=Path, default=ROOT/'build/decodium')
p.add_argument('--qml', type=Path, default=ROOT/'build/qml/decodium/Main.qml')
a = p.parse_args(); qml = a.qml.resolve(); original = qml.read_text(); marker='GlassApplicationWindow {'
if original.count(marker)!=1: raise RuntimeError('unexpected QML root')
try:
 with tempfile.TemporaryDirectory(prefix='decodium-received-lab-') as tmp:
  probe='''
 property int receivedCount: 0
 Connections { target: bridge
  function onFt2LinkReceivedFileSaveFinished(id, result) {
   console.log("[RECEIVED-SAVE] "+JSON.stringify({id:id,ok:result.ok,fileName:result.fileName,bytes:result.bytes}));
  }
 }
 Timer { interval:10000; running:true; repeat:false; onTriggered: {
  bridge.saveFt2LinkReceivedFileAsync(__DIR__, "../CON.txt", "alpha", "", false, true);
  bridge.saveFt2LinkReceivedFileAsync(__DIR__, "data.bin", "", "AQIDBA==", true, true);
  bridge.saveFt2LinkReceivedFileAsync(__DIR__, "collision.txt", "one", "", false, true);
  bridge.saveFt2LinkReceivedFileAsync(__DIR__, "collision.txt", "two", "", false, true);
  bridge.saveFt2LinkReceivedFileAsync(__DIR__, "invalid.bin", "", "!!!", true, true);
 }}
'''.replace('__DIR__',json.dumps(tmp))
  qml.write_text(original.replace(marker,marker+probe,1))
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','Received'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','23000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  lines=[json.loads(x.split('[RECEIVED-SAVE] ',1)[1]) for x in result.stdout.splitlines() if '[RECEIVED-SAVE] ' in x]
  if result.returncode or len(lines)!=5 or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-12000:]);raise RuntimeError('received file probe failed')
  if (Path(tmp)/'_CON.txt').read_bytes()!=b'alpha' or (Path(tmp)/'data.bin').read_bytes()!=bytes([1,2,3,4]):raise RuntimeError('saved content mismatch')
  collision=[x for x in lines if x['fileName'].startswith('collision')]; invalid=[x for x in lines if not x['ok']];
  if len(collision)!=2 or len({x['fileName'] for x in collision})!=2 or { (Path(tmp)/x['fileName']).read_bytes() for x in collision}!={b'one',b'two'} or len(invalid)!=1 or (Path(tmp)/'invalid.bin').exists():raise RuntimeError('collision or invalid base64 handling failed')
  lines.sort(key=lambda x:x['id']);print(json.dumps(lines,sort_keys=True))
finally: qml.write_text(original)

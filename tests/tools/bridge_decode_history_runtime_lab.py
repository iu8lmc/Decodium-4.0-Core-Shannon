#!/usr/bin/env python3
"""Exercise history queries against a fresh isolated test session."""
import argparse,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--variant',choices=('a','b'),required=True);p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML')
try:
 with tempfile.TemporaryDirectory(prefix='decodium-history-') as tmp:
  output=Path(tmp)/'history.adi';token='NO-SUCH-CALL-'+secrets.token_hex(6)
  probe='''\n    Timer {interval:12000;running:true;repeat:false;onTriggered:{var f=[];var rows=bridge.queryDecodeHistory({callsign:__TOKEN__,limit:__LIMIT__});if(rows.length!==0)f.push("unexpected-record");var sessions=bridge.queryDecodeSessions(),bands=bridge.queryDecodeBands(),modes=bridge.queryDecodeModes(),stats=bridge.queryDecodeStats();if(sessions===undefined || bands===undefined || modes===undefined || stats===undefined)f.push("queries");if(bridge.exportAdifFromHistory({callsign:__TOKEN__},__OUT__))f.push("unexpected-export");console.log("[HISTORY] "+(f.length?"FAIL "+f.join(","):"PASS"));} }\n'''.replace('__TOKEN__',json.dumps(token)).replace('__LIMIT__','1' if a.variant=='a' else '5000').replace('__OUT__',json.dumps(str(output)))
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','BridgeHistory'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','20000'];r=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  if r.returncode or '[HISTORY] PASS' not in r.stdout or '[LAB] CAT auto-connect disabled' not in r.stdout or not output.exists() or '<EOH>' not in output.read_text():
   print(r.stdout[-16000:]);raise RuntimeError('history probe failed')
  print(f'{a.variant}: history queries and empty export passed in isolated session; CAT disabled')
finally:qml.write_text(original)

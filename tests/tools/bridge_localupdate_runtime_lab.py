#!/usr/bin/env python3
"""Exercise update failure in isolated app profiles with external networking denied."""
import argparse,json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);a=p.parse_args()
qml=root/'build/qml/decodium/Main.qml';original=qml.read_text();marker='GlassApplicationWindow {';assert original.count(marker)==1
rig='LocalUpdate'+secrets.token_hex(5)
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.checkCtyDatUpdate(true);bridge.downloadCall3Txt();}}
 Timer {interval:28000;running:true;repeat:false;onTriggered:{var c=bridge.ctyDatState;var d=bridge.call3TxtState;console.log('[LOCALUPDATE] '+JSON.stringify({ctyError:!!c.error,call3Error:!!d.error,ctyStatus:c.status,call3Status:d.status,cat:bridge.catConnected,tx:bridge.transmitting}));Qt.quit();}}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-localupdate-') as tmp:
  sockets=[]
  for port in (2237,2239):
   s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',port));sockets.append(s)
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','35000']
  policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
  res=subprocess.run(['sandbox-exec','-p',policy]+cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=65)
  rows=[x.split('[LOCALUPDATE] ',1)[1] for x in res.stdout.splitlines() if '[LOCALUPDATE] ' in x]
  if res.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in res.stdout:raise RuntimeError(res.stdout[-7000:])
  result=json.loads(rows[0]);assert not result['cat'] and not result['tx'] and result['ctyError'] and result['call3Error'],result
  print(json.dumps(result,sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)

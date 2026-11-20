#!/usr/bin/env python3
"""Log a synthetic QSO and capture TCP ADIF on a private loopback port."""
import argparse,json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='QsoDone'+secrets.token_hex(5)
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{var ok=bridge.createLogbook("Synthetic__RIG__",false);var safe=ok && String(bridge.activeLogbookPath).indexOf("/.qttest/")>=0 && !bridge.transmitting && !bridge.txEnabled && !bridge.catConnected;if(!safe){console.log("[QSODONE-PROBE] unsafe");return;}bridge.setSetting("PromptToLog",false);bridge.setSetting("ADIFTcpEnabled",true);bridge.setSetting("ADIFTcpServer","127.0.0.1");bridge.setSetting("ADIFTcpPort",__PORT__);bridge.dxCall="K1ABC";bridge.dxGrid="FN42";bridge.resetStandardTxMessages();bridge.logQso();checkDone.start();}}
 Timer {id:checkDone;interval:500;repeat:true;property int n:0;onTriggered:{if(bridge.qsoCount>0){console.log("[QSODONE-PROBE] "+JSON.stringify({count:bridge.qsoCount,clear:bridge.dxCall==="",tx:bridge.txEnabled || bridge.transmitting}));stop();}else if(++n>35){console.log("[QSODONE-PROBE] timeout");stop();}}}
'''.replace('__RIG__',rig)
try:
 with tempfile.TemporaryDirectory(prefix='decodium-qsodone-lab-') as tmp, socket.socket(socket.AF_INET,socket.SOCK_STREAM) as sink:
  sink.bind(('127.0.0.1',0));sink.listen(4);port=sink.getsockname()[1];sink.settimeout(0.2);probe=probe.replace('__PORT__',str(port))
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  app=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms','45000'];policy=f'(version 1)(allow default)(deny network*)(allow network-inbound (local ip \"localhost:*\"))(allow network-outbound (remote ip \"localhost:{port}\"))';cmd=['sandbox-exec','-p',policy]+app
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=85)
  lines=[x.split('[QSODONE-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[QSODONE-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or lines[0] in ('unsafe','timeout') or '[LAB] CAT auto-connect disabled' not in result.stdout:raise RuntimeError('probe failed '+result.stdout[-5000:])
  v=json.loads(lines[0]);expected={'count':1,'clear':True,'tx':False}
  if v!=expected:raise RuntimeError('unexpected '+str(v))
  packets=[]
  while True:
   try:
    conn,_=sink.accept()
    with conn:packets.append(conn.recv(65535))
   except socket.timeout:break
  matches=[p for p in packets if b'K1ABC' in p and b'<eor>' in p.lower()]
  if not matches:raise RuntimeError('synthetic ADIF TCP payload missing: '+str([len(p) for p in packets]))
  print(json.dumps(v,sort_keys=True)+'; synthetic ADIF TCP received on private loopback')
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)

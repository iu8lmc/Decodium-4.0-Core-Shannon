#!/usr/bin/env python3
"""Compare LoTW existing shared cache priority in isolated app processes, with no network."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';module=r/'src/bridge/DecodiumBridgeUsStateLotw.cpp';qml=r/'build/qml/decodium/Main.qml'
a,b,q=(x.read_text() for x in (main,module,qml));needle='static QString sharedLotwUsersCachePath()\n{';assert b.count(needle)==1
instrumented=b.replace(needle,needle+'\n    return qEnvironmentVariable("LAB_LOTW_PATH");\n',1)
instrumented=instrumented.replace('QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)', 'qEnvironmentVariable("LAB_LOTW_LEGACY_DIR")')
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.updateLotwUsers();watch.start();}}\n Timer {id:watch;interval:200;running:false;repeat:true;property int n:0;onTriggered:{if(bridge.lotwUpdating&&++n<50)return;console.log('[LOTW-LOCAL] '+JSON.stringify({first:bridge.isLotwUser("k1abc"),second:bridge.isLotwUser("W1AW"),absent:bridge.isLotwUser("N0ZZZ"),updating:bridge.lotwUpdating,cat:bridge.catConnected,tx:bridge.transmitting}));stop();Qt.quit();}}\n'''
rigs=[];results=[]
try:
 for label in ('before','after'):
  if label=='before':main.write_text(a+'\n#undef bridgeLog\n'+instrumented);module.write_text('#include "DecodiumBridge.h"\n')
  else:main.write_text(a);module.write_text(instrumented)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='LotwLocal'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-lotw-local-') as tmp:
   cache=Path(tmp)/'synthetic.csv';legacy=Path(tmp)/'lotw_users.txt';legacy.write_text('CALLSIGN,UPDATED\nOLD1,2026\n'); profile=Path(tmp)/'profile';profile.mkdir();newest=profile/'lotw-user-activity.csv';newest.write_text('CALLSIGN,UPDATED\nK1ABC,2026\nk1abc,2025\nW1AW,2026\n');os.utime(legacy,(1,1));cache.write_bytes(newest.read_bytes());newest.write_text('CALLSIGN,UPDATED\nOTHER1,2026\n')
   env=os.environ.copy();env.update(LAB_LOTW_PATH=str(cache),LAB_LOTW_LEGACY_DIR=str(profile),DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   proc=subprocess.run(['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'],cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[LOTW-LOCAL] ',1)[1] for line in proc.stdout.splitlines() if '[LOTW-LOCAL] ' in line]
   if proc.returncode or len(rows)!=1:raise RuntimeError(proc.stdout[-5000:])
   value=json.loads(rows[0]);assert value=={'first':True,'second':True,'absent':False,'updating':False,'cat':False,'tx':False},value
   assert cache.exists() and cache.read_text()=='CALLSIGN,UPDATED\nK1ABC,2026\nk1abc,2025\nW1AW,2026\n', 'existing cache was changed';value['preserved']=True;results.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
   for sock in socks:sock.close()
 assert results[0]==results[1]
finally:
 for p,t in ((main,a),(module,b),(qml,q)):p.write_text(t)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)

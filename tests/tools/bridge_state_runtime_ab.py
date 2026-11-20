#!/usr/bin/env python3
"""Compare FT scheduler inside isolated real application processes."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; module=r/'src/bridge/DecodiumBridgeState.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,s,n,q=(p.read_text() for p in (main,sstv,module,qml))
fixture=(r/'tests/bridge_refactor_fixtures/bridge_state_before.inc').read_text()
assert fixture in n
head=n[:-len(fixture)]
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert s.count(anchor)==1
prototypes=""
injection='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);QString priorCall=m_callsign,priorMode=m_mode;auto oldD=m_decodeList,oldR=m_rxDecodeList;int oldP=m_periodProgress;m_callsign=QStringLiteral("TEST1");m_mode=QStringLiteral("FT8");m_decodeList={QVariantMap{{"message",QStringLiteral("band")}}};m_rxDecodeList={QVariantMap{{"message",QStringLiteral("rx")}}};m_periodProgress=37;int catSignal=0,timeSignal=0;auto ca=connect(this,&DecodiumBridge::catSettingsRequested,this,[&]{++catSignal;});auto ct=connect(this,&DecodiumBridge::utcTimeChanged,this,[&]{++timeSignal;});openCatSettings();onUtcTimer();bool clock=QTime::fromString(utcTime(),QStringLiteral("hh:mm:ss")).isValid();bool state=callsign()==QStringLiteral("TEST1")&&mode()==QStringLiteral("FT8")&&decodeList().size()==1&&rxDecodeList().size()==1&&periodProgress()==37&&!transmitting()&&!decoding();disconnect(ca);disconnect(ct);m_callsign=priorCall;m_mode=priorMode;m_decodeList=oldD;m_rxDecodeList=oldR;m_periodProgress=oldP;return {{"state",state},{"clock",clock},{"catSignal",catSignal},{"timeSignal",timeSignal},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''
 Timer { interval:12000; running:true; repeat:false; onTriggered:Qt.quit() }
 Timer { interval:10000; running:true; repeat:false; onTriggered:console.log('[FTSCHED-PROBE] '+JSON.stringify(bridge.logSstvQso({}))) }
'''
rigs=[]; results=[]
try:
 sstv.write_text(s.replace(anchor,prototypes+anchor+injection,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_UTC_TIMER_INSERTION_POINT'; assert m.count(point)==1
   main.write_text(m.replace(point,fixture+point)); module.write_text(n.replace(fixture, '', 1))
  else: main.write_text(m); module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1)); rig='StateAccessLab'+secrets.token_hex(5); rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-ftsched-runtime-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[FTSCHED-PROBE] ',1)[1] for line in proc.stdout.splitlines() if '[FTSCHED-PROBE] ' in line]
   if proc.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:raise RuntimeError(proc.stdout[-6000:])
   value=json.loads(rows[0]); expected={'state':True,'clock':True,'catSignal':1,'timeSignal':1,'cat':False,'tx':False}
   assert value==expected,value
   results.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
   for sock in socks:sock.close()
 assert results[0]==results[1]
finally:
 for p,text in ((main,m),(sstv,s),(module,n),(qml,q)):p.write_text(text)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

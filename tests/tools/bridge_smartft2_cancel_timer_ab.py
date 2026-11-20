#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeSmartFt2Scheduler.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/smartft2scheduler_before.inc').read_text();mark='void DecodiumBridge::scheduleSmartFt2AsyncTx(';head=n.split(mark,1)[0];assert (mark+n.split(mark,1)[1]).replace('decodiumBridgeSmartFt2Log(', 'bridgeLog(').replace('decodiumBridgeLatestD3SyncStartMs(', 'latestD3CompatibleSyncTxStartMs(')==fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''\n#if DECODIUM_HAS_SSTV
    {Q_UNUSED(values);m_mode=QStringLiteral("FT2");m_asyncTxEnabled=true;m_monitoring=true;m_txEnabled=true;m_manualTxHold=false;m_ft2AsyncAudioQuietRuns=5;m_ft2AsyncLastDecodeMs=0;m_ft2AsyncFirstDecodeMs=0;m_ft2AsyncPartnerSlotMs=0;int txChanges=0;auto c=connect(this,&DecodiumBridge::transmittingChanged,this,[&](){if(m_transmitting)++txChanges;});scheduleSmartFt2AsyncTx(QStringLiteral("cancel-lab"));bool scheduled=m_ft2AsyncSmartTxPending;quint64 generation=m_txRequestGeneration;quint64 serial=m_ft2AsyncSmartTxSerial;invalidateDeferredTxRequests(QStringLiteral("cancel-lab"));bool invalidated=!m_ft2AsyncSmartTxPending&&m_txRequestGeneration==generation+1&&m_ft2AsyncSmartTxSerial>serial;QEventLoop loop;QTimer::singleShot(3200,&loop,&QEventLoop::quit);loop.exec();bool noTx=!m_transmitting&&txChanges==0&&!m_ft2AsyncSmartTxPending;disconnect(c);m_monitoring=false;m_txEnabled=false;return {{"scheduled",scheduled},{"invalidated",invalidated},{"noTx",noTx},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif\n'''

probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[SMART-FT2] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text('#include <QEventLoop>\n#include <QTimer>\n'+s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='SmartFt2Cancel'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-smartft2scheduler-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[SMART-FT2] ',1)[1] for line in result.stdout.splitlines() if '[SMART-FT2] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value['scheduled'] and value['invalidated'] and value['noTx'] and not value['cat'] and not value['tx'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)

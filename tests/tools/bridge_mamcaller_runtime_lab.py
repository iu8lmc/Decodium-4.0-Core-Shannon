#!/usr/bin/env python3
"""Exercise directed caller ingestion and signoff on isolated before/after apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeMamCaller.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/mamcaller_before.inc').read_text();mark='void DecodiumBridge::removeCallerFromQueue(';head=n.split(mark,1)[0];assert (mark+n.split(mark,1)[1]).replace('decodiumBridgeMamQueueLog(', 'bridgeLog(').replace('decodiumBridgeInferPartnerForAutolog(', 'inferPartnerFromDirectedMessage(')==fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''\n#if DECODIUM_HAS_SSTV\n    { Q_UNUSED(values); const bool oldMulti=m_multiAnswerMode, oldAuto=m_autoCqRepeat, oldEnabled=m_txEnabled; const int oldCurrent=m_currentTx; m_multiAnswerMode=true; m_autoCqRepeat=false; m_txEnabled=false; m_currentTx=1; clearCallerQueue(); maybeEnqueueMamCallerFromMessage(QStringLiteral("9H1SR K1ABC -10"),1200,-10,true); const int queued=m_callerQueue.size(); const bool hasCaller=m_callerQueue.join(QLatin1Char(' ')).contains(QStringLiteral("K1ABC")); maybeEnqueueMamCallerFromMessage(QStringLiteral("9H1SR K1ABC 73"),1200,-10,true); const int remaining=m_callerQueue.size(); const bool worked=isRecentAutoCqWorkedOrLoggedDuplicate(QStringLiteral("K1ABC"),m_frequency,m_mode); const bool cooldown=m_qsoCooldown.contains(QStringLiteral("K1ABC")); m_multiAnswerMode=oldMulti;m_autoCqRepeat=oldAuto;m_txEnabled=oldEnabled;m_currentTx=oldCurrent;return {{QStringLiteral("queued"),queued},{QStringLiteral("hasCaller"),hasCaller},{QStringLiteral("remaining"),remaining},{QStringLiteral("worked"),worked},{QStringLiteral("cooldown"),cooldown},{QStringLiteral("cat"),m_catConnected},{QStringLiteral("tx"),m_transmitting}}; }\n#endif\n'''
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT8";bridge.callsign="9H1SR";bridge.frequency=14074000;console.log('[MAM-CALLER] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text(s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='MamCaller'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-mamcaller-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[MAM-CALLER] ',1)[1] for line in result.stdout.splitlines() if '[MAM-CALLER] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value=={'queued':1,'hasCaller':True,'remaining':0,'worked':True,'cooldown':True,'cat':False,'tx':False},value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

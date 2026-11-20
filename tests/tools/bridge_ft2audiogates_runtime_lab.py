#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeFt2AudioGates.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/ft2audiogates_before.inc').read_text();mark='void DecodiumBridge::onAudioLevelForFt2Gate()';head=n.split(mark,1)[0];assert (mark+n.split(mark,1)[1])==fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''\n#if DECODIUM_HAS_SSTV\n    { Q_UNUSED(values);const bool oldAsync=m_asyncTxEnabled,oldAdaptive=m_ft2AdaptiveTxGates;const double oldLevel=m_audioLevel;const int oldRuns=m_ft2AsyncAudioQuietRuns;const qint64 oldDecode=m_ft2AsyncLastDecodeMs;m_mode=QStringLiteral("FT2");m_asyncTxEnabled=true;m_ft2AdaptiveTxGates=true;m_audioLevel=0;m_ft2AsyncAudioQuietRuns=5;m_ft2AsyncLastDecodeMs=0;onAudioLevelForFt2Gate();int a,b,c,d;ft2AsyncGateRequirements(10000,a,b,c,d);const bool clear=a==2 && b==250 && c==20 && d==80;m_audioLevel=0.005;onAudioLevelForFt2Gate();ft2AsyncGateRequirements(10000,a,b,c,d);const bool busy=m_ft2AsyncAudioQuietRuns==0 && a==5 && b==600 && c==80 && d==300;m_asyncTxEnabled=oldAsync;m_ft2AdaptiveTxGates=oldAdaptive;m_audioLevel=oldLevel;m_ft2AsyncAudioQuietRuns=oldRuns;m_ft2AsyncLastDecodeMs=oldDecode;return {{QStringLiteral("clear"),clear},{QStringLiteral("busy"),busy},{QStringLiteral("cat"),m_catConnected},{QStringLiteral("tx"),m_transmitting}}; }\n#endif\n'''
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[FT2-AUDIO-GATES] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text(s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='Ft2AudioGates'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-ft2audiogates-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[FT2-AUDIO-GATES] ',1)[1] for line in result.stdout.splitlines() if '[FT2-AUDIO-GATES] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value['clear'] and value['busy'] and not value['cat'] and not value['tx'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

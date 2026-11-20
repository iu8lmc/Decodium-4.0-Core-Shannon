#!/usr/bin/env python3
"""Exercise telemetry reset/summary in real apps with a temporary test entry point."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeTxAudioTelemetry.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/txaudiotelemetry_before.inc').read_text();marker='void DecodiumBridge::resetTxAudioTelemetry(';head=n.split(marker,1)[0];body=marker+n.split(marker,1)[1];assert body.replace('decodiumBridgeTxAudioTelemetryLog(', 'bridgeLog(')==fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''\n#if DECODIUM_HAS_SSTV\n    { Q_UNUSED(values); resetTxAudioTelemetry(123,1000,900000,20,30,40,400,800,0.5,0.7); const auto expected=m_txAudioTelemetryExpectedUs; logTxAudioTelemetrySummary(QStringLiteral("explicit-telemetry-lab")); return {{QStringLiteral("serialAfter"),QVariant::fromValue(m_txAudioTelemetrySerial)},{QStringLiteral("expectedUs"),expected},{QStringLiteral("cat"),m_catConnected},{QStringLiteral("tx"),m_transmitting}}; }\n#endif\n'''
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{console.log('[AUDIO-TELEM] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];results=[]
try:
 sstv.write_text(s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_CONSTRUCTOR_INSERTION_POINT';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='TelemetryExplicit'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-audio-telemetry-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[AUDIO-TELEM] ',1)[1] for line in result.stdout.splitlines() if '[AUDIO-TELEM] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value=={'serialAfter':0,'expectedUs':900000,'cat':False,'tx':False},value
   logs=Path.home()/'Library/Application Support/IU8LMC/Decodium';diagnostic=''.join(p.read_text() for p in (logs/'decodium_diagnostic.1.log',logs/'decodium_diagnostic.log') if p.exists()).rsplit(rig,1)[-1];summary=[l for l in diagnostic.splitlines() if 'TX audio telemetry summary: serial=123 reason=explicit-telemetry-lab' in l];assert len(summary)==1,summary
   assert 'expected=900ms' in summary[0] and 'pcmBytes=400' in summary[0],summary
   results.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert results[0]==results[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)

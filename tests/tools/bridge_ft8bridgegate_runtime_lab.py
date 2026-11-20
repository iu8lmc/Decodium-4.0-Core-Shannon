#!/usr/bin/env python3
"""Exercise FT8 bridge/legacy gate selection in isolated app builds."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeFt8AutoGate.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/ft8bridgegate_before.inc').read_text();mark='bool DecodiumBridge::usesFt8AutoTxDecodeGate() const';head=n.split(mark,1)[0];body=mark+n.split(mark,1)[1]
for a,b in [('decodiumBridgeFt8GateLog(', 'bridgeLog('),('decodiumBridgeTruthyEnvironmentFlag(', 'truthyEnvironmentFlag('),('decodiumBridgeLatestD3SyncStartMs(', 'latestD3CompatibleSyncTxStartMs('),('decodiumBridgeTxPeriodIsEven(', 'bridgeTxPeriodIsEven(')]:body=body.replace(a,b)
assert body==fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''\n#if DECODIUM_HAS_SSTV\n    { Q_UNUSED(values);const bool oldLegacy=m_useLegacyTxBackend,oldForced=m_forceLegacyTxForSpecialOp,oldEnabled=m_txEnabled,oldAuto=m_autoCqRepeat,oldSeq=m_autoSeq;m_useLegacyTxBackend=false;m_forceLegacyTxForSpecialOp=false;m_decoPortUseRemote=false;m_cwTxActive=false;m_telemetryTxActive=false;m_explicitManualTxRequest=0;m_autoSeq=true;m_autoCqRepeat=false;m_txEnabled=true;m_txPeriod=((correctedUtcEpochMs()/15000)%2)==0?0:1;const bool gate=usesFt8AutoTxDecodeGate(),defer=deferFt8AutoTxForDecode();m_useLegacyTxBackend=true;const bool legacyOwner=usingLegacyBackendForTx(),legacyGate=usesFt8AutoTxDecodeGate();m_useLegacyTxBackend=oldLegacy;m_forceLegacyTxForSpecialOp=oldForced;m_txEnabled=oldEnabled;m_autoCqRepeat=oldAuto;m_autoSeq=oldSeq;++m_txRequestGeneration;return {{QStringLiteral("gate"),gate},{QStringLiteral("defer"),defer},{QStringLiteral("legacyOwner"),legacyOwner},{QStringLiteral("legacyGate"),legacyGate},{QStringLiteral("cat"),m_catConnected},{QStringLiteral("tx"),m_transmitting}}; }\n#endif\n'''
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT8";bridge.callsign="9H1SR";console.log('[FT8-GATE] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text(s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='Ft8Gate'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-ft8gate-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-audio-input','BlackHole 16ch','--lab-audio-output','BlackHole 2ch','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[FT8-GATE] ',1)[1] for line in result.stdout.splitlines() if '[FT8-GATE] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value['gate'] and value['defer'] and not value['cat'] and not value['tx'] and (not value['legacyOwner'] or not value['legacyGate']),value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeQ65Split.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/q65split_before.inc').read_text();mark='double DecodiumBridge::catSplitTxPttDialFrequencyHz()';helperStart=n.index('static QVariant legacySplitModeSettingValue(');head=n[:helperStart];helper=n[helperStart:n.index(mark)];tail='#undef normalizedCatSplitMode\n#undef legacyOwnsRigControl\n#undef bridgeLog\n';assert n==head+helper+fixture+tail;head+=tail
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);syncCatSplitModeToLegacy(QStringLiteral(" fake it "),QStringLiteral("test"));QSettings st(QSettings::IniFormat,QSettings::UserScope,QStringLiteral("Decodium"),QStringLiteral("Decodium3"));decodium::beginActiveSettingsProfile(st);QString first=st.value(QStringLiteral("Transceiver/splitMode")).toString();syncCatSplitModeToLegacy(QStringLiteral("rig"),QStringLiteral("test"));st.sync();QString second=st.value(QStringLiteral("Transceiver/splitMode")).toString();syncCatSplitModeToLegacy(QStringLiteral("invalid"),QStringLiteral("test"));st.sync();QString third=st.value(QStringLiteral("Transceiver/splitMode")).toString();double fallback=catSplitTxDialFrequencyHz();double noDoppler=catSplitTxPttDialFrequencyHz();m_q65Doppler->setEnabled(true);refreshQ65Doppler();double disconnected=catSplitTxPttDialFrequencyHz();bool notApplied=!m_q65Doppler->applied();m_q65Doppler->setEnabled(false);return {{"first",first},{"second",second},{"third",third},{"fallback",qFuzzyCompare(fallback+1,noDoppler+1)},{"disconnected",qFuzzyCompare(fallback+1,disconnected+1)},{"notApplied",notApplied},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
''' 
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[Q65-SPLIT] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text('#include "Q65DopplerTracker.h"\n'+s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,helper+fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='Q65Split'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-q65split-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[Q65-SPLIT] ',1)[1] for line in result.stdout.splitlines() if '[Q65-SPLIT] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value['first']=='emulate' and value['second']=='rig' and value['third']=='none' and value['fallback'] and value['disconnected'] and value['notApplied'] and not value['cat'] and not value['tx'],value;values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

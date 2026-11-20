#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeLegacySync.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/legacy_sync_before.inc').read_text();mark='void DecodiumBridge::syncLegacyBackendDecodeList()';head=n.split(mark,1)[0];assert n==head+fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
backendh=r/'src/bridge/DecodiumLegacyBackend.h';backendcpp=r/'src/bridge/DecodiumLegacyBackend.cpp';bh=backendh.read_text();bc=backendcpp.read_text()
filters=r/'src/bridge/DecodiumBridgeDecodeFilterSettings.cpp';filter_original=filters.read_text()
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);if(!m_legacyBackend)return {{"error","no legacy"}};setMode(QStringLiteral("FT8"));setHideGhostDecodes(false);m_filterMyCallOnly=false;m_filtersBypassed=false;m_filterCqOnly=true;m_cqFilterLevel=0;bool second=m_legacyBackend->labAppendDecode(QStringLiteral("120000 -12 0.1 1700 ~ W1XYZ K2ABC -10"));bool inserted=m_legacyBackend->labAppendDecode(QStringLiteral("120000 -10 0.1 1500 ~ CQ K1ABC FN42"));auto f=readDecodeUserFilterConfig();QVariantMap sample{{"message","CQ K1ABC FN42"},{"isCQ",true},{"isTx",false}};QString filterReason;bool accepted=shouldAcceptDecodeEntryByUserFilters(sample,f,false,&filterReason);QString reason;bool semantic=shouldAcceptDecodedMessage(QStringLiteral("CQ K1ABC FN42"),&reason);auto rawLines=m_legacyBackend->bandActivityLines();auto parsed=mirrorLegacyDecodeLines(m_legacyBackend->bandActivityLines(),false,QVariantList{});syncLegacyBackendDecodeList();bool cq=false,directed=false;for(const QVariant &v:std::as_const(m_decodeList)){auto msg=v.toMap().value("message").toString();cq|=msg.contains("CQ K1ABC FN42");directed|=msg.contains("W1XYZ K2ABC -10");}return {{"filterAccepted",accepted},{"filterReason",filterReason},{"territories",f.territoryTerms},{"blacklist",f.blacklistEnabled},{"whitelist",f.whitelistEnabled},{"raw",rawLines},{"semantic",semantic},{"reason",reason},{"parsed",parsed},{"inserted",inserted},{"second",second},{"cq",cq},{"directed",directed},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[MODE-CLEANUP] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 signature='DecodeUserFilterConfig DecodiumBridge::readDecodeUserFilterConfig() const\n{'
 assert filter_original.count(signature)==1
 filters.write_text(filter_original.replace(signature,signature+'\n    if (qEnvironmentVariableIsSet("DECODIUM_LAB_CLEAR_USER_FILTERS")) return {};',1))
 backendh.write_text(bh.replace('    void clearBandActivity();','    bool labAppendDecode(const QString&);\n    void clearBandActivity();',1))
 backendcpp.write_text(bc.replace('#include <QTimer>','#include <QTimer>\n#include <QTextEdit>',1)+'\nbool DecodiumLegacyBackend::labAppendDecode(const QString& line) { if(!m_mainWindow)return false; auto *view=m_mainWindow->findChild<QTextEdit*>(QStringLiteral("decodedTextBrowser")); if(!view)return false;view->append(line);return true; }\n')
 sstv.write_text(s.replace('#include \"DecodiumBridge.h\"','#include \"DecodiumBridge.h\"\n#include \"DecodeUserFilterConfig.h\"').replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// === STATION ===';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='LegacySyncCqFilter'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-modecleanup-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_CLEAR_USER_FILTERS='1',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[MODE-CLEANUP] ',1)[1] for line in result.stdout.splitlines() if '[MODE-CLEANUP] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);values.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
 assert values[0]==values[1]
 if not all(v['inserted'] and v['second'] and v['cq'] and not v['directed'] and not v['cat'] and not v['tx'] for v in values):raise RuntimeError('CQ filter did not yield the expected row')
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q);backendh.write_text(bh);backendcpp.write_text(bc);filters.write_text(filter_original)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

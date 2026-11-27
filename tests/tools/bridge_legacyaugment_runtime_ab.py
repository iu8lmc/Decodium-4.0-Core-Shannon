#!/usr/bin/env python3
"""Compare FT scheduler inside isolated real application processes."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; module=r/'src/bridge/DecodiumBridgeLegacyAugment.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,s,n,q=(p.read_text() for p in (main,sstv,module,qml))
signature='QVariantList DecodiumBridge::augmentLegacyMirrorWithAllTxt(';head,tail=n.split(signature,1);fixture=signature+tail
legacy=r/'src/bridge/DecodiumBridgeLegacyAllTxt.cpp';original_legacy=legacy.read_text();pathSig='QString DecodiumBridge::legacyAllTxtPath() const\n{';assert original_legacy.count(pathSig)==1
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert s.count(anchor)==1
prototypes=""
injection='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);m_mode=QStringLiteral("FT8");m_filtersBypassed=true;m_legacyAllTxtConsumedPath.clear();m_legacyAllTxtConsumedSize=-1;QString path=qEnvironmentVariable("LAB_SYNTHETIC_ALLTXT");QFile f(path);if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate))return {{"error",QStringLiteral("create")}};f.write("seed\\n");f.close();auto first=augmentLegacyMirrorWithAllTxt({},false);qint64 primeSize=m_legacyAllTxtConsumedSize;bool enabled=isTimeSyncDecodeMode(m_mode);QString checkPath=legacyAllTxtPath();QString reject;bool semantic=shouldAcceptDecodedMessage(QStringLiteral("CQ K1ABC FN42"),&reject);if(!f.open(QIODevice::Append))return {{"error",QStringLiteral("append")}};f.write("261107_120030 14.074 Rx FT8 -12 0.1 1500 CQ K1ABC FN42\\n");f.close();QFile rawFile(path);if(!rawFile.open(QIODevice::ReadOnly))return {{"error",QStringLiteral("read")}};rawFile.seek(primeSize);QString raw=QString::fromUtf8(rawFile.readAll());rawFile.close();auto second=augmentLegacyMirrorWithAllTxt({},false);bool one=second.size()==1&&second.first().toMap().value(QStringLiteral("message")).toString()==QStringLiteral("CQ K1ABC FN42");auto third=augmentLegacyMirrorWithAllTxt(second,false);bool dedupe=third.size()==1;return {{"raw",raw},{"primeSize",primeSize},{"enabled",enabled},{"pathOk",checkPath==path},{"semantic",semantic},{"reason",reject},{"fileSize",QFileInfo(f).size()},{"count",second.size()},{"record",second.isEmpty()?QVariantMap{}:second.first().toMap()},{"primed",first.isEmpty()},{"appended",one},{"dedupe",dedupe},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''
 Timer { interval:12000; running:true; repeat:false; onTriggered:Qt.quit() }
 Timer { interval:10000; running:true; repeat:false; onTriggered:console.log('[FTSCHED-PROBE] '+JSON.stringify(bridge.logSstvQso({}))) }
'''
rigs=[]; results=[]
try:
 legacy.write_text(original_legacy.replace(pathSig,pathSig+'\n    if (qEnvironmentVariableIsSet("LAB_SYNTHETIC_ALLTXT")) return qEnvironmentVariable("LAB_SYNTHETIC_ALLTXT");',1))
 sstv.write_text(s.replace(anchor,prototypes+anchor+injection,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_CONSTRUCTOR_INSERTION_POINT'; assert m.count(point)==1
   main.write_text(m.replace(point,head+fixture+point)); module.write_text(head)
  else: main.write_text(m); module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1)); rig='LegacyAugmentAudit'+secrets.token_hex(5); rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-ftsched-runtime-') as tmp:
   env=os.environ.copy();env.update(LAB_SYNTHETIC_ALLTXT=tmp+'/synthetic-all.txt',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   proc=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[FTSCHED-PROBE] ',1)[1] for line in proc.stdout.splitlines() if '[FTSCHED-PROBE] ' in line]
   if proc.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:raise RuntimeError(proc.stdout[-6000:])
   value=json.loads(rows[0]); expected={'primed':True,'appended':True,'dedupe':True,'cat':False,'tx':False}
   print(label,json.dumps(value,sort_keys=True),flush=True);print("\n".join(x for x in proc.stdout.splitlines() if "alltxt" in x.lower() or "filter" in x.lower()),flush=True);assert value["primed"] and value["appended"] and value["dedupe"] and not value["cat"] and not value["tx"],value
   results.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
   for sock in socks:sock.close()
 assert results[0]==results[1]
finally:
 for p,text in ((main,m),(sstv,s),(module,n),(qml,q),(legacy,original_legacy)):p.write_text(text)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)

#!/usr/bin/env python3
"""Compare FT scheduler inside isolated real application processes."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; module=r/'src/bridge/DecodiumBridgeLegacyHash.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,s,n,q=(p.read_text() for p in (main,sstv,module,qml))
signature='QString DecodiumBridge::resolveFt8HashPlaceholdersFromRecentAllTxt(';head,tail=n.split(signature,1);fixture=signature+tail
base=subprocess.check_output(['git','show','v1.0.737:src/bridge/DecodiumBridge.cpp'],cwd=r,text=True); start=base.index(signature); end=base.index('\n}',start)+2; before=base[start:end]+'\n'; before=before.replace('secondsFromUtcDisplayToken(', 'decodiumBridgeSecondsFromUtcDisplayToken(').replace('normalizeUtcDisplayToken(', 'decodiumBridgeNormalizeRemoteActivityTime(').replace('bridgeLog(', 'decodiumBridgeDecodeFlowLog('); assert before.strip()==fixture.strip()
hashfile=r/'src/bridge/LegacyHashResolve.cpp'; original_hash=hashfile.read_text(); hashanchor='QStringList hashResolveLogCandidatePaths(QString const& primaryAllTxtPath)\n{'; assert original_hash.count(hashanchor)==1
legacy=r/'src/bridge/DecodiumBridgeLegacyAllTxt.cpp';original_legacy=legacy.read_text();pathSig='QString DecodiumBridge::legacyAllTxtPath() const\n{';assert original_legacy.count(pathSig)==1
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert s.count(anchor)==1
prototypes=""
injection='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);QString path=qEnvironmentVariable("LAB_SYNTHETIC_ALLTXT");QFile f(path);if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate))return {{"error","file"}};f.write("261107_120000 14.074 Rx FT8 -12 0.1 1500 K1ABC K2XYZ RR73\\n");f.close();QString source;QString resolved=resolveFt8HashPlaceholdersFromRecentAllTxt("120001","1503","<...> K2XYZ RR73","FT8",&source);QString wrong=resolveFt8HashPlaceholdersFromRecentAllTxt("120001","1503","<...> K2XYZ RR73","FT4",nullptr);return {{"resolved",resolved},{"source",source},{"wrong",wrong},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''

probe='''
 Timer { interval:12000; running:true; repeat:false; onTriggered:Qt.quit() }
 Timer { interval:10000; running:true; repeat:false; onTriggered:console.log('[FTSCHED-PROBE] '+JSON.stringify(bridge.logSstvQso({}))) }
'''
rigs=[]; results=[]
try:
 hashfile.write_text(original_hash.replace(hashanchor,hashanchor+'\n    return QStringList{primaryAllTxtPath};',1))
 legacy.write_text(original_legacy.replace(pathSig,pathSig+'\n    if (qEnvironmentVariableIsSet("LAB_SYNTHETIC_ALLTXT")) return qEnvironmentVariable("LAB_SYNTHETIC_ALLTXT");',1))
 sstv.write_text(s.replace(anchor,prototypes+anchor+injection,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_CONSTRUCTOR_INSERTION_POINT'; assert m.count(point)==1
   main.write_text(m.replace(point,head+before+point)); module.write_text(head)
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
   value=json.loads(rows[0]); print(label,json.dumps(value,sort_keys=True),flush=True)
   assert value['resolved']=='K1ABC K2XYZ RR73' and value['source']=='synthetic-all.txt' and value['wrong']=='<...> K2XYZ RR73' and not value['cat'] and not value['tx'],value
   results.append(value);print(label,json.dumps(value,sort_keys=True),flush=True)
   for sock in socks:sock.close()
 assert results[0]==results[1]
finally:
 for p,text in ((main,m),(sstv,s),(module,n),(qml,q),(legacy,original_legacy),(hashfile,original_hash)):p.write_text(text)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True)

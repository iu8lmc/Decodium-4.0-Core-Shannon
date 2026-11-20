#!/usr/bin/env python3
"""Compare FT scheduler inside isolated real application processes."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; module=r/'src/bridge/DecodiumBridgeTxPrepare.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,s,n,q=(p.read_text() for p in (main,sstv,module,qml))
fixture=(r/'tests/bridge_refactor_fixtures/tx_prepare_adapters_before.inc').read_text()
assert fixture in n
head=n[:-len(fixture)]
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert s.count(anchor)==1
prototypes="""#include <QAudioDevice>
#include <QAudioFormat>
QString decodiumBridgeTxPrepareAudioFormat(const QAudioFormat&);
bool decodiumBridgeTxPrepareSameFormat(const QAudioFormat&, const QAudioFormat&);
QAudioFormat decodiumBridgeTxPrepareChooseFormat(const QAudioDevice&);
QByteArray decodiumBridgeTxPreparePcm(const QVector<float>&, const QAudioFormat&, int);
"""
injection='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);QAudioFormat f;f.setSampleRate(12000);f.setChannelCount(2);f.setSampleFormat(QAudioFormat::Int16);QAudioFormat g=f;g.setSampleRate(48000);QVector<float> wave{-1.0f,0.0f,0.5f,1.0f};QByteArray left=decodiumBridgeTxPreparePcm(wave,f,0),right=decodiumBridgeTxPreparePcm(wave,f,1);QAudioFormat empty=decodiumBridgeTxPrepareChooseFormat(QAudioDevice{});return {{"same",decodiumBridgeTxPrepareSameFormat(f,f)},{"different",!decodiumBridgeTxPrepareSameFormat(f,g)},{"format",decodiumBridgeTxPrepareAudioFormat(f)},{"left",QString::fromLatin1(left.toHex())},{"right",QString::fromLatin1(right.toHex())},{"emptyValid",empty.isValid()},{"cat",m_catConnected},{"tx",m_transmitting}};}
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
   point='// BRIDGE_TX_FORMAT_ADAPTER_INSERTION_POINT'; assert m.count(point)==1
   main.write_text(m.replace(point,fixture+point)); module.write_text(n.replace(fixture, ''.join(line.split('{',1)[0].rstrip()+';\n' for line in fixture.splitlines()), 1))
  else: main.write_text(m); module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1)); rig='TxAdaptersLab'+secrets.token_hex(5); rigs.append(rig)
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
   value=json.loads(rows[0]); assert value['same'] and value['different'] and value['format'] and value['left'] and value['right'] and not value['cat'] and not value['tx'],value
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

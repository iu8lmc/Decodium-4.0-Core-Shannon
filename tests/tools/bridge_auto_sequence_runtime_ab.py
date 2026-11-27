#!/usr/bin/env python3
"""A/B sequencer on synthetic decoded messages, CAT and PTT disabled."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';mod=r/'src/bridge/DecodiumBridgeAutoSequence.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';qml=r/'build/qml/decodium/Main.qml';m,n,s,q=(p.read_text() for p in (main,mod,sstv,qml));fixture=(r/'tests/bridge_refactor_fixtures/auto_sequence_before.inc').read_text();sig='void DecodiumBridge::autoSequenceStep(const QStringList& f)';head=n.split(sig,1)[0];assert n==head+fixture
rank=head[head.index('static constexpr int kFt2AsyncAutoSeqDedupeSeconds'):];anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);m_mode=QStringLiteral("FT8");m_callsign=QStringLiteral("9H1SR");m_grid=QStringLiteral("JM75");m_dxCall=QStringLiteral("K1ABC");m_filtersBypassed=true;m_manualTxHold=false;m_monitoring=false;m_transmitting=false;m_tuning=false;m_txEnabled=false;m_autoSeq=true;m_multiAnswerMode=false;m_autoCqRepeat=false;m_qsoProgress=2;m_currentTx=1;
 auto snap=[this](){return QVariantMap{{"progress",m_qsoProgress},{"currentTx",m_currentTx},{"dx",m_dxCall},{"txEnabled",m_txEnabled}};};
 QVariantMap out;out["initial"]=snap();autoSequenceStep(QStringList{});out["empty"]=snap();
 autoSequenceStep({QStringLiteral("120000"),QStringLiteral("-10"),QStringLiteral("0.1"),QStringLiteral("1500"),QStringLiteral("9H1SR K1ABC -12")});out["report"]=snap();
 autoSequenceStep({QStringLiteral("120000"),QStringLiteral("-10"),QStringLiteral("0.1"),QStringLiteral("1500"),QStringLiteral("9H1SR K1ABC -12")});out["duplicate"]=snap();
 autoSequenceStep({QStringLiteral("120010"),QStringLiteral("-10"),QStringLiteral("0.1"),QStringLiteral("1500"),QStringLiteral("9H1SR W1AW -08")});out["otherPartner"]=snap();
 autoSequenceStep({QStringLiteral("120015"),QStringLiteral("-10"),QStringLiteral("0.1"),QStringLiteral("1500"),QStringLiteral("9H1SR K1ABC RR73?")});out["lowConfidence"]=snap();
 autoSequenceStep({QStringLiteral("120030"),QStringLiteral("-10"),QStringLiteral("0.1"),QStringLiteral("1500"),QStringLiteral("K1ABC 9H1SR R-10")});out["selfEcho"]=snap();
 m_qsoProgress=2;m_currentTx=1;m_txEnabled=false;m_lastAutoSeqKey.clear();m_manualTxHold=true;out["holdBefore"]=snap();autoSequenceStep({QStringLiteral("120040"),QStringLiteral("-10"),QStringLiteral("0.1"),QStringLiteral("1500"),QStringLiteral("9H1SR K1ABC -12")});out["holdAfter"]=snap();m_manualTxHold=false;
 m_qsoProgress=2;m_currentTx=1;m_txEnabled=false;m_lastAutoSeqKey.clear();autoSequenceStep({QStringLiteral("120050"),QStringLiteral("-10"),QStringLiteral("0.1"),QStringLiteral("1500"),QStringLiteral("9H1SR K1ABC FN20")});out["grid"]=snap();
 m_qsoProgress=3;m_currentTx=2;m_txEnabled=false;m_lastAutoSeqKey.clear();autoSequenceStep({QStringLiteral("120055"),QStringLiteral("-10"),QStringLiteral("0.1"),QStringLiteral("1500"),QStringLiteral("9H1SR K1ABC R-12")});out["rogerReport"]=snap();
 m_qsoProgress=4;m_currentTx=3;m_txEnabled=false;m_lastAutoSeqKey.clear();m_lastTransmittedMessage=QStringLiteral("K1ABC 9H1SR R-10");autoSequenceStep({QStringLiteral("120100"),QStringLiteral("-10"),QStringLiteral("0.1"),QStringLiteral("1500"),QStringLiteral("9H1SR K1ABC RRR")});out["rrr"]=snap();
 m_qsoProgress=2;m_currentTx=1;m_txEnabled=false;m_lastAutoSeqKey.clear();out["unrelatedBefore"]=snap();autoSequenceStep({QStringLiteral("120105"),QStringLiteral("-10"),QStringLiteral("0.1"),QStringLiteral("1500"),QStringLiteral("W1AW K1ABC FN20")});out["unrelatedAfter"]=snap();
 for (const QString &mode : {QStringLiteral("FT4"), QStringLiteral("FT2")}) {m_mode=mode;m_qsoProgress=2;m_currentTx=1;m_txEnabled=false;m_lastAutoSeqKey.clear();m_lastAutoSeqMs=0;autoSequenceStep({QStringLiteral("120045"),QStringLiteral("-10"),QStringLiteral("0.1"),QStringLiteral("1500"),QStringLiteral("9H1SR K1ABC -12")});out[mode]=snap();}
 m_autoSeq=false;m_txEnabled=false;out["cat"]=m_catConnected;out["transmitting"]=m_transmitting;return out;}
#endif
'''
probe='''\n Timer {interval:9000;running:true;repeat:false;onTriggered:{console.log('[AUTOSEQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n''';rigs=[];results=[]
try:
 sstv.write_text(s.replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LAB_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,point+rank+fixture+'\n'));mod.write_text(head[:head.index('static constexpr int kFt2AsyncAutoSeqDedupeSeconds')])
  else:main.write_text(m);mod.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='AutoSeq'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-autoseq-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];p=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[AUTOSEQ-LAB] ',1)[1] for line in p.stdout.splitlines() if '[AUTOSEQ-LAB] ' in line]
   if p.returncode or len(rows)!=1:raise RuntimeError(p.stdout[-10000:])
   v=json.loads(rows[0]);assert v['rrr']['currentTx']==5 and v['unrelatedBefore']==v['unrelatedAfter'] and v['grid']['currentTx']==2 and v['grid']['progress']==3 and v['rogerReport']['currentTx']==3 and v['rogerReport']['progress']==4 and v['holdBefore']==v['holdAfter'] and v['FT2']['currentTx']==3 and v['FT2']['progress']==4 and v['FT4']['currentTx']==3 and v['FT4']['progress']==4 and v['report']!=v['initial'] and v['empty']==v['initial'] and v['duplicate']==v['report'] and v['otherPartner']==v['report'] and v['lowConfidence']==v['report'] and v['selfEcho']==v['report'] and not v['cat'] and not v['transmitting'],v;results.append(v);print(label,json.dumps(v,sort_keys=True),flush=True)
 assert results[0]==results[1]
finally:
 for p,v in ((main,m),(mod,n),(sstv,s),(qml,q)):p.write_text(v)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

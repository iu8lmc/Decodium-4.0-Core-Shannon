#!/usr/bin/env python3
"""Compare DecoDXLog integration initialization in isolated apps."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridgeConstructor.cpp'; mod=r/'src/bridge/DecodiumBridgeDecoLogInit.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,n,s,q=(p.read_text() for p in (main,mod,sstv,qml)); fixture=(r/'tests/bridge_refactor_fixtures/decolog_init_before.inc').read_text();assert n.split('void DecodiumBridge::initialiseDecoLogLink()\n{\n',1)[1]==fixture+'}\n'
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);QStringList statuses,errors;auto c1=connect(this,&DecodiumBridge::statusMessage,this,[&](const QString& x){statuses<<x;});auto c2=connect(this,&DecodiumBridge::errorMessage,this,[&](const QString& x){errors<<x;});QJsonArray first;first.append(QJsonArray{QStringLiteral("k1abc"),QStringLiteral("20M"),QStringLiteral("ft8"),QStringLiteral("20260101"),QStringLiteral("fn42"),1});m_decoLogLink->workedRows(first,true,false);bool staged=m_decoLogPendingRows.size()==1;QJsonArray second;second.append(QJsonArray{QStringLiteral("w1xyz"),QStringLiteral("40M"),QStringLiteral("ft4"),QStringLiteral("20260102"),QStringLiteral("fn31"),0});m_decoLogLink->workedRows(second,false,true);bool rows=m_decoLogRows.size()==2&&m_decoLogRows.at(0).call==QStringLiteral("K1ABC")&&m_decoLogRows.at(0).band==QStringLiteral("20m")&&m_decoLogPendingRows.isEmpty();QJsonArray row{QStringLiteral("k2abc"),QStringLiteral("20m"),QStringLiteral("ft8"),QStringLiteral("20260103"),QStringLiteral("fn20"),0};QJsonObject msg{{"row",row},{"source",QStringLiteral("udp_decodium")},{"status",QStringLiteral("logged")}};m_decoLogLink->qsoWritten(msg);bool logged=m_decoLogRows.size()==3&&m_workedCalls.contains(QStringLiteral("K2ABC"));msg.insert(QStringLiteral("status"),QStringLiteral("duplicate"));m_decoLogLink->qsoWritten(msg);msg.insert(QStringLiteral("status"),QStringLiteral("error"));msg.insert(QStringLiteral("message"),QStringLiteral("synthetic"));m_decoLogLink->qsoWritten(msg);bool qso=logged&&statuses.size()>=2&&errors.size()==1;QJsonObject spot{{"call",QStringLiteral("ja1yyy")},{"freqKhz",14074.0},{"mode",QStringLiteral("FT8")},{"status",QStringLiteral("NEW")}};m_decoLogLink->spotReceived(spot);bool spotted=!m_dxCluster->spots().isEmpty()&&m_dxCluster->spots().last().toMap().value(QStringLiteral("dxCall")).toString()==QStringLiteral("JA1YYY");QJsonObject award{{"ft2",QJsonObject{{"points",42}}},{"dxcc",QJsonObject{{"worked",12},{"confirmed",5}}}};m_decoLogLink->awardChanged(award);bool awardOk=m_decoLogAward.value(QStringLiteral("points")).toInt()==42&&m_decoLogAward.value(QStringLiteral("dxccAllWorked")).toInt()==12&&m_decoLogAward.value(QStringLiteral("dxccAllConfirmed")).toInt()==5;disconnect(c1);disconnect(c2);bool ok=staged&&rows&&qso&&spotted&&awardOk;return {{"wasShutdown",!ok},{"shutdown",ok},{"staged",staged},{"rows",rows},{"qso",qso},{"spot",spotted},{"award",awardOk},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''

probe='''\n Timer {interval:9000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''

rigs=[]; results=[]
try:
 sstv.write_text(s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QBuffer>\n#include "Network/DecodiumDecoLogLink.h"\n#include "DecodiumDxCluster.h"\n#include <QJsonArray>\n#include <QJsonObject>',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='    initialiseDecoLogLink();\n'; assert m.count(point)==1; prefix=n.split('\nvoid DecodiumBridge::',1)[0].split('\nnamespace {',1)[0]; prefix='\n'.join(line for line in prefix.splitlines() if not line.startswith('#define bridgeLog '))+'\n'; undef=''.join('\n#undef '+line.split()[1]+'\n' for line in prefix.splitlines() if line.startswith('#define ')); main.write_text(m.replace('#include "DecodiumBridge.h"',prefix,1).replace(point,fixture+undef,1));mod.write_text(n)
  else:main.write_text(m);mod.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='DecoLogInit'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-display-frequency-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
   sockets=[]
   for port in (2237,2239):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sockets.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
   cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000']
   result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[DISPLAY-FREQ-LAB] ',1)[1] for line in result.stdout.splitlines() if '[DISPLAY-FREQ-LAB] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-8000:])
   val=json.loads(rows[0]);assert not val['wasShutdown'] and val['shutdown'] and not val['cat'] and not val['tx'],val
   readProbe="\n Timer {interval:9000;running:true;repeat:false;onTriggered:{console.log('[SHUTDOWN-READ] '+String(bridge.getSetting('ShutdownSynthetic','missing')));Qt.quit();}}\n"
   qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+readProbe,1))
   reopened=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   persisted=[line.split('[SHUTDOWN-READ] ',1)[1] for line in reopened.stdout.splitlines() if '[SHUTDOWN-READ] ' in line]
   if reopened.returncode or persisted!=['persisted']:raise RuntimeError('persistence readback failed: '+reopened.stdout[-8000:])
   val['readback']=persisted[0];results.append(val);print(label,json.dumps(val,sort_keys=True),flush=True)
 assert results[0]==results[1]
finally:
 for p,content in ((main,m),(mod,n),(sstv,s),(qml,q)):p.write_text(content)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

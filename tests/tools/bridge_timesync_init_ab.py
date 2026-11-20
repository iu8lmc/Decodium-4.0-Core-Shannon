#!/usr/bin/env python3
"""Compare time synchronization initialization in isolated apps."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; main=r/'src/bridge/DecodiumBridgeConstructor.cpp'; mod=r/'src/bridge/DecodiumBridgeTimeSync.cpp'; sstv=r/'src/bridge/DecodiumBridgeSstv.cpp'; qml=r/'build/qml/decodium/Main.qml'
m,n,s,q=(p.read_text() for p in (main,mod,sstv,qml)); fixture=(r/'tests/bridge_refactor_fixtures/ntp_init_before.inc').read_text();assert n.split('void DecodiumBridge::initialiseTimeSync()\n{\n',1)[1]==fixture+'}\n#undef bridgeLog\n'
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'; assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);int offsetNotices=0,syncNotices=0;auto c1=connect(this,&DecodiumBridge::ntpOffsetMsChanged,this,[&](){++offsetNotices;});auto c2=connect(this,&DecodiumBridge::ntpSyncedChanged,this,[&](){++syncNotices;});m_ntpEnabled=true;m_ntpSynced=true;m_monitoring=true;m_mode=QStringLiteral("FT8");m_lastTxEndMs=QDateTime::currentMSecsSinceEpoch();m_ntpOffsetMs=0;quint64 seq=m_periodTimerSessionId;m_ntpClient->offsetUpdated(123.0);bool offset=m_ntpOffsetMs==123.0&&globalNtpOffsetMs()==123&&offsetNotices==1;bool held=m_periodTimerSessionId==seq;m_ntpClient->offsetUpdated(123.0);bool dedup=offsetNotices==1;m_monitoring=false;m_ntpClient->syncStatusChanged(false,QStringLiteral("synthetic"));bool unsynced=!m_ntpSynced;m_ntpClient->syncStatusChanged(true,QStringLiteral("synthetic"));bool synced=m_ntpSynced&&syncNotices==2;m_ntpEnabled=false;m_ntpClient->syncStatusChanged(true,QStringLiteral("synthetic"));bool disabled=!m_ntpSynced&&syncNotices==3;disconnect(c1);disconnect(c2);setGlobalNtpOffsetMs(0);bool ok=offset&&held&&dedup&&unsynced&&synced&&disabled;return {{"wasShutdown",!ok},{"shutdown",ok},{"offset",offset},{"held",held},{"dedup",dedup},{"unsynced",unsynced},{"synced",synced},{"disabled",disabled},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''

probe='''\n Timer {interval:9000;running:true;repeat:false;onTriggered:{bridge.setSetting("ShutdownSynthetic", "persisted");console.log('[DISPLAY-FREQ-LAB] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''

rigs=[]; results=[]
try:
 sstv.write_text(s.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include <QBuffer>\n#include "Network/NtpClient.hpp"\n#include "PrecisionTime.hpp"',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='    initialiseTimeSync();\n'; assert m.count(point)==1; prefix=n.split('\nvoid DecodiumBridge::',1)[0].split('\nnamespace {',1)[0]; prefix='\n'.join(line for line in prefix.splitlines() if not line.startswith('#define bridgeLog '))+'\n'; undef=''.join('\n#undef '+line.split()[1]+'\n' for line in prefix.splitlines() if line.startswith('#define ')); main.write_text(m.replace('#include "DecodiumBridge.h"',prefix,1).replace(point,fixture+undef,1));mod.write_text(n)
  else:main.write_text(m);mod.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='TimeSyncInit'+secrets.token_hex(5);rigs.append(rig)
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

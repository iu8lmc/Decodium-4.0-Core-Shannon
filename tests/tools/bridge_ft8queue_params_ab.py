#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeFt8Queue.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/ft8queue_before.inc').read_text();mark='void DecodiumBridge::queueFt8DecodeRequest(';head=n.split(mark,1)[0];assert n==head+fixture
needle='    auto* worker = m_ft8Worker;';assert fixture.count(needle)==1
instrument='    qWarning("[FT8-REQUEST] serial=%llu depth=%d threads=%d ap=%d nz=%d min=%d max=%d budget=%d pressure=%d", static_cast<unsigned long long>(req.serial),req.ndepth,req.threadCount,req.lft8apon,req.nzhsym,req.nfa,req.nfb,req.maxDecodeMs,req.cpuPressureLimited ? 1 : 0); return;\n'
fixture_test=fixture.replace(needle,instrument+needle)
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
    {Q_UNUSED(values);m_mode=QStringLiteral("FT8");m_monitoring=false;m_transmitting=false;m_tuning=false;m_cpuPressureUntilMs=0;m_cpuPressureSevereUntilMs=0;m_lowCpuModeEnabled=false;ensureDecodeWorkerForMode(m_mode);
    QVector<short> pcm(240,100);queueFt8DecodeRequest(pcm,101,120000,1,4,0,0,90,true,false,false,1000,true,1);
    m_transmitting=true;queueFt8DecodeRequest(pcm,102,120000,1,4,0,0,90,true,false,false,1000,true,1);m_transmitting=false;
    m_lowCpuModeEnabled=true;queueFt8DecodeRequest(pcm,103,120000,1,4,0,0,90,true,false,false,1000,true,1);m_lowCpuModeEnabled=false;
    return {{"worker",m_ft8Worker!=nullptr},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{bridge.mode="FT2";bridge.callsign="9H1SR";console.log('[AUDIO-SINK-TAPS] '+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
rigs=[];values=[]
try:
 sstv.write_text(s.replace('#include <QTimer>','#include <QTimer>\n#include <QEventLoop>',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_UTC_TIMER_INSERTION_POINT';assert m.count(point)==1;main.write_text(m.replace(point,fixture_test+point));module.write_text(head)
  else:main.write_text(m);module.write_text(head+fixture_test)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='Ft8QueueParams'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-ft8queueparams-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   rows=[line.split('[AUDIO-SINK-TAPS] ',1)[1] for line in result.stdout.splitlines() if '[AUDIO-SINK-TAPS] ' in line]
   if result.returncode or len(rows)!=1:raise RuntimeError(result.stdout[-6000:])
   value=json.loads(rows[0]);assert value=={'worker':True,'cat':False,'tx':False},value
   import re
   requests=[tuple(map(int,match.groups())) for line in result.stdout.splitlines() if (match:=re.search(r'\[FT8-REQUEST\] serial=(\d+) depth=(\d+) threads=(\d+) ap=(\d+) nz=(\d+) min=(\d+) max=(\d+) budget=(\d+) pressure=(\d+)',line))]
   assert len(requests)==3,requests
   for x in requests:assert x[2]==1 and x[4]==50 and x[7]<=1000,x
   assert requests[1][1]<=2 and requests[1][3]==0,requests
   assert requests[2][1]<=2 and requests[2][3]==0 and requests[2][8]==1,requests
   values.append(requests);print(label,requests,flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

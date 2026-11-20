#!/usr/bin/env python3
"""Exercise decode/TX wait conditions in isolated original/refactored apps."""
import json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeLogPromptDialog.cpp';qml=r/'build/qml/decodium/Main.qml';m=main.read_text();s=sstv.read_text();n=module.read_text();q=qml.read_text();fixture=(r/'tests/bridge_refactor_fixtures/log_prompt_dialog_before.inc').read_text();mark='static QString satelliteCodeFromDisplayText(';head=n.split(mark,1)[0];assert n==head+fixture
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{';assert s.count(anchor)==1
inj='''
#if DECODIUM_HAS_SSTV
 {Q_UNUSED(values);m_promptLogSnapshotValid=true;m_promptLogCall=QStringLiteral("K1ABC");m_promptLogGrid=QStringLiteral("FN42");m_promptLogMode=QStringLiteral("FT8");m_promptLogDialFreq=14074000;m_promptLogOn=QDateTime::currentDateTimeUtc();m_promptLogOff=m_promptLogOn;m_logPromptOpen=true;m_logPromptShownByQml=false;showLogQsoPromptDialog();auto* dialog=m_logQsoPromptDialog.data();if(!dialog)return {{"clicked",false}};auto edits=dialog->findChildren<QLineEdit*>();auto* buttons=dialog->findChild<QDialogButtonBox*>();if(edits.size()<3||!buttons)return {{"clicked",false}};edits[2]->setText(QStringLiteral("synthetic dialog note"));auto boxes=dialog->findChildren<QCheckBox*>();bool spotDisabled=boxes.size()==1 && !boxes[0]->isEnabled() && !boxes[0]->isChecked();buttons->button(QDialogButtonBox::Ok)->click();return {{"spotDisabled",spotDisabled},{"clicked",true},{"closed",m_logQsoPromptDialog.isNull()},{"cat",m_catConnected},{"tx",m_transmitting}};}
#endif
'''
probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{if(!bridge.createLogbook("DialogSynthetic",false) || String(bridge.activeLogbookPath).indexOf("/.qttest/")<0){console.log("[DIALOG-START] unsafe");Qt.quit();return;}console.log("[DIALOG-START] "+JSON.stringify(bridge.logSstvQso({})));check.start();}}
 Timer {id:check;interval:250;repeat:true;property int ticks:0;onTriggered:{var rows=bridge.searchQsos("K1ABC","","","","");if(rows.length>0){console.log("[DIALOG-SAVED] "+JSON.stringify({count:rows.length,comment:rows[0].comment,call:rows[0].call}));stop();Qt.quit();}else if(++ticks>=40){console.log("[DIALOG-SAVED] timeout");stop();Qt.quit();}}}
'''
rigs=[];values=[]
try:
 sstv.write_text(s.replace('#include <QTimer>','#include <QTimer>\n#include <QDialog>\n#include <QLabel>\n#include <QDialogButtonBox>\n#include <QPushButton>\n#include <QLineEdit>\n#include <QCheckBox>',1).replace(anchor,anchor+inj,1))
 for label in ('before','after'):
  if label=='before':
   point='// BRIDGE_LOG_QSO_INSERTION_POINT\n';assert m.count(point)==1;main.write_text(m.replace(point,fixture+point));module.write_text(head)
  else:main.write_text(m);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);qml.write_text(q.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1));rig='LogPromptClusterOffline'+secrets.token_hex(5);rigs.append(rig)
  with tempfile.TemporaryDirectory(prefix='decodium-modecleanup-') as tmp:
   env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software');socks=[]
   for port in (2237,2239):sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));socks.append(sock)
   policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','25000'];result=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
   starts=[line.split('[DIALOG-START] ',1)[1] for line in result.stdout.splitlines() if '[DIALOG-START] ' in line];saved=[line.split('[DIALOG-SAVED] ',1)[1] for line in result.stdout.splitlines() if '[DIALOG-SAVED] ' in line]
   if result.returncode or len(starts)!=1 or len(saved)!=1 or saved[0]=='timeout':raise RuntimeError(result.stdout[-8000:])
   value=json.loads(starts[0]);record=json.loads(saved[0]);assert value['spotDisabled'] and value['clicked'] and value['closed'] and not value['cat'] and not value['tx'] and record=={'count':1,'comment':'synthetic dialog note','call':'K1ABC'},(value,record);values.append((value,record));print(label,json.dumps([value,record],sort_keys=True),flush=True)
 assert values[0]==values[1]
finally:
 main.write_text(m);sstv.write_text(s);module.write_text(n);qml.write_text(q)
 for rig in rigs:
  for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
   p=parent/('Decodium - '+rig+' - test')
   if p.is_dir():shutil.rmtree(p)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

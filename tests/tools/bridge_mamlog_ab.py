#!/usr/bin/env python3
"""Compare MAM log snapshot, duplicate handling and cooldown behaviour."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/mamlog_before.inc').read_text();new='void DecodiumBridge::mamLogSlot(MamQsoSlot& s)'+(r/'src/bridge/DecodiumBridgeMamLog.cpp').read_text().split('void DecodiumBridge::mamLogSlot(MamQsoSlot& s)',1)[1];new=new.replace('decodiumBridgeMamQueueLog(', 'bridgeLog(')
if old!=new:raise SystemExit('MAM log differs')
src=r'''
#include <QString>
#include <QDateTime>
#include <QHash>
#include <iostream>
void bridgeLog(QString const&){}
struct MamQsoSlot{enum class State{Active,Done};bool logged=false;State state=State::Active;QString call,callFull,grid,reportSent,reportReceived;qint64 startedOnMs=0;};
class DecodiumBridge{public:
 bool m_pendingAutoLogValid=false,m_qsoLogged=true;QString m_pendingAutoLogCall,m_pendingAutoLogGrid,m_pendingAutoLogRptSent,m_pendingAutoLogRptRcvd,m_mode="FT8",captured;QDateTime m_pendingAutoLogOn,m_pendingAutoLogOff;double m_pendingAutoLogDialFreq=0,m_frequency=14074000;QHash<QString,qint64>m_qsoCooldown;int logs=0,clear=0,worked=0;
 void logQsoNow(){++logs;captured=m_pendingAutoLogCall+"|"+m_pendingAutoLogGrid+"|"+m_pendingAutoLogRptSent+"|"+m_pendingAutoLogRptRcvd+"|"+QString::number(m_pendingAutoLogDialFreq,'f',0)+"|"+QString::number(m_pendingAutoLogValid)+"|"+QString::number(m_pendingAutoLogOff>=m_pendingAutoLogOn);}
 void clearPendingAutoLogSnapshot(){++clear;m_pendingAutoLogValid=false;}void rememberRecentAutoCqWorked(QString const&,double,QString const&){++worked;}void mamLogSlot(MamQsoSlot&);
};
#include "method.inc"
int main(){DecodiumBridge b;MamQsoSlot s;s.call="K1ABC";s.callFull="K1ABC/P";s.grid=" FN42 ";s.reportSent=" -12 ";s.reportReceived=" -08 ";s.startedOnMs=QDateTime::currentMSecsSinceEpoch()-1000;b.mamLogSlot(s);b.mamLogSlot(s);std::cout<<b.captured.toStdString()<<','<<b.logs<<','<<b.clear<<','<<b.worked<<','<<s.logged<<','<<(s.state==MamQsoSlot::State::Done)<<','<<b.m_qsoCooldown.contains("K1ABC")<<'\n';MamQsoSlot empty;b.mamLogSlot(empty);std::cout<<empty.logged<<','<<(empty.state==MamQsoSlot::State::Done)<<','<<b.logs<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-mamlog-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
expected='K1ABC/P|FN42|-12|-08|14074000|1|1,1,1,1,1,1,1\n1,1,1\n'
if outputs!=[expected,expected]:raise SystemExit('MAM log A/B mismatch '+repr(outputs))
print('MAM log A/B OK\n'+expected,end='')

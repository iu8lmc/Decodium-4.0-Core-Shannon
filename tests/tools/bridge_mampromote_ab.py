#!/usr/bin/env python3
"""Compare MAM queue promotion and its duplicate/frequency policies."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/mampromote_before.inc').read_text();new='void DecodiumBridge::mamPromoteFromQueue()'+(r/'src/bridge/DecodiumBridgeMamPromote.cpp').read_text().split('void DecodiumBridge::mamPromoteFromQueue()',1)[1];new=new.replace('decodiumBridgeMamQueueLog(', 'bridgeLog(')
if old!=new:raise SystemExit('MAM promotion differs')
src=r'''
#include <QString>
#include <QStringList>
#include <QVector>
#include <QSet>
#include <QDateTime>
#include <iostream>
#define emit
void bridgeLog(QString const&){} QString normalizedBaseCall(QString s){return s.trimmed().toUpper();}QString formatSignalReport(int n){return QString::number(n);}
struct MamQsoSlot{QString call,callFull,reportSent,reportReceived;int audioFreqHz=0,progress=0,currentTx=0,partnerSnrDb=0;qint64 startedOnMs=0,lastHeardMs=0;};
class DecodiumBridge{public:
 QString m_callsign="9H1SR",m_mode="FT8";double m_frequency=14074000;int m_mamMaxStreams=3,m_rxFrequency=1500,changed=0,queueChanged=0,ranked=0,recovered=0;QVector<MamQsoSlot>m_mamSlots;QStringList m_callerQueue;QSet<QString>m_qsoCooldown;
 void mamPromoteLateReportRecoveries(){++recovered;}void rankCallerQueue(QString const&){++ranked;}void callerQueueChanged(){++queueChanged;}void mamActiveSlotsChanged(){++changed;}int mamMinStreamGapHz()const{return 50;}bool isRecentAutoCqDuplicate(QString const&s,double,QString const&)const{return s=="OLD";}int mamSlotIndexForCall(QString const&s)const{for(int i=0;i<m_mamSlots.size();++i)if(m_mamSlots[i].call==s)return i;return -1;}void mamPromoteFromQueue();
};
#include "method.inc"
int main(){DecodiumBridge b;b.m_qsoCooldown.insert("COOL");b.m_callerQueue={" ","9H1SR 900 -5","OLD 900 -5","COOL 900 -5","K1ABC 1000 -10","K1ABC 1300 -10","W1AW 1020 -8","K2AAA 2000 -80"};b.mamPromoteFromQueue();std::cout<<b.m_mamSlots.size()<<','<<b.m_callerQueue.size()<<','<<b.m_callerQueue.first().toStdString()<<','<<b.queueChanged<<','<<b.changed<<'\n';b.m_mamSlots.clear();b.m_callerQueue.removeFirst();b.mamPromoteFromQueue();auto s=b.m_mamSlots.first();std::cout<<s.call.toStdString()<<','<<s.audioFreqHz<<','<<s.partnerSnrDb<<','<<s.reportSent.toStdString()<<','<<s.currentTx<<','<<b.recovered<<','<<b.ranked<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-mampromote-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
expected='1,2,W1AW 1020 -8,6,1\nK2AAA,2000,-50,-50,2,2,2\n'
if outputs!=[expected,expected]:raise SystemExit('MAM promotion A/B mismatch '+repr(outputs))
print('MAM promotion A/B OK\n'+expected,end='')

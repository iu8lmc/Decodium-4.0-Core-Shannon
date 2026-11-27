#!/usr/bin/env python3
"""Compare MAM late-report recovery, capacity and frequency arbitration."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/mamrecovery_before.inc').read_text();new='void DecodiumBridge::mamPromoteLateReportRecoveries()'+(r/'src/bridge/DecodiumBridgeMamRecovery.cpp').read_text().split('void DecodiumBridge::mamPromoteLateReportRecoveries()',1)[1];new=new.replace('decodiumBridgeMamQueueLog(', 'bridgeLog(')
if old!=new:raise SystemExit('MAM recovery differs')
src=r'''
#include <QString>
#include <QVector>
#include <QDateTime>
#include <iostream>
#define emit
void bridgeLog(QString const&){}
struct MamQsoSlot{QString call,callFull;int audioFreqHz=0,currentTx=0;};struct Recovery{qint64 expiresMs;bool readyForSlot;MamQsoSlot slot;};
class DecodiumBridge{public:
 int m_mamMaxStreams=2,slotChanged=0,queueChanged=0;double m_frequency=14074000;QString m_mode="FT8";QVector<MamQsoSlot>m_mamSlots;QVector<Recovery>m_mamLateReportRecoveries;
 bool isRecentMamWorkedOrLoggedDuplicate(QString const&s,double,QString const&)const{return s=="OLD";}int mamMinStreamGapHz()const{return 50;}void mamActiveSlotsChanged(){++slotChanged;}void callerQueueChanged(){++queueChanged;}void mamPromoteLateReportRecoveries();
};
#include "method.inc"
int main(){DecodiumBridge b;qint64 now=QDateTime::currentMSecsSinceEpoch();b.m_mamSlots={{"A","A",1000,2}};b.m_mamLateReportRecoveries={{now-1,true,{"EXP","EXP",2000,5}},{now+60000,false,{"WAIT","WAIT",2100,5}},{now+60000,true,{"OLD","OLD",2200,5}},{now+60000,true,{"BUSY","BUSY",1020,5}},{now+60000,true,{"GOOD","GOOD",2400,5}},{now+60000,true,{"LATER","LATER",2600,5}}};b.mamPromoteLateReportRecoveries();std::cout<<b.m_mamSlots.last().call.toStdString()<<','<<b.m_mamLateReportRecoveries.size()<<','<<b.slotChanged<<','<<b.queueChanged<<'\n';b.mamPromoteLateReportRecoveries();std::cout<<b.m_mamLateReportRecoveries.size()<<','<<b.slotChanged<<','<<b.queueChanged<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-mamrecovery-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
expected='GOOD,3,1,1\n3,1,1\n'
if outputs!=[expected,expected]:raise SystemExit('MAM recovery A/B mismatch '+repr(outputs))
print('MAM recovery A/B OK\n'+expected,end='')

#!/usr/bin/env python3
"""Compare MAM slot pruning and retained recovery snapshots."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/mamprune_before.inc').read_text();new='void DecodiumBridge::mamPruneSlots()'+(r/'src/bridge/DecodiumBridgeMamPrune.cpp').read_text().split('void DecodiumBridge::mamPruneSlots()',1)[1];new=new.replace('decodiumBridgeMamQueueLog(', 'bridgeLog(').replace('decodiumBridgeQsoCooldownWindowMs()', 'kQsoCooldownWindowMs')
if old!=new:raise SystemExit('MAM prune differs')
src=r'''
#include <QString>
#include <QStringList>
#include <QHash>
#include <QVector>
#include <QDateTime>
#include <iostream>
#define emit
constexpr qint64 kQsoCooldownWindowMs=30000;void bridgeLog(QString const&){}
struct MamQsoSlot{enum class State{Active,Done};QString call,callFull;bool logged=false,weakFinalAck=false;int retryCount=0,lateResumes=0,currentTx=0,progress=0,lastNtx=0;qint64 lastHeardMs=0,lastTxSlotMs=0;State state=State::Active;};
struct MamResumeCarry{int lateResumes=0;qint64 expiresMs=0;};struct MamLateReportRecovery{MamQsoSlot slot;qint64 expiresMs=0;bool readyForSlot=false;};
class DecodiumBridge{public:
 QHash<QString,qint64>m_qsoCooldown;QHash<QString,int>m_postLogReengageCount;QHash<QString,MamLateReportRecovery>m_mamLateReportRecoveries;QHash<QString,MamResumeCarry>m_mamResumeCarry;QVector<MamQsoSlot>m_mamSlots;QStringList m_callerQueue;bool m_ft2LogRr73OnPartnerLeft=false;double m_frequency=0;QString m_mode="FT2";int slotChanged=0,queueChanged=0,abandoned=0,logged=0;
 qint64 periodMsForMode(QString const&)const{return 15000;}int maxCallerRetries()const{return 2;}void rememberRecentAutoCqAbandoned(QString const&,double,QString const&){++abandoned;}void mamLogSlot(MamQsoSlot&s){s.logged=true;++logged;}void mamActiveSlotsChanged(){++slotChanged;}void callerQueueChanged(){++queueChanged;}void mamPruneSlots();
};
#include "method.inc"
int main(){DecodiumBridge b;qint64 now=QDateTime::currentMSecsSinceEpoch();b.m_qsoCooldown.insert("STALE",now-31000);b.m_postLogReengageCount.insert("STALE",1);MamLateReportRecovery expired;expired.expiresMs=now-1;expired.readyForSlot=true;b.m_mamLateReportRecoveries.insert("EXPIRED",expired);MamQsoSlot a;a.call=a.callFull="K1ABC";a.progress=3;a.currentTx=3;a.retryCount=3;a.lateResumes=1;b.m_mamSlots.append(a);MamQsoSlot c;c.call=c.callFull="W1AW";c.progress=1;c.lastHeardMs=now;b.m_mamSlots.append(c);b.mamPruneSlots();auto v=b.m_mamLateReportRecoveries.value("K1ABC");std::cout<<b.m_mamSlots.size()<<','<<b.m_mamSlots.first().call.toStdString()<<','<<b.m_mamLateReportRecoveries.size()<<','<<v.slot.retryCount<<','<<v.slot.lastNtx<<','<<b.m_mamResumeCarry.value("K1ABC").lateResumes<<','<<b.abandoned<<','<<b.slotChanged<<','<<b.queueChanged<<','<<b.m_qsoCooldown.size()<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-mamprune-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
expected='1,W1AW,1,0,-1,1,1,1,1,0\n'
if outputs!=[expected,expected]:raise SystemExit('MAM prune A/B mismatch '+repr(outputs))
print('MAM prune A/B OK\n'+expected,end='')

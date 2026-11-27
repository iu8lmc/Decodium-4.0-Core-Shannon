#!/usr/bin/env python3
"""Compare TX request guard and cancellation with controlled state."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/txdispatchguard_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeTxDispatchGuard.cpp').read_text();mark='void DecodiumBridge::armExplicitManualTxRequest(';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QString>
#include <QStringList>
#include <iostream>
#define emit
struct QDateTime {static qint64 currentMSecsSinceEpoch(){return 1000000;}};
QStringList events;void bridgeLog(const QString&s){events<<s;}
struct Gate{int resets=0;void resetKeepingSpeed(){resets++;}};
class DecodiumBridge{public:int m_explicitManualTxRequest=0,m_currentTx=6,m_pendingAutoSeqTxAfterActiveTx=0,m_ft2AsyncSmartTxRetries=0;bool m_manualTxHold=false,m_startupTxGuard=false,m_autoCqRepeat=false,m_multiAnswerMode=false,m_txEnabled=true,m_periodicTxCheckScheduled=false,m_syncTxRetryScheduled=false,m_deferredManualSyncTx=false,m_ft2AsyncSmartTxPending=false;quint64 m_txRequestGeneration=0,m_syncTxRetrySerial=0,m_ft2AsyncSmartTxSerial=0;long long m_ft2AsyncSmartTxDeadlineMs=0;Gate m_ft8AutoTxDecodeGate;void txEnabledChanged(){events<<"notify";}void armExplicitManualTxRequest(int,const QString&);bool txDispatchGuardAllows(const QString&);void invalidateDeferredTxRequests(const QString&);};
#include "methods.inc"
void show(const char*label,DecodiumBridge&b,bool ok){std::cout<<label<<':'<<ok<<':'<<b.m_explicitManualTxRequest<<':'<<b.m_txEnabled<<':'<<b.m_txRequestGeneration<<':'<<b.m_syncTxRetrySerial<<':'<<b.m_ft2AsyncSmartTxSerial<<':'<<b.m_ft8AutoTxDecodeGate.resets<<':'<<b.m_periodicTxCheckScheduled<<':'<<b.m_syncTxRetryScheduled<<':'<<b.m_deferredManualSyncTx<<':'<<b.m_ft2AsyncSmartTxPending<<':'<<b.m_ft2AsyncSmartTxDeadlineMs<<':'<<b.m_ft2AsyncSmartTxRetries<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge b;b.armExplicitManualTxRequest(7,"invalid");show("invalid",b,false);show("stale6",b,b.txDispatchGuardAllows("probe"));b.armExplicitManualTxRequest(6,"button");b.m_manualTxHold=true;b.m_startupTxGuard=true;show("manual",b,b.txDispatchGuardAllows("probe"));b.m_currentTx=1;show("hold",b,b.txDispatchGuardAllows("probe"));b.m_manualTxHold=false;show("startup",b,b.txDispatchGuardAllows("probe"));b.m_startupTxGuard=false;show("normal",b,b.txDispatchGuardAllows("probe"));b.m_currentTx=6;b.m_autoCqRepeat=true;show("autocq",b,b.txDispatchGuardAllows("probe"));b.m_autoCqRepeat=false;b.m_multiAnswerMode=true;show("multi",b,b.txDispatchGuardAllows("probe"));b.m_periodicTxCheckScheduled=b.m_syncTxRetryScheduled=b.m_deferredManualSyncTx=b.m_ft2AsyncSmartTxPending=true;b.m_explicitManualTxRequest=2;b.m_ft2AsyncSmartTxDeadlineMs=9;b.m_ft2AsyncSmartTxRetries=4;b.invalidateDeferredTxRequests("test");show("invalidate",b,false);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-txguard-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==9
assert lines[0].startswith('invalid:0:0:') and lines[1].startswith('stale6:0:0:0:') and 'notify' in lines[1]
assert lines[2].startswith('manual:1:6:') and lines[3].startswith('hold:0:0:') and lines[4].startswith('startup:0:')
assert all(lines[i].split(':')[1]=='1' for i in (5,6,7))
assert lines[8].startswith('invalidate:0:0:0:1:1:1:1:0:0:0:0:0:0|') and 'invalidated deferred TX requests' in lines[8]
print('TX dispatch A/B: manual and stale requests, TX6 authorization, startup/hold guards and generation invalidation passed')

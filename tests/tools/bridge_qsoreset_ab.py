#!/usr/bin/env python3
"""Compare QSO reset state and preservation of automatic TX."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/qsoreset_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeQsoReset.cpp').read_text();mark='void DecodiumBridge::resetQsoStateForNextSequence(';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QDateTime>
#include <QString>
#include <iostream>
class DecodiumBridge{public:QString m_courtesySignoffBase="x",m_pendingAutoSeqPartnerBase="x",m_pendingAutoSeqMessage="x",m_pendingAutoSeqMode="x",m_autoSeqRogerReportBase="x",dx="x",grid="x";bool m_qsoLogged=true,m_logAfterOwn73=true,m_ft2DeferredLogPending=true,m_quickPeerSignaled=true,tx=true;QDateTime m_qsoFirstReplyOn=QDateTime::currentDateTimeUtc();int m_pendingAutoSeqTxAfterActiveTx=2,m_qsoProgress=2,m_nTx73=1,m_txRetryCount=2,m_lastNtx=4,m_lastCqPidx=4,m_currentPartnerSnrDb=5,invalid=0,completed=0,progress=0,lock=0;void invalidateDeferredTxRequests(const QString&){invalid++;}void noteAutoCallQsoCompleted(){completed++;}void setTxEnabled(bool v){tx=v;}void qsoProgressChanged(){progress++;}void clearAutoCqPartnerLock(){lock++;}void setDxCall(const QString&v){dx=v;}void setDxGrid(const QString&v){grid=v;}void resetQsoStateForNextSequence(bool);};
#include "methods.inc"
int main(){for(bool preserve:{false,true}){DecodiumBridge b;b.resetQsoStateForNextSequence(preserve);std::cout<<b.tx<<':'<<b.invalid<<':'<<b.completed<<':'<<b.m_qsoProgress<<':'<<b.progress<<':'<<b.lock<<':'<<b.dx.isEmpty()<<':'<<b.grid.isEmpty()<<':'<<b.m_courtesySignoffBase.isEmpty()<<':'<<b.m_pendingAutoSeqPartnerBase.isEmpty()<<':'<<b.m_pendingAutoSeqMessage.isEmpty()<<':'<<b.m_pendingAutoSeqMode.isEmpty()<<':'<<b.m_qsoLogged<<':'<<b.m_qsoFirstReplyOn.isValid()<<':'<<b.m_currentPartnerSnrDb<<':'<<b.m_lastNtx<<':'<<b.m_lastCqPidx<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-qsoreset-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='0:1:1:6:1:1:1:1:1:1:1:1:0:0:127:-1:-1\n1:1:1:6:1:1:1:1:1:1:1:1:0:0:127:-1:-1\n',outputs
print('QSO reset A/B: TX preservation, stale state cleanup and notifications passed')

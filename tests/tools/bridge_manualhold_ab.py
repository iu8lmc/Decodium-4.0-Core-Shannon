#!/usr/bin/env python3
"""Compare manual TX hold, resume arming and deferred state handling."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/manualhold_before.inc').read_text();new='void DecodiumBridge::engageManualTxHold('+(r/'src/bridge/DecodiumBridgeManualHold.cpp').read_text().split('void DecodiumBridge::engageManualTxHold(',1)[1];new=new.replace('decodiumBridgeManualHoldLog(', 'bridgeLog(');assert old==new
src=r'''
#include <QString>
#include <QDateTime>
#include <iostream>
QString normalizedBaseCall(const QString&s){return s.section('/',0,0);}void bridgeLog(const QString&){}
struct DecodiumBridge{bool m_manualTxHold=false,m_transmitting=false,m_txEnabled=false,m_deferredManualSyncTx=false,m_resumeQsoOnReply=true,m_logAfterOwn73=true,m_ft2DeferredLogPending=true,m_asyncTxEnabled=false,m_quickPeerSignaled=true,preserve=false;QString m_mode="FT8",m_dxCall="K1ABC/P",m_resumeTargetCall,m_resumeArmedMode,m_pendingAutoSeqPartnerBase="K1ABC",m_pendingAutoSeqMessage="msg",m_pendingAutoSeqMode="FT8",m_autoSeqRogerReportBase="K1ABC";int m_currentTx=3,m_pendingAutoSeqTxAfterActiveTx=0,m_qsoProgress=3,m_resumeArmedTx=0,m_resumeArmedProgress=0,m_txRetryCount=5,m_lastNtx=4,m_txWatchdogTicks=3,m_autoCQPeriodsMissed=2,clearResume=0,locks=0,queue=0,abort=0;qint64 m_resumeArmedMs=0;void clearResumeQsoOnReplyArm(const QString&){clearResume++;}void clearAutoCqPartnerLock(){locks++;}void clearPendingAutoLogSnapshot(){}void clearLateAutoLogSnapshot(){}void clearFt2AsyncAbortQsoState(const QString&){abort++;}bool shouldPreserveDeferredAutoSeqTxForRearm(QString*){return preserve;}void clearCallerQueue(){queue++;}void engageManualTxHold(const QString&,bool);void clearManualTxHold(const QString&);};
#include "methods.inc"
int main(){for(int k=0;k<8;k++){DecodiumBridge b;if(k==1)b.m_mode="JT65";if(k==2)b.m_dxCall="";if(k==3)b.m_resumeQsoOnReply=false;if(k==4){b.m_mode="FT2";b.m_asyncTxEnabled=true;}if(k==5){b.preserve=true;b.m_pendingAutoSeqTxAfterActiveTx=4;}if(k==6){b.preserve=true;b.m_pendingAutoSeqTxAfterActiveTx=3;}if(k==7)b.m_qsoProgress=0;b.engageManualTxHold("test",k%2==0);std::cout<<b.m_manualTxHold<<':'<<(!b.m_resumeTargetCall.isEmpty())<<':'<<b.clearResume<<':'<<b.queue<<':'<<b.abort<<':'<<b.m_pendingAutoSeqTxAfterActiveTx<<':'<<b.m_quickPeerSignaled<<':'<<b.m_txRetryCount<<':'<<b.locks<<'\n';b.clearManualTxHold("test");std::cout<<b.m_manualTxHold<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-manualhold-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'methods.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1],out
lines=out[0].splitlines();assert len(lines)==16 and lines[0]=='1:1:0:1:0:0:0:0:1' and lines[10]=='1:1:0:0:0:4:1:0:1' and all(lines[i]=='0' for i in range(1,16,2)),lines
print('manual hold A/B: resume eligibility, queue and deferred TX state passed')

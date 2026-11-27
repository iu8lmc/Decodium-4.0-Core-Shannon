#!/usr/bin/env python3
"""Compare deferred AutoSeq state transitions using controlled dependencies."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/deferredseq_before.inc').read_text();new='void DecodiumBridge::armFt2AutoCqOneShotAfterCompletedTx('+(r/'src/bridge/DecodiumBridgeAutoSeqAfterTx.cpp').read_text().split('void DecodiumBridge::armFt2AutoCqOneShotAfterCompletedTx(',1)[1];new=new.replace('decodiumBridgeDeferredSeqLog(', 'bridgeLog(');assert old==new
src=r'''
#include <QObject>
#include <QDateTime>
#include <QString>
#include <iostream>
namespace Radio {QString base_callsign(const QString&s){return s.section('/',0,0);}}bool messageContainsCallToken(const QString&m,const QString&,const QString&b){return m.contains(b);}void bridgeLog(const QString&){}
struct Legacy{int disabled=0;void setTxEnabled(bool x){if(!x)disabled++;}};
struct DecodiumBridge{QString m_mode="FT2",m_dxCall="K1ABC",m_pendingAutoSeqPartnerBase="K1ABC",m_pendingAutoSeqMessage="K1ABC -10",m_pendingAutoSeqMode="FT2";bool m_asyncTxEnabled=true,m_autoCqRepeat=true,m_multiAnswerMode=false,m_ft2LogRr73OnPartnerLeft=true,m_ft2DeferredLogPending=false,m_txEnabled=true,m_logAfterOwn73=false,legacy=false,advance=true;int m_nTx73=1,m_pendingAutoSeqTxAfterActiveTx=3,m_currentTx=6,m_txRetryCount=4,m_txWatchdogTicks=4,m_autoCQPeriodsMissed=4,m_lastNtx=0,armed=0,snapshot=0,notifications=0,refresh=0,lock=0; qint64 m_asyncLastTxEndMs=0;Legacy instance,*m_legacyBackend=&instance;void capturePendingAutoLogSnapshot(){snapshot++;}void armFt2AutoCqAwaitingPartnerDecode(int,const QString&){armed++;}void txEnabledChanged(){notifications++;}void clearDeferredManualSyncTx(const QString&){}bool usingLegacyBackendForTx(){return legacy;}bool legacyBackendAvailable(){return true;}void scheduleLegacyStateRefreshBurst(){refresh++;}bool advanceQsoState(int n){if(!advance)return false;m_currentTx=n;return true;}void setTxEnabled(bool x){m_txEnabled=x;}void updateAutoCqPartnerLock(){lock++;}void armFt2AutoCqOneShotAfterCompletedTx(int,const QString&);bool applyPendingAutoSeqTxAfterCompletedTx(int);};
#include "methods.inc"
int main(){for(int k=0;k<10;k++){DecodiumBridge b;if(k==0)b.m_pendingAutoSeqTxAfterActiveTx=0;if(k==1)b.m_pendingAutoSeqMode="FT8";if(k==2)b.m_pendingAutoSeqPartnerBase="W1AW";if(k==3)b.m_pendingAutoSeqMessage="W1AW -10";if(k==4)b.m_pendingAutoSeqTxAfterActiveTx=2;if(k==5)b.m_pendingAutoSeqTxAfterActiveTx=5;if(k==6)b.advance=false;if(k==7){b.m_mode="FT8";b.m_pendingAutoSeqMode="FT8";}if(k==8){b.m_pendingAutoSeqTxAfterActiveTx=2;b.m_autoCqRepeat=false;b.m_txEnabled=false;}if(k==9)b.m_currentTx=3;bool applied=b.applyPendingAutoSeqTxAfterCompletedTx(6);std::cout<<applied<<':'<<b.m_pendingAutoSeqTxAfterActiveTx<<':'<<b.m_currentTx<<':'<<b.m_txEnabled<<':'<<b.armed<<':'<<b.m_logAfterOwn73<<':'<<(b.m_asyncLastTxEndMs>0)<<':'<<b.lock<<'\n';}DecodiumBridge b;b.legacy=true;b.armFt2AutoCqOneShotAfterCompletedTx(4,"test");std::cout<<"one:"<<b.m_ft2DeferredLogPending<<':'<<b.snapshot<<':'<<b.armed<<':'<<b.m_txEnabled<<':'<<b.instance.disabled<<':'<<b.refresh<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-deferredseq-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1],out
lines=out[0].splitlines();assert len(lines)==11 and all(x.startswith('0:0:') for x in lines[:4]) and lines[4]=='1:0:2:0:1:0:0:1' and lines[5]=='1:0:5:1:0:1:1:1' and lines[-1]=='one:1:1:1:0:1:1',lines
print('deferred AutoSeq A/B: stale state, FT2 slot guard, TX5 and legacy one-shot passed')

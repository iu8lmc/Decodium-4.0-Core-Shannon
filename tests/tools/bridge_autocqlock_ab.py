#!/usr/bin/env python3
"""A/B AutoCQ partner lock and caller queue with controlled dependencies."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/autocqlock_before.inc').read_text();new='void DecodiumBridge::updateAutoCqPartnerLock('+(r/'src/bridge/DecodiumBridgeAutoCqLockQueue.cpp').read_text().split('void DecodiumBridge::updateAutoCqPartnerLock(',1)[1];new=new.replace('decodiumBridgeMamQueueLog(', 'bridgeLog(');assert old==new
src=r'''
#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QStringList>
#include <iostream>
namespace Radio{QString base_callsign(const QString&s){return s.section('/',0,0);}}void bridgeLog(const QString&){}
struct DecodiumBridge{static constexpr int FOX_QUEUE_MAX=20;bool m_autoCqRepeat=true,m_manualTxHold=false,m_multiAnswerMode=false,duplicate=false,allow=true,m_callerQueueManualOrder=true,enabled=false;int m_qsoProgress=3,m_currentTx=3,m_autoCqLockedNtx=0,m_autoCqLockedProgress=0,m_txRetryCount=5,m_txWatchdogTicks=5,changed=0,ranked=0;QString m_dxCall="K1ABC/P",m_dxGrid="FN20",m_autoCqLockedCall,m_autoCqLockedGrid,m_mode="FT8";double m_frequency=14074000;QStringList m_callerQueue;QHash<QString,qint64> m_callerQueueArrivalMs;void clearAutoCqPartnerLock(){m_autoCqLockedCall.clear();}bool isRecentAutoCqDuplicate(const QString&,double,const QString&)const{return duplicate;}void setDxCall(const QString&s){m_dxCall=s;}void setDxGrid(const QString&s){m_dxGrid=s;}bool selectCurrentTxIfAllowed(int n,const QString&){if(!allow)return false;m_currentTx=n;return true;}void qsoProgressChanged(){}void regenerateTxMessages(){}void setTxEnabled(bool x){enabled=x;}QString activeMamCallerBase()const{return Radio::base_callsign(m_dxCall);}void callerQueueChanged(){changed++;}void rankCallerQueue(const QString&){ranked++;}void updateAutoCqPartnerLock();void restoreAutoCqPartnerLock();bool enqueueCallerInternal(const QString&,int,int,bool);};
#include "methods.inc"
int main(){for(int k=0;k<5;k++){DecodiumBridge b;b.updateAutoCqPartnerLock();b.m_dxCall.clear();b.m_currentTx=6;b.m_qsoProgress=0;if(k==1)b.m_manualTxHold=true;if(k==2)b.duplicate=true;if(k==3)b.allow=false;if(k==4)b.m_autoCqRepeat=false;b.restoreAutoCqPartnerLock();std::cout<<b.m_dxCall.toStdString()<<':'<<b.m_currentTx<<':'<<b.m_qsoProgress<<':'<<b.enabled<<':'<<b.m_autoCqLockedCall.isEmpty()<<'\n';}for(int k=0;k<6;k++){DecodiumBridge b;b.m_dxCall="K1ABC";if(k==1)b.duplicate=true;if(k==2)b.m_callerQueue<<"W1AW/P 1000 -9";if(k==3)for(int i=0;i<20;i++)b.m_callerQueue<<QString("N%1ABC").arg(i);QString c=k==4?"K1ABC":k==5?"":"W1AW";bool ok=b.enqueueCallerInternal(c,1200,-10,true);std::cout<<ok<<':'<<b.m_callerQueue.size()<<':'<<b.changed<<':'<<b.ranked<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-autocqlock-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'methods.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1],out
lines=out[0].splitlines();assert len(lines)==11 and lines[0]=='K1ABC/P:3:3:1:0' and lines[2].endswith(':1') and lines[5]=='1:1:1:1' and all(x.startswith('0:') for x in lines[6:]),lines
print('AutoCQ lock/queue A/B: restoration and active/recent/duplicate/full guards passed')

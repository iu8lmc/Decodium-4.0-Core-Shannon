#!/usr/bin/env python3
"""Compare scheduler source and test manual dispatch decisions at slot boundaries."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/syncschedule_before.inc').read_text();new='bool DecodiumBridge::usesDeferredManualSyncTx() const'+(r/'src/bridge/DecodiumBridgeSyncSchedule.cpp').read_text().split('bool DecodiumBridge::usesDeferredManualSyncTx() const',1)[1]
for a,b in [('decodiumBridgeCatSuppressedForTx(', 'catSuppressedByEnvironment('),('decodiumBridgeTxPeriodIsEven(', 'bridgeTxPeriodIsEven('),('decodiumBridgeLatestD3SyncStartMs(', 'latestD3CompatibleSyncTxStartMs('),('decodiumBridgeSyncScheduleLog(', 'bridgeLog(')]:new=new.replace(a,b)
if old!=new.replace('QTimer::singleShot(delayMs, Qt::PreciseTimer, this,', 'QTimer::singleShot(delayMs, this,'):raise SystemExit('sync scheduler differs beyond precise timer fix')
program=r'''
#include <QString>
#include <iostream>
static bool cat=false;bool catSuppressedByEnvironment(){return cat;} bool bridgeTxPeriodIsEven(int p){return p==0;} int latestD3CompatibleSyncTxStartMs(const QString&,int p,bool){return p-100;}
void bridgeLog(const QString&){}
class DecodiumBridge { public:
 QString m_mode="FT8";bool m_asyncTxEnabled=false,m_monitoring=true,m_transmitting=false,m_tuning=false,m_deferredManualSyncTx=true,m_ftxImmediateClickTx=false,legacy=false,gate=false,guard=true;int m_txPeriod=0,m_currentTx=1;long long now=0;QString cleared;int scheduled=0,starts=0,periodic=0;
 bool usingLegacyBackendForTx()const{return legacy;} int effectivePeriodMsForMode(const QString&)const{return 1000;} long long correctedUtcMsecsSinceStartOfDay()const{return now;} bool usesFt8AutoTxDecodeGate()const{return gate;}void clearDeferredManualSyncTx(const QString& x){cleared=x;m_deferredManualSyncTx=false;}void checkAndStartPeriodicTx(){periodic++;}void scheduleSyncTxAtNextValidSlot(const QString&,int,int){scheduled++;}bool txDispatchGuardAllows(const QString&){return guard;}void startTx(){starts++;m_transmitting=true;}
 bool usesDeferredManualSyncTx()const;bool shouldDeferManualSyncTxStart()const;bool tryStartDeferredManualSyncTx();};
#include "methods.inc"
int main(){for(QString mode:{QString("FT8"),QString("FT4"),QString("FT2"),QString("JT65"),QString("FST4"),QString("MSK144"),QString("CW")})for(bool legacy:{false,true})for(bool suppression:{false,true})for(int t:{0,899,900,1000,1999}){DecodiumBridge b;b.m_mode=mode;b.legacy=legacy;cat=suppression;b.now=t;bool uses=b.usesDeferredManualSyncTx(),defer=b.shouldDeferManualSyncTxStart(),started=b.tryStartDeferredManualSyncTx();std::cout<<uses<<defer<<started<<':'<<b.scheduled<<':'<<b.starts<<':'<<b.cleared.toStdString()<<'\n';}for(int k=0;k<4;k++){DecodiumBridge b;b.now=0;b.gate=k==1;b.guard=k!=2;b.m_monitoring=k!=3;bool x=b.tryStartDeferredManualSyncTx();std::cout<<x<<':'<<b.starts<<':'<<b.periodic<<':'<<b.cleared.toStdString()<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outs=[]
with tempfile.TemporaryDirectory(prefix='decodium-sync-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(program)
 for body in (old,new):
  (p/'methods.inc').write_text(body.split('void DecodiumBridge::clearDeferredManualSyncTx(',1)[0]);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outs.append(subprocess.check_output([str(p/'test')],text=True))
assert outs[0]==outs[1] and len(outs[0].splitlines())==144
assert 'TX guard' in outs[0] and 'auto-ft8-rx-rendezvous' in outs[0] and 'unsupported-mode' in outs[0]
print('sync scheduler A/B identical: 140 mode/backend/slot cases plus guard and rendezvous')

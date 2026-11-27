#!/usr/bin/env python3
"""Exercise real Qt timer retry/cancellation in original and moved scheduler."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/syncschedule_before.inc').read_text();new='bool DecodiumBridge::usesDeferredManualSyncTx() const'+(r/'src/bridge/DecodiumBridgeSyncSchedule.cpp').read_text().split('bool DecodiumBridge::usesDeferredManualSyncTx() const',1)[1]
for a,b in [('decodiumBridgeCatSuppressedForTx(', 'catSuppressedByEnvironment('),('decodiumBridgeTxPeriodIsEven(', 'bridgeTxPeriodIsEven('),('decodiumBridgeLatestD3SyncStartMs(', 'latestD3CompatibleSyncTxStartMs('),('decodiumBridgeSyncScheduleLog(', 'bridgeLog(')]:new=new.replace(a,b)
assert old==new.replace('QTimer::singleShot(delayMs, Qt::PreciseTimer, this,', 'QTimer::singleShot(delayMs, this,')
src=r'''
#include <QCoreApplication>
#include <QTimer>
#include <QEventLoop>
#include <iostream>
bool catSuppressedByEnvironment(){return true;} bool bridgeTxPeriodIsEven(int p){return p==0;} int latestD3CompatibleSyncTxStartMs(const QString&,int p,bool){return p-10;} void bridgeLog(const QString&){}
class DecodiumBridge: public QObject { public:
 QString m_mode="FT2";bool m_asyncTxEnabled=false,m_monitoring=true,m_transmitting=false,m_tuning=false,m_deferredManualSyncTx=true,m_ftxImmediateClickTx=false,m_manualTxHold=false,m_syncTxRetryScheduled=false,m_txEnabled=false,m_autoCqRepeat=false;int m_txPeriod=0,m_currentTx=1;quint64 m_syncTxRetrySerial=0,m_txRequestGeneration=0,m_periodTimerSessionId=0;QTimer* m_periodTimer=nullptr;int starts=0,periodic=0,precompute=-1,armed=0;
 bool usingLegacyBackendForTx()const{return false;}int effectivePeriodMsForMode(const QString&)const{return 100;}qint64 correctedUtcMsecsSinceStartOfDay()const{return 0;}bool usesFt8AutoTxDecodeGate()const{return false;}bool txDispatchGuardAllows(const QString&){return true;}void startTx(){starts++;}void checkAndStartPeriodicTx(){periodic++;}void scheduleTxAudioPrecompute(qint64 d){precompute=d;}void armPeriodTimerForCurrentMode(quint64,const QString&){armed++;}
 bool usesDeferredManualSyncTx()const;bool shouldDeferManualSyncTxStart()const;bool tryStartDeferredManualSyncTx();void clearDeferredManualSyncTx(const QString&);void scheduleSyncTxAtNextValidSlot(const QString&,int,int);void ensureSyncTxSchedulerActive(const QString&);};
#include "methods.inc"
void wait(){QEventLoop loop;QTimer::singleShot(90,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);
 for(int scenario=0;scenario<6;scenario++){DecodiumBridge b;QTimer timer;b.m_periodTimer=&timer;b.scheduleSyncTxAtNextValidSlot("test",-1,-1);if(scenario==1)b.m_txRequestGeneration++;if(scenario==2)b.m_syncTxRetrySerial++;if(scenario==3)b.m_mode="CW";if(scenario==4)b.m_manualTxHold=true;if(scenario==5)b.m_transmitting=true;wait();std::cout<<b.starts<<':'<<b.m_syncTxRetryScheduled<<':'<<b.precompute<<':'<<b.m_deferredManualSyncTx<<'\n';}
 DecodiumBridge b;QTimer timer;b.m_periodTimer=&timer;b.ensureSyncTxSchedulerActive("test");std::cout<<"armed:"<<b.armed<<':'<<b.m_periodTimerSessionId<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));results=[]
with tempfile.TemporaryDirectory(prefix='decodium-sync-timer-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'methods.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);results.append(subprocess.check_output([str(p/'test')],text=True))
assert results[0]==results[1],results
lines=results[0].splitlines();assert lines[0]=='1:0:40:0' and lines[1]=='0:1:40:1' and lines[2]=='0:1:40:1' and all(line=='0:0:40:1' for line in lines[3:6]) and lines[6]=='armed:1:1',lines
print('sync timer A/B: retry, generation/serial cancellation, state guards and rearming passed')

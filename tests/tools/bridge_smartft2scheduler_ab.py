#!/usr/bin/env python3
"""Compare FT2 asynchronous scheduler guards and timer outcomes."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
old=(r/'tests/bridge_refactor_fixtures/smartft2scheduler_before.inc').read_text()
s=(r/'src/bridge/DecodiumBridgeSmartFt2Scheduler.cpp').read_text();marker='void DecodiumBridge::scheduleSmartFt2AsyncTx('
new=marker+s.split(marker,1)[1];new=new.replace('decodiumBridgeSmartFt2Log(', 'bridgeLog(');assert old==new
source=r'''
#include <QObject>
#include <QCoreApplication>
#include <QDateTime>
#include <QTimer>
#include <QEventLoop>
#include <iostream>
void bridgeLog(const QString&){} int estimatedSyncPayloadMs(const QString&){return 3800;}
class DecodiumBridge:public QObject{public:QString m_mode="FT2",m_callsign="9H1SR";bool m_asyncTxEnabled=true,m_manualTxHold=false,m_monitoring=true,m_transmitting=false,m_tuning=false,m_txEnabled=true,m_ft2AsyncSmartTxPending=false;quint64 m_ft2AsyncSmartTxSerial=0,m_txRequestGeneration=0; qint64 m_ft2AsyncSmartTxDeadlineMs=0,m_ft2AsyncFirstDecodeMs=0,m_ft2AsyncPartnerSlotMs=0,m_ft2AsyncLastDecodeMs=0;int m_ft2AsyncSmartTxRetries=0,m_ft2AsyncAudioQuietRuns=5,started=0,rescheduled=0;bool late=false;qint64 correctedUtcEpochMs()const{return QDateTime::currentMSecsSinceEpoch();}void ft2AsyncGateRequirements(qint64,int&a,int&b,int&c,int&d)const{a=3;b=400;c=50;d=200;}qint64 safeFt2AsyncTxDelay(qint64 v,QString*)const{return v;}bool isFt2AsyncTxStartTooLate(int*a,int*b,int*c){*a=1;*b=2;*c=3;return late;}void scheduleFt2AsyncTxAtNextSafeSlot(const QString&,int,int,int){++rescheduled;}void checkAndStartPeriodicTx(){++started;}void scheduleSmartFt2AsyncTx(const QString&);};
#include "methods.inc"
void wait(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(int k=0;k<7;k++){DecodiumBridge b;b.m_ft2AsyncSmartTxPending=true;if(k==0)b.m_mode="FT8";if(k==1)b.m_asyncTxEnabled=false;if(k==2)b.m_manualTxHold=true;if(k==3)b.m_monitoring=false;if(k==4)b.m_transmitting=true;if(k==5)b.m_tuning=true;if(k==6)b.m_txEnabled=false;b.scheduleSmartFt2AsyncTx("start");std::cout<<"guard:"<<b.m_ft2AsyncSmartTxPending<<":"<<b.m_ft2AsyncSmartTxSerial<<"\n";}for(int k=0;k<5;k++){DecodiumBridge b;b.late=k==1;b.scheduleSmartFt2AsyncTx("start");if(k==2)b.m_txRequestGeneration++;if(k==3)b.m_ft2AsyncSmartTxSerial++;if(k==4)b.m_manualTxHold=true;wait(350);std::cout<<"timer:"<<b.started<<":"<<b.rescheduled<<"\n";}DecodiumBridge b;b.scheduleSmartFt2AsyncTx("start");auto serial=b.m_ft2AsyncSmartTxSerial;b.scheduleSmartFt2AsyncTx("start");std::cout<<"coalesced:"<<(serial==b.m_ft2AsyncSmartTxSerial)<<"\n";}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-smartft2-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1],outputs
lines=outputs[0].splitlines();assert lines==['guard:0:1']*7+['timer:1:0','timer:0:1','timer:0:0','timer:0:0','timer:0:0','coalesced:1'],lines
print('FT2 smart scheduler A/B: guards, timer, late slot, cancellation and coalescing passed')

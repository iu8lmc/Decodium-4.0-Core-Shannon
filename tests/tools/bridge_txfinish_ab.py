#!/usr/bin/env python3
"""Compare playback completion and sequencer decisions using controlled state."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/txfinish_before.inc').read_text();new='bool DecodiumBridge::noteTxPlaybackFinished('+(r/'src/bridge/DecodiumBridgeTxFinish.cpp').read_text().split('bool DecodiumBridge::noteTxPlaybackFinished(',1)[1].split('\nvoid DecodiumBridge::scheduleSyncTxBoundaryStop(',1)[0];new=new.replace('decodiumBridgeTxFinishLog(', 'bridgeLog(');assert old==new
src=r'''
#include <QObject>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <iostream>
void bridgeLog(const QString&){}bool messageCarries73Payload(const QString& s){return s.contains("73");}
class DecodiumBridge:public QObject{public:int m_activeTxNumber=0,m_nTx73=0,m_maxCallerRetries=3,m_lastNtx=0,bursts=0,completed=0,snapshots=0,periodic=0;QString m_activeTxMessage,m_mode="FT2",m_lastTransmittedMessage;bool m_activeTxWasPureAutoCqCq=false,m_autoCqRepeat=false,m_logAfterOwn73=false,m_ft2DeferredLogPending=false,m_periodicTxCheckScheduled=false,m_transmitting=false,m_tuning=false;quint64 m_txRequestGeneration=1;void noteAutoCqBurstCqCompleted(bool,const QString&,bool){bursts++;}bool finishAutoSequenceQsoFromCompletedSignoff(const QString&,bool){completed++;return true;}void capturePendingAutoLogSnapshot(){snapshots++;}void checkAndStartPeriodicTx(){periodic++;}bool noteTxPlaybackFinished(const QString&,bool);};
#include "method.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(int k=0;k<9;k++){DecodiumBridge b;bool error=false;b.m_activeTxNumber=k?4:0;b.m_activeTxMessage="CQ TEST";if(k==2)error=true;if(k==3){b.m_activeTxNumber=5;b.m_activeTxMessage="K1ABC 73";}if(k>=4){b.m_activeTxMessage="K1ABC RR73";b.m_autoCqRepeat=true;b.m_nTx73=k==4?2:3;}if(k==6)b.m_mode="JT65";if(k==7)b.m_transmitting=true;bool result=b.noteTxPlaybackFinished("test",error);if(k==8)b.m_txRequestGeneration++;QEventLoop loop;QTimer::singleShot(5,&loop,&QEventLoop::quit);loop.exec();std::cout<<result<<':'<<b.m_activeTxNumber<<':'<<b.bursts<<':'<<b.m_nTx73<<':'<<b.completed<<':'<<b.snapshots<<':'<<b.periodic<<':'<<b.m_logAfterOwn73<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-txfinish-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for body in (old,new):
  (p/'method.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1],out
lines=out[0].splitlines();assert len(lines)==9 and lines[0]=='0:0:0:0:0:0:0:0' and lines[3]=='1:0:1:1:1:0:0:1' and lines[4]=='0:0:1:3:0:0:1:0' and lines[5]=='1:0:1:4:1:1:0:0',lines
print('TX finish A/B: error, signoff, retry cap, timer and generation guard passed')

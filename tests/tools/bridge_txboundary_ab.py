#!/usr/bin/env python3
"""Compare TX boundary/PCM completion using real timers and in-memory audio."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/txboundary_before.inc').read_text();new='void DecodiumBridge::scheduleSyncTxBoundaryStop('+(r/'src/bridge/DecodiumBridgeTxFinish.cpp').read_text().split('void DecodiumBridge::scheduleSyncTxBoundaryStop(',1)[1];new=new.replace('decodiumBridgeTxFinishLog(', 'bridgeLog(');assert old==new
src=r'''
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QBuffer>
#include <QDateTime>
#include <iostream>
void bridgeLog(const QString&){}
class DecodiumBridge:public QObject{public:bool align=true,m_transmitting=true,m_tuning=false,m_txPlaybackReleasePending=false,m_ft2LinkTxActive=false,m_bridgeAudioLegacyTxActive=false;int period=100,complete=0,idle=0;quint64 m_txPlaybackSerial=1;qint64 m_txPlaybackHoldUntilMs=50,m_txPlaybackHardDeadlineMs=0;QString m_mode="FT2",last;QBuffer* m_txPcmBuffer=nullptr;bool shouldAlignTxAudioToCurrentSyncSlot(){return align;}int effectivePeriodMsForMode(const QString&){return period;}qint64 correctedUtcEpochMs(){return 50;}void completeTxPlayback(const QString& s,bool=false){complete++;last=s;}void finishModulatorIdlePlayback(const QString& s){idle++;last=s;}void scheduleSyncTxBoundaryStop(const QString&,quint64);bool delayTxFinishUntilPcmConsumed(const QString&,const QString&,bool);};
#include "methods.inc"
void wait(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(int k=0;k<6;k++){DecodiumBridge b;QBuffer p;p.setData(QByteArray(4,'x'));p.open(QIODevice::ReadOnly);b.m_txPcmBuffer=&p;b.scheduleSyncTxBoundaryStop("test",1);if(k==1)b.m_tuning=true;if(k==2)b.m_txPlaybackSerial=2;if(k==3)b.m_mode="FT4";if(k==4)b.align=false;if(k==5)b.m_transmitting=false;wait(120);std::cout<<"slot:"<<b.complete<<':'<<p.pos()<<':'<<b.m_txPlaybackHoldUntilMs<<'\n';}for(int k=0;k<5;k++){DecodiumBridge b;QBuffer p;p.setData(QByteArray(4,'x'));p.open(QIODevice::ReadOnly);b.m_txPcmBuffer=&p;if(k==2)b.m_txPlaybackHardDeadlineMs=QDateTime::currentMSecsSinceEpoch()-1;if(k==3)p.seek(p.size());if(k==4)b.m_transmitting=false;bool delayed=b.delayTxFinishUntilPcmConsumed("test","stop",k==0);if(k<2)b.delayTxFinishUntilPcmConsumed("test","stop",k==0);wait(300);std::cout<<"pcm:"<<delayed<<':'<<b.complete<<':'<<b.idle<<':'<<b.m_txPlaybackReleasePending<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-txboundary-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1],out
lines=out[0].splitlines();assert lines==['slot:1:4:0']+['slot:0:0:50']*5+['pcm:1:1:0:0','pcm:1:0:1:0']+['pcm:0:0:0:0']*3,lines
print('TX boundary A/B: slot stop, timer guards, buffer drain and deadlines passed')

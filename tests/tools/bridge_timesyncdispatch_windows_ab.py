#!/usr/bin/env python3
"""Compare native dispatch windows against the saved pre-extraction method."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
mark='void DecodiumBridge::dispatchTimeSyncDecodeWhenReady('
before=(r/'tests/bridge_refactor_fixtures/timesyncdispatch_before.inc').read_text()
after=(r/'src/bridge/DecodiumBridgeTimeSyncDispatch.cpp').read_text();after=mark+after.split(mark,1)[1]
assert before==after
harness=r'''#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <QEventLoop>
#include <QVector>
#include <QString>
#include <iostream>
#define DECODIUM_HAS_SSTV 1
void bridgeLog(const QString&){}
bool bridgeOwnsLegacyJtRxWhenCatSuppressed(const QString&){return false;}
bool bridgeOwnsFst4Audio(const QString&){return false;}
class DecodiumBridge:public QObject {public:
 static constexpr int SAMPLE_RATE=12000,TIMER_MS=100;bool m_sstvWorkspaceActive=false,m_monitoring=true,m_rxAudioSuspendedForTx=false,m_transmitting=false,m_tuning=false,legacy=false,m_pendingTimeSyncDecodeActive=false,m_forcedDecodeAudioSnapshotActive=false;
 quint64 m_periodTimerSessionId=1;QString m_mode="FT8",m_pendingTimeSyncDecodeMode; qint64 m_pendingTimeSyncDecodeSlot=-1,now=15001;int m_periodTicksMax=150,dispatches=0;QMutex m_audioBufferMutex;QVector<short>m_audioBuffer,m_pendingTimeSyncDecodeAudio,m_forcedDecodeAudioSnapshot,last;
 bool isTimeSyncDecodeMode(const QString&mode){return mode=="FT8";}bool usingLegacyBackendForRx(){return legacy;}int targetDecodeSamplesForMode(const QString&){return 10;}int minimumDecodeSamplesForMode(const QString&){return 5;}int periodMsForMode(const QString&){return 15000;}qint64 correctedUtcEpochMs(){return now;}
 void feedAudioToDecoder(qint64){++dispatches;last=m_forcedDecodeAudioSnapshot;m_forcedDecodeAudioSnapshotActive=false;}
 void dispatchTimeSyncDecodeWhenReady(qint64,const QString&,quint64,qint64);
};
#include "under_test.inc"
void require(bool ok,const char*message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main(int argc,char**argv){QCoreApplication app(argc,argv);
 {DecodiumBridge b;b.m_audioBuffer={0,1,2,3,4,5,6,7,8,9,10,11};b.dispatchTimeSyncDecodeWhenReady(0,"FT8",1,0);require(b.dispatches==1&&b.last==QVector<short>({0,1,2,3,4,5,6,7,8,9})&&b.m_audioBuffer==QVector<short>({10,11}),"alignment");std::cout<<"aligned:10 retained:2\n";}
 {DecodiumBridge b;b.m_audioBuffer={1,2,3,4,5,6};b.dispatchTimeSyncDecodeWhenReady(0,"FT8",1,16000);require(b.dispatches==0&&b.m_pendingTimeSyncDecodeActive,"waiting");b.m_audioBuffer={7,8,9,10};QEventLoop loop;QTimer::singleShot(180,&loop,&QEventLoop::quit);loop.exec();require(b.dispatches==1&&b.last==QVector<short>({1,2,3,4,5,6,7,8,9,10})&&b.m_audioBuffer.isEmpty(),"deferred borrow");std::cout<<"deferred:10 consumed:4\n";}
 {DecodiumBridge b;b.m_pendingTimeSyncDecodeActive=true;b.m_pendingTimeSyncDecodeSlot=0;b.m_pendingTimeSyncDecodeMode="FT8";b.m_pendingTimeSyncDecodeAudio={1,2,3};b.m_audioBuffer={4,5};b.dispatchTimeSyncDecodeWhenReady(0,"FT8",1,0);require(b.dispatches==1&&b.last==QVector<short>({1,2,3,4,5})&&b.m_audioBuffer==QVector<short>({4,5}),"partial peek");std::cout<<"timeout:5 retained:2\n";}
 {DecodiumBridge b;b.m_pendingTimeSyncDecodeActive=true;b.m_pendingTimeSyncDecodeAudio={1,2};b.m_transmitting=true;b.dispatchTimeSyncDecodeWhenReady(0,"FT8",1,0);require(!b.m_pendingTimeSyncDecodeActive&&b.m_pendingTimeSyncDecodeAudio.isEmpty()&&b.dispatches==0,"tx guard");std::cout<<"tx-guard:cleared\n";}
}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True)); results=[]
with tempfile.TemporaryDirectory(prefix='decodium-timesync-windows-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text(harness)
 for label,body in [('before',before),('after',after)]:
  (d/'under_test.inc').write_text(body);subprocess.run([os.environ.get('CXX','c++'),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test')],check=True)
  result=subprocess.run([str(d/'test')],check=True,text=True,capture_output=True);results.append(result.stdout);print(label,result.stdout,end='')
assert results[0]==results[1]

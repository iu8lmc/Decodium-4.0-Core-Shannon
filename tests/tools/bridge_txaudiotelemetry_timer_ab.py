#!/usr/bin/env python3
"""Compare actual periodic telemetry callbacks using a simulated audio sink."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/txaudiotelemetry_before.inc').read_text();new='void DecodiumBridge::resetTxAudioTelemetry('+(r/'src/bridge/DecodiumBridgeTxAudioTelemetry.cpp').read_text().split('void DecodiumBridge::resetTxAudioTelemetry(',1)[1];new=new.replace('decodiumBridgeTxAudioTelemetryLog(', 'bridgeLog(');assert old==new
src=r'''
#include <QCoreApplication>
#include <QEventLoop>
#include <QAudio>
#include <QBuffer>
#include <QDateTime>
#include <QPointer>
#include <QTimer>
#include <iostream>
int logs=0;void bridgeLog(const QString&){logs++;}
class QAudioSink:public QObject { public: qint64 processedUSecs(){return 0;}QAudio::State state(){return QAudio::ActiveState;}QAudio::Error error(){return QAudio::UnderrunError;}qint64 bytesFree(){return 100;}qsizetype bufferSize(){return 500;}};
class DecodiumBridge:public QObject {public:quint64 m_txAudioTelemetrySerial=0,m_txPlaybackSerial=0;qint64 m_txAudioTelemetryStartWallMs=0,m_txAudioTelemetryLastWallMs=0,m_txAudioTelemetryLastProcessedUs=0,m_txAudioTelemetryExpectedUs=0,m_txAudioTelemetryMaxLagMs=0,m_txAudioTelemetryMaxStallMs=0,m_txAudioTelemetryLastWarningMs=0,m_txAudioTelemetryPcmBytes=0;int m_txAudioTelemetrySlotElapsedMs=0,m_txAudioTelemetryLeadInMs=0,m_txAudioTelemetryPayloadDelayMs=0,m_txAudioTelemetryStallCount=0,m_txAudioTelemetryErrorCount=0,m_txAudioTelemetryIdleCount=0,m_txAudioTelemetryUnderrunCount=0,m_activeTxNumber=6;qsizetype m_txAudioTelemetrySinkBufferSize=0;double m_txAudioTelemetryWavePeak=0,m_txAudioTelemetryGain=0;bool m_transmitting=true,m_tuning=false,m_lowCpuModeEnabled=false;QString m_mode="FT2",m_activeTxMessage="CQ TEST";QAudioSink* m_txAudioSink=nullptr;QBuffer* m_txPcmBuffer=nullptr;bool cpuPressureActive(){return false;}void resetTxAudioTelemetry(quint64,qint64,qint64,int,int,int,qint64,qsizetype,double,double);void scheduleTxAudioTelemetryProbe(quint64);void logTxAudioTelemetrySummary(const QString&);};
#include "methods.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(int scenario=0;scenario<4;scenario++){DecodiumBridge b;QAudioSink sink;QBuffer buffer;buffer.setData(QByteArray(100,'x'));buffer.open(QIODevice::ReadOnly);b.m_txAudioSink=&sink;b.m_txPcmBuffer=&buffer;b.m_txPlaybackSerial=4;b.resetTxAudioTelemetry(4,QDateTime::currentMSecsSinceEpoch()-700,5000000,1,2,3,100,500,.5,.7);b.m_txAudioTelemetryLastWallMs=QDateTime::currentMSecsSinceEpoch()-250;if(scenario==1)b.m_tuning=true;if(scenario==2)b.m_txPlaybackSerial=5;if(scenario==3)b.m_transmitting=false;logs=0;b.scheduleTxAudioTelemetryProbe(4);QEventLoop loop;QTimer::singleShot(300,&loop,&QEventLoop::quit);loop.exec();std::cout<<b.m_txAudioTelemetryStallCount<<':'<<b.m_txAudioTelemetryErrorCount<<':'<<b.m_txAudioTelemetryUnderrunCount<<':'<<logs<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Multimedia'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-txaudio-probe-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-Wno-deprecated-declarations','-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1] and out[0].splitlines()==['1:1:1:1']+['0:0:0:0']*3,out
print('telemetry timer A/B: stall/underrun warning and tuning, serial, TX guards passed')

#!/usr/bin/env python3
"""Compare audio telemetry source and exercise reset/summary against a fake sink."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/txaudiotelemetry_before.inc').read_text();new='void DecodiumBridge::resetTxAudioTelemetry('+(r/'src/bridge/DecodiumBridgeTxAudioTelemetry.cpp').read_text().split('void DecodiumBridge::resetTxAudioTelemetry(',1)[1];new=new.replace('decodiumBridgeTxAudioTelemetryLog(', 'bridgeLog(');assert old==new

def methods(s):return s[:s.index('void DecodiumBridge::scheduleTxAudioTelemetryProbe(')]+s[s.index('void DecodiumBridge::logTxAudioTelemetrySummary('):]
src=r'''
#include <QBuffer>
#include <QString>
#include <iostream>
QString logLine;void bridgeLog(const QString& s){logLine=s;}
struct Sink{qint64 processedUSecs(){return 500000;}int state(){return 1;}int error(){return 0;}qint64 bytesFree(){return 100;}qsizetype bufferSize(){return 500;}};
struct DecodiumBridge {quint64 m_txAudioTelemetrySerial=0;qint64 m_txAudioTelemetryStartWallMs=0,m_txAudioTelemetryLastWallMs=0,m_txAudioTelemetryLastProcessedUs=0,m_txAudioTelemetryExpectedUs=0,m_txAudioTelemetryMaxLagMs=0,m_txAudioTelemetryMaxStallMs=0,m_txAudioTelemetryLastWarningMs=0,m_txAudioTelemetryPcmBytes=0;int m_txAudioTelemetrySlotElapsedMs=0,m_txAudioTelemetryLeadInMs=0,m_txAudioTelemetryPayloadDelayMs=0,m_txAudioTelemetryStallCount=0,m_txAudioTelemetryErrorCount=0,m_txAudioTelemetryIdleCount=0,m_txAudioTelemetryUnderrunCount=0,m_activeTxNumber=6;qsizetype m_txAudioTelemetrySinkBufferSize=0;double m_txAudioTelemetryWavePeak=0,m_txAudioTelemetryGain=0;QString m_mode="FT2",m_activeTxMessage="CQ TEST";Sink* m_txAudioSink=nullptr;QBuffer* m_txPcmBuffer=nullptr;void resetTxAudioTelemetry(quint64,qint64,qint64,int,int,int,qint64,qsizetype,double,double);void logTxAudioTelemetrySummary(const QString&);};
#include "methods.inc"
int main(){for(int k=0;k<2;k++){DecodiumBridge b;Sink sink;QBuffer buffer;buffer.setData(QByteArray("abcd"));buffer.open(QIODevice::ReadOnly);buffer.seek(2);if(k){b.m_txAudioSink=&sink;b.m_txPcmBuffer=&buffer;}b.m_txAudioTelemetryStallCount=9;b.resetTxAudioTelemetry(8,100,900000,20,30,40,400,800,.5,.7);b.logTxAudioTelemetrySummary("test");std::cout<<logLine.toStdString()<<'\n'<<b.m_txAudioTelemetrySerial<<':'<<b.m_txAudioTelemetryStallCount<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-txaudiotelemetry-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for body in (old,new):
  (p/'methods.inc').write_text(methods(body));subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1] and out[0].count('0:0')==2 and 'pcm=2/4' in out[0],out
print('TX audio telemetry A/B: identical source, reset and summary with and without sink')

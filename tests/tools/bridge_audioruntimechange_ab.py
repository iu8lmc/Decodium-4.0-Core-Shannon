#!/usr/bin/env python3
"""A/B audio runtime rebind policies with controlled collaborators."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/audioruntimechange_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeAudioRuntimeChange.cpp').read_text();mark='void DecodiumBridge::applyAudioInputRuntimeChange(';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <atomic>
#include <iostream>
#define emit
#define DECODIUM_HAS_SSTV 1
QStringList events; void bridgeLog(const QString&s){events<<"log:"+s;}
struct Recovery{void cancel(const QString&s){events<<"cancel:"+s;}};struct Clock{void restart(){events<<"clock";}};struct Out{void setAttenuation(double v){events<<QString("atten:%1").arg(v);}};struct Sink{void setVolume(float v){events<<QString("volume:%1").arg(v);}};
bool fallback=false;bool useLegacyRigControlFallback(void*,const QString&){return fallback;}double txAttenuationFromSlider(double){return 3;}double txGainFromSlider(double){return .5;}
class DecodiumBridge{public:
 Recovery recovery,*m_rttyRxRecovery=&recovery;bool legacy=false,modern=false,sstv=false,m_monitoring=false,m_transmitting=false,m_tuning=false,m_tciAudioCaptureActive=false,tci=false;void*m_legacyBackend=nullptr,*m_soundInput=nullptr;QString m_catBackend,m_mode="FT2",m_audioInputDevice="Input";int m_periodTimerSessionId=1,m_wfRingPos=8,m_driftFrameCount=2,m_driftExpectedFrames=3;long long m_audioWatchdogIgnoreUntilMs=0,m_audioUnhealthyStartMs=1,m_lastPanadapterFrameMs=0,m_lastSpectrumRecoveryMs=0;QString m_spectrumBuf="x",m_lastPanadapterData="x";Clock m_driftClock;bool m_asyncDecodePending=true,m_cachedTxOutputDeviceValid=true;std::atomic<int>m_asyncAudioPos{4};Out output,*m_soundOutput=&output;Sink sink,*m_txAudioSink=&sink;double m_txOutputLevel=50;
 bool nativeSstvRxForcesDedicatedAudioCapture(){return sstv;}bool usingLegacyBackendForRx(){return legacy;}bool useModernSpectrumFeedWithLegacy(){return modern;}bool usingTciAudioInput(){return tci;}
 void scheduleMonitorRecovery(const QString&,int,bool){events<<"defer";}void stopAudioCapture(){events<<"stop";}void startAudioCapture(){events<<"start";}void resetRxPeriodAccumulation(bool){events<<"reset-period";}void resetTimeSyncDecodeMetrics(){events<<"reset-metrics";}void statusMessage(const QString&s){events<<"status:"+s;}void invalidateTxAudioCache(){events<<"invalidate";}void scheduleTxAudioPrecompute(){events<<"precompute";}
 void applyAudioInputRuntimeChange(const QString&);void applyAudioOutputRuntimeChange(const QString&);};
#include "methods.inc"
void show(const char*name,DecodiumBridge&b){std::cout<<name<<'|'<<events.join(';').toStdString()<<'|'<<b.m_cachedTxOutputDeviceValid<<'|'<<b.m_asyncAudioPos<<'|'<<b.m_spectrumBuf.isEmpty()<<'\n';events.clear();}
int main(){DecodiumBridge b;b.legacy=true;b.applyAudioInputRuntimeChange("x");show("legacy",b);b.legacy=false;b.applyAudioInputRuntimeChange("x");show("stopped",b);b.m_monitoring=true;b.m_transmitting=true;b.applyAudioInputRuntimeChange("x");show("tx",b);b.m_transmitting=false;fallback=true;b.applyAudioInputRuntimeChange("x");show("fallback",b);fallback=false;b.legacy=true;b.sstv=true;b.applyAudioInputRuntimeChange("x");show("sstv",b);b.legacy=false;b.sstv=false;b.applyAudioInputRuntimeChange("x");show("active",b);b.m_transmitting=true;b.applyAudioOutputRuntimeChange("x");show("out-tx",b);b.m_transmitting=false;b.applyAudioOutputRuntimeChange("x");show("out-idle",b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-audioruntime-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==8
assert 'handled by legacy' in lines[0] and ';stop;' not in lines[0] and ';stop;' not in lines[1]
assert 'defer' in lines[2] and 'fallback owns RX audio' in lines[3]
assert all('stop;reset-period;reset-metrics;clock;start;status:' in lines[i] and lines[i].endswith('|1|0|1') for i in (4,5))
assert 'invalidate;atten:3;volume:0.5' in lines[6] and 'precompute' not in lines[6]
assert 'invalidate;atten:3;volume:0.5;precompute' in lines[7]
print('Audio runtime A/B: legacy ownership, monitoring, TX deferral, fallback, RX restart and output guards passed')

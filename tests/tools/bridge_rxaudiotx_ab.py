#!/usr/bin/env python3
"""Compare RX audio pause/resume across simulated Windows and non-Windows paths."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/rxaudiotx_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeRxAudioTx.cpp').read_text();mark='void DecodiumBridge::pauseRxAudioForTx(';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMutex>
#include <QMutexLocker>
#include <functional>
#include <vector>
#include <iostream>
#ifdef TEST_WINDOWS
#define Q_OS_WIN 1
#endif
struct FakeDateTime{static qint64 currentMSecsSinceEpoch(){return 1000;}};
#define QDateTime FakeDateTime
std::vector<std::function<void()>> callbacks;struct FakeTimer{template<class T,class F>static void singleShot(int,T*,F f){callbacks.push_back(f);}};
#define QTimer FakeTimer
QStringList events;void bridgeLog(const QString&s){events<<"log:"+s;}
struct Input{void suspend(){events<<"suspend";}void resume(){events<<"resume";}};struct Sink{void setDiscardSamples(bool b){events<<QString("discard:%1").arg(b);}};
class DecodiumBridge{public:static constexpr int TIMER_MS=250,SAMPLE_RATE=12000;Input input,*m_soundInput=&input;Sink sink,*m_audioSink=&sink;QMutex m_audioBufferMutex;QVector<short>m_audioBuffer={1,2};int m_periodTicksMax=2;bool m_rxAudioKeptOpenForTx=false,m_rxAudioSuspendedForTx=false,m_monitoring=false,m_monitorRequested=false,m_tciAudioCaptureActive=false,m_transmitting=false,m_tuning=false,legacy=false,modern=false,dedicated=false,sstv=false,tci=false;long long m_lastTxEndMs=0,m_audioUnhealthyStartMs=0,m_audioWatchdogIgnoreUntilMs=0,m_lastLegacyPcmSampleMs=0,m_lastAudioHealthMs=0;quint64 m_postTxRxResumeSerial=0;
 bool usingLegacyBackendForRx(){return legacy;}bool useModernSpectrumFeedWithLegacy(){return modern;}bool useDedicatedModernAudioCaptureWithLegacy(){return dedicated;}bool nativeSstvRxForcesDedicatedAudioCapture(){return sstv;}bool usingTciAudioInput(){return tci;}
 void restartAudioCaptureFromWatchdog(const QString&){events<<"restart";}void rearmLegacyPcmSpectrumFeed(const QString&){events<<"rearm";}void scheduleLegacyPcmSpectrumRearm(const QString&){events<<"schedule-rearm";}void pauseRxAudioForTx(const QString&);void resumeRxAudioAfterTx(const QString&);};
#include "methods.inc"
void show(const char*n,DecodiumBridge&b){std::cout<<n<<':'<<b.m_rxAudioSuspendedForTx<<':'<<b.m_rxAudioKeptOpenForTx<<':'<<b.m_audioBuffer.size()<<':'<<callbacks.size()<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge b;b.pauseRxAudioForTx("test");show("pause",b);b.resumeRxAudioAfterTx("off");show("off",b);b.m_monitoring=true;b.m_audioBuffer={1,2};b.pauseRxAudioForTx("test");b.resumeRxAudioAfterTx("on");show("normal",b);callbacks.clear();b.legacy=true;b.modern=false;b.m_rxAudioSuspendedForTx=true;b.resumeRxAudioAfterTx("legacy");show("legacy",b);b.legacy=false;b.tci=true;b.m_tciAudioCaptureActive=false;b.resumeRxAudioAfterTx("tci");show("tci",b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-rxaudiotx-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source)
 for variant in ('native','windows'):
  out=[]
  for body in (old,new):
   (p/'methods.inc').write_text(body);defs=['-DTEST_WINDOWS'] if variant=='windows' else [];subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',*defs,str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
  assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==5
  assert ('discard:1' if variant=='windows' else 'suspend') in lines[0]
  assert ('discard:0' if variant=='windows' else 'monitor off') in lines[1]
  assert ('discard:0' if variant=='windows' else 'resume') in lines[2]
  assert 'rearm;schedule-rearm' in lines[3] and 'restart' in lines[4]
print('RX audio A/B: native/Windows pause, monitor-off, resume, legacy tap and TCI recovery passed')

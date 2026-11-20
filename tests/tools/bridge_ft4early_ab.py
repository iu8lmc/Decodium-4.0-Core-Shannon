#!/usr/bin/env python3
"""Compare FT4 early-decode scheduling and request construction."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/ft4early_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeFt4Early.cpp').read_text();mark='void DecodiumBridge::queueFt4DecodeRequest(';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QByteArray>
#include <iostream>
struct FakeDateTime{static qint64 currentMSecsSinceEpoch(){return 100000;}static FakeDateTime currentDateTimeUtc(){return {};}QString toString(const QString&)const{return "123456";}};
#define QDateTime FakeDateTime
struct FakeMeta{template<class W,class F>static void invokeMethod(W*,F f,Qt::ConnectionType){f();}};
#define QMetaObject FakeMeta
void bridgeLog(const QString&){}int utcTokenForSlotStart(qint64,int){return 123000;}QString utcDisplayTokenForSlotStart(qint64,int){return "123000";}
namespace decodium::ft4{struct DecodeRequest{quint64 serial=0;QVector<short>audio;int nutc=0,nqsoprogress=0,nfqso=0,nfa=0,nfb=0,ndepth=0,threadCount=0,ncontest=0,lapcqonly=0;QByteArray mycall,hiscall;};struct FT4DecodeWorker{int count=0;DecodeRequest last;void markLatestDecodeSerial(quint64){}void decode(const DecodeRequest&r){last=r;count++;}};}
class DecodiumBridge{public:decodium::ft4::FT4DecodeWorker worker,*m_ft4Worker=&worker;int m_nfa=200,m_nfb=3000,m_rxFrequency=4000,m_ncontest=0;bool m_transmitting=false,m_tuning=false,m_monitoring=true,m_rxAudioSuspendedForTx=false,m_lowCpuModeEnabled=false,legacy=false,latency=false,adaptive=false,backlog=false,pressure=false;QString m_mode="FT4",m_callsign="ME",m_dxCall="DX";quint64 m_deepDecodeVisualSerial=0,m_decodeSerial=0,m_decodeSessionId=2;qint64 m_deepDecodeVisualThrottleUntilMs=0,m_ft4EarlyDecodeSlot=-1,m_lastEarlyDecodeSkipLogMs=0;bool m_ft4EarlyDecodeSent=false;QMutex m_audioBufferMutex;QVector<short>m_audioBuffer;QHash<quint64,qint64>m_decodeStartMsBySerial,m_decodeSessionBySerial;QHash<quint64,QString>m_decodeModeBySerial,m_decodeUtcTokenBySerial;int activated=0;
 int effectiveFt4DecodeDepth(int d){return d;}int effectiveFt4ThreadLimit(){return 2;}bool ft4AdaptiveCpuLimitActive(qint64=0){return adaptive;}bool usingLegacyBackendForTx(){return legacy;}bool ft4LatencyGuardActive(qint64){return latency;}bool ft4LiveDecodeBacklogActive(qint64,int,int,QString*d){if(backlog)*d="busy";return backlog;}void activateFt4AdaptiveCpuLimit(qint64,const QString&,qint64){activated++;}bool cpuPressureActive(){return pressure;}int minimumDecodeSamplesForMode(const QString&){return 4;}int effectiveDecodeDepth(){return 3;}int legacyDecodeQsoProgress(){return 1;}int legacyDecodeCqHint(){return 0;}void queueFt4DecodeRequest(const QVector<short>&,quint64,int,qint64,int,int,int);void maybeDispatchFt4EarlyDecode(qint64,int,int);};
#include "methods.inc"
void show(const char*n,DecodiumBridge&b){std::cout<<n<<':'<<b.worker.count<<':'<<b.m_ft4EarlyDecodeSent<<':'<<b.m_decodeSerial<<':'<<b.activated<<':'<<b.worker.last.ndepth<<':'<<b.worker.last.nfqso<<':'<<b.worker.last.audio.size()<<'\n';}
int main(){DecodiumBridge b;b.m_audioBuffer={1,2,3,4};b.maybeDispatchFt4EarlyDecode(1,6249,7500);show("early",b);b.maybeDispatchFt4EarlyDecode(1,6250,7500);show("dispatch",b);b.maybeDispatchFt4EarlyDecode(1,7000,7500);show("once",b);b.m_transmitting=true;b.maybeDispatchFt4EarlyDecode(2,7000,7500);show("tx",b);b.m_transmitting=false;b.adaptive=true;b.maybeDispatchFt4EarlyDecode(2,7000,7500);show("adaptive",b);b.adaptive=false;b.backlog=true;b.maybeDispatchFt4EarlyDecode(3,7000,7500);show("backlog",b);b.backlog=false;b.pressure=true;b.maybeDispatchFt4EarlyDecode(4,7000,7500);show("pressure",b);b.pressure=false;b.m_audioBuffer.clear();b.maybeDispatchFt4EarlyDecode(5,7000,7500);show("short",b);b.m_audioBuffer={1,2,3,4};b.latency=true;b.maybeDispatchFt4EarlyDecode(5,5200,7500);show("latency",b);b.m_transmitting=true;b.queueFt4DecodeRequest(b.m_audioBuffer,99,123,1,5,0,0);show("txdepth",b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-ft4early-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==10
assert lines[0].startswith('early:0:0:0:') and lines[1]=='dispatch:1:1:1:0:2:3000:4' and lines[2]==lines[1].replace('dispatch','once')
assert lines[3].startswith('tx:1:1:1:') and lines[4].startswith('adaptive:1:1:1:') and lines[5].startswith('backlog:1:1:1:1:')
assert lines[6].startswith('pressure:1:1:1:1:') and lines[7].startswith('short:1:0:1:1:') and lines[8]=='latency:2:1:2:1:2:3000:4' and lines[9]=='txdepth:3:1:2:1:2:3000:4'
print('FT4 early A/B: thresholds, TX guards, CPU/backlog and queued request passed')

#!/usr/bin/env python3
"""Compare remote PCM input guards and sample forwarding."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/decoportaudioinput_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeRemoteRxAudio.cpp').read_text();mark='void DecodiumBridge::onDecoPortRxAudio(';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeRemoteAudioLog(', 'bridgeLog(');assert old==new
source=r'''
#include <QDateTime>
#include <QVector>
#include <iostream>
void bridgeLog(const QString&){}
struct Sink{int samples=0;void injectExternalSamples(const QVector<short>&v){samples+=v.size();}};
class DecodiumBridge{public:static constexpr int SAMPLE_RATE=12000;bool m_decoPortUseRemote=false,m_transmitting=false,m_tuning=false,m_cwRemoteKeying=false;Sink*m_audioSink=nullptr;int m_decoPortRemoteSamples=0,monitored=0;qint64 m_lastDecoPortAudioLogMs=0;void ssbMonitorAudio(const QVector<short>&v){monitored+=v.size();}void onDecoPortRxAudio(const QVector<short>&,quint64);};
#include "methods.inc"
int main(){DecodiumBridge b;Sink s;QVector<short> v{1,2,3};b.onDecoPortRxAudio(v,0);b.m_decoPortUseRemote=true;b.onDecoPortRxAudio(v,0);b.m_audioSink=&s;b.onDecoPortRxAudio({},0);b.m_transmitting=true;b.onDecoPortRxAudio(v,0);b.m_transmitting=false;b.m_tuning=true;b.onDecoPortRxAudio(v,0);b.m_tuning=false;b.m_cwRemoteKeying=true;b.onDecoPortRxAudio(v,0);std::cout<<s.samples<<':'<<b.monitored<<':'<<b.m_decoPortRemoteSamples<<'\n';b.m_cwRemoteKeying=false;b.onDecoPortRxAudio(v,0);std::cout<<s.samples<<':'<<b.monitored<<':'<<b.m_decoPortRemoteSamples<<':'<<(b.m_lastDecoPortAudioLogMs>0)<<'\n';b.onDecoPortRxAudio(v,0);std::cout<<s.samples<<':'<<b.monitored<<':'<<b.m_decoPortRemoteSamples<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-remoteaudio-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='0:0:0\n3:3:3:1\n6:6:6\n',outputs
print('DecoPort audio input A/B: source and TX guards, forwarding and sample count passed')

#!/usr/bin/env python3
"""Compare remote TX framing and PTT handling with a synthetic link."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/decoporttxframes_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeDecoPortTxFrames.cpp').read_text();mark='bool DecodiumBridge::decoPortSendTxWave(';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <iostream>
#define emit
#define connect fakeConnect
template<class...A>void fakeConnect(A...){ }
QStringList events;void bridgeLog(const QString&s){events<<"log:"+s;}namespace decoport{constexpr int StateCanTransmit=1;quint64 nowUnixNs(){return 1000000000ULL;}}
struct FakeTimer{template<class T>explicit FakeTimer(T*){}void setInterval(int){}void start(){events<<"timer-start";}void stop(){events<<"timer-stop";}void timeout(){}};
#define QTimer FakeTimer
struct Remote{struct State{int stateFlags=1;};bool linked=false,allowed=true;QString blocked;int sent=0;QVector<QVector<short>>packets;bool isLinked(){return linked;}bool canTransmit(){return allowed;}State state(){return {};}QString txBlockedReason(){return blocked;}int txAudioLeadMs(){return 250;}void setPtt(bool on,int){events<<QString("ptt:%1").arg(on);}void sendTxAudio(const QVector<short>&v,quint64){packets<<v;sent++;}QString rigLabel(){return "synthetic";}};
class DecodiumBridge:public QObject{public:static constexpr int SAMPLE_RATE=12000;Remote remote,*link=&remote;bool m_remoteIsDecolink=false;QVector<QVector<short>>m_decoPortTxFrames;int m_decoPortTxNextFrame=0;quint64 m_decoPortTxStartNs=0;FakeTimer*m_decoPortTxPacer=nullptr;Remote*remoteLink(){return link;}void errorMessage(const QString&s){events<<"error:"+s;}bool decoPortSendTxWave(const QVector<float>&,double*);void decoPortPumpTxFrames();void decoPortStopTx(const QString&);};
#include "methods.inc"
void show(const char*n,DecodiumBridge&b,bool ok,double d){std::cout<<n<<':'<<ok<<':'<<b.remote.sent<<':'<<b.m_decoPortTxFrames.size()<<':'<<b.m_decoPortTxNextFrame<<':'<<d<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge b;double d=-1;QVector<float>w(4000,.5f);w[0]=2;w[4]=-2;bool ok=b.decoPortSendTxWave(w,&d);show("unlinked",b,ok,d);b.remote.linked=true;b.remote.allowed=false;ok=b.decoPortSendTxWave(w,&d);show("denied",b,ok,d);b.remote.allowed=true;b.remote.blocked="busy";ok=b.decoPortSendTxWave(w,&d);show("blocked",b,ok,d);b.remote.blocked.clear();ok=b.decoPortSendTxWave({},&d);show("empty",b,ok,d);ok=b.decoPortSendTxWave(w,&d);show("sent",b,ok,d);std::cout<<"pcm:"<<b.remote.packets[0][0]<<':'<<b.remote.packets[0][1]<<':'<<b.remote.packets[0].size()<<':'<<b.remote.packets[2].size()<<'\n';b.decoPortStopTx("done");show("stop",b,false,d);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-decoporttxframes-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==7
assert 'no remote radio' in lines[0] and 'does not accept' in lines[1] and 'busy' in lines[2] and lines[3].startswith('empty:0:0:0:0:-1|')
assert lines[4].startswith('sent:1:3:3:3:0.0833333|') and 'ptt:1;timer-start;timer-stop' in lines[4] and lines[5]=='pcm:32767:-32768:480:40'
assert lines[6].startswith('stop:0:3:0:0:') and 'ptt:0' in lines[6]
print('DecoPort TX A/B: admission, PCM clamping/framing, pacing and PTT release passed')

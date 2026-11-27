#!/usr/bin/env python3
"""Compare local/remote receive handoff with a simulated radio link."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/decoportuse_remote_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeDecoPortUseRemote.cpp').read_text();mark='void DecodiumBridge::setDecoPortUseRemote(';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QObject>
#include <QString>
#include <QStringList>
#include <iostream>
#define emit
#define DECODIUM_HAS_SSTV 1
#define connect fakeConnect
#define disconnect fakeDisconnect
template<class...A>void fakeConnect(A...){ }template<class...A>void fakeDisconnect(A...){ }
QStringList events;void bridgeLog(const QString&s){events<<"log:"+s;}
namespace decodium::sstv {enum class SstvAudioSourceKind{DecoPort,Local};}
struct RemoteRadioLink{bool linked=false;bool isLinked(){return linked;}QString rigLabel(){return "remote-rig";}QString modeName(){return "USB";}void setModeName(const QString&s){events<<"remote-mode:"+s;}void rxAudio(){}void stateChanged(){}};RemoteRadioLink* g_decoPortCatLink=nullptr;
class DecodiumBridge:public QObject{public:RemoteRadioLink remote,*link=&remote;bool m_decoPortUseRemote=false,m_monitoring=false,m_monitorRequested=false,m_catConnected=true,m_catConnectedBeforeDecoPort=false,m_remoteIsDecolink=true,legacy=true;int m_decoPortRemoteSamples=5;QString m_catBackend="hamlib",m_catBackendBeforeDecoPort,m_catRigName="local",m_catRigNameBeforeDecoPort,m_catMode="DATA-U",m_catModeBeforeDecoPort,m_mode="FT8",m_ssbRadioMode="USB";
 void ssbStop(){events<<"ssb-stop";}RemoteRadioLink*remoteLink(){return link;}bool usingLegacyBackendForRx(){return legacy;}void stopRx(){events<<"stop-rx";m_monitoring=false;}void startRx(){events<<"start-rx";m_monitoring=true;}void stopAudioCapture(){events<<"stop-capture";}void startAudioCapture(bool){events<<"start-capture";}void ensureAudioSink(){events<<"sink";}void onDecoPortRxAudio(){}void onDecoPortRemoteState(){events<<"state";}void setDecoPortMonitor(bool b){events<<QString("monitor:%1").arg(b);}void statusMessage(const QString&s){events<<"status:"+s;}void catBackendChanged(){events<<"backend-changed";}void catConnectedChanged(){events<<"connected-changed";}void catRigNameChanged(){events<<"name-changed";}void catModeChanged(){events<<"mode-changed";}void decoPortUseRemoteChanged(){events<<"remote-changed";}
 int currentSstvAudioStreamId(decodium::sstv::SstvAudioSourceKind){return 1;}decodium::sstv::SstvAudioSourceKind currentSstvAudioSourceKind(){return decodium::sstv::SstvAudioSourceKind::Local;}void selectSstvRxSource(decodium::sstv::SstvAudioSourceKind,int,bool=false){events<<"sstv-source";}void setDecoPortUseRemote(bool);};
#include "methods.inc"
void show(const char*n,DecodiumBridge&b){std::cout<<n<<':'<<b.m_decoPortUseRemote<<':'<<b.m_catBackend.toStdString()<<':'<<b.m_catConnected<<':'<<b.m_catRigName.toStdString()<<':'<<b.m_catMode.toStdString()<<':'<<(g_decoPortCatLink!=nullptr)<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge b;b.setDecoPortUseRemote(true);show("unlinked",b);b.remote.linked=true;b.m_monitoring=true;b.setDecoPortUseRemote(true);show("on",b);b.setDecoPortUseRemote(true);show("same",b);b.setDecoPortUseRemote(false);show("off",b);b.m_mode="CW";b.m_monitoring=false;b.setDecoPortUseRemote(true);show("cw",b);b.setDecoPortUseRemote(false);show("cw-off",b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-decoportuse-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==6
assert lines[0].startswith('unlinked:0:hamlib:1:local:DATA-U:0|') and 'not linked' in lines[0]
assert lines[1].startswith('on:1:decoport:1:remote-rig:USB:1|') and 'stop-rx' in lines[1] and 'remote-mode:DIGU' in lines[1]
assert lines[2].endswith('|ssb-stop') and lines[3].startswith('off:0:hamlib:1:local:DATA-U:0|') and 'start-capture' in lines[3]
assert 'remote-mode:CW' in lines[4] and lines[5].startswith('cw-off:0:hamlib:1:local:DATA-U:0|')
print('DecoPort remote A/B: link refusal, legacy handoff, modes and local CAT restoration passed')

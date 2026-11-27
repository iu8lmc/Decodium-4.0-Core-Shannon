#!/usr/bin/env python3
"""Compare remote RX audio settings and monitor restart behaviour."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/remoterxaudio_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeRemoteRxAudio.cpp').read_text();mark='int DecodiumBridge::remoteRxVolume() const';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QString>
#include <QVariant>
#include <QMap>
#include <iostream>
struct FakeSettings {static QMap<QString,QVariant> values;QVariant value(const QString&k,const QVariant&d=QVariant())const{return values.value(k,d);}void setValue(const QString&k,const QVariant&v){values[k]=v;}};QMap<QString,QVariant>FakeSettings::values;
class DecodiumBridge{public:double m_remoteRxGain=.7;bool m_decoPortMonitor=false;int notifications=0,stops=0,starts=0;void remoteRxAudioChanged(){notifications++;}void setDecoPortMonitor(bool on){m_decoPortMonitor=on;if(on)starts++;else stops++;}int remoteRxVolume()const;QString remoteRxDevice()const;void setRemoteRxVolume(int);void setRemoteRxDevice(const QString&);};
#define QSettings FakeSettings
#include "methods.inc"
int main(){DecodiumBridge b;std::cout<<b.remoteRxVolume()<<':'<<b.remoteRxDevice().isEmpty()<<'\n';b.setRemoteRxVolume(-10);std::cout<<b.remoteRxVolume()<<':'<<b.m_remoteRxGain<<':'<<b.notifications<<'\n';b.setRemoteRxVolume(150);std::cout<<b.remoteRxVolume()<<':'<<b.m_remoteRxGain<<':'<<b.notifications<<'\n';b.setRemoteRxDevice("A");b.setRemoteRxDevice("A");std::cout<<b.remoteRxDevice().toStdString()<<':'<<b.notifications<<':'<<b.stops<<':'<<b.starts<<'\n';b.m_decoPortMonitor=true;b.setRemoteRxDevice("B");std::cout<<b.remoteRxDevice().toStdString()<<':'<<b.notifications<<':'<<b.stops<<':'<<b.starts<<':'<<b.m_decoPortMonitor<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-remoterx-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='70:1\n0:0:1\n100:1:2\nA:3:0:0\nB:4:1:1:1\n',outputs
print('Remote RX audio A/B: defaults, clamping, gain, persistence and monitor restart passed')

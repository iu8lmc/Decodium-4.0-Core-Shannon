#!/usr/bin/env python3
"""Compare device identity persistence and legacy synchronisation."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/audioidentity_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeAudioIdentity.cpp').read_text();mark='void DecodiumBridge::rememberAudioInputDeviceIdentity(';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QString>
#include <QStringList>
#include <QVariant>
#include <iostream>
#define emit
QStringList events;
struct QAudioDevice{QString name,id;QString description() const{return name;}};
QString audioDeviceIdForSettings(const QAudioDevice&d){return d.id;}
bool audioDeviceNameExactMatches(const QString&a,const QString&b){return !a.trimmed().isEmpty()&&a.trimmed().compare(b.trimmed(),Qt::CaseInsensitive)==0;}
QString audioDeviceSelectionForLog(const QString&a,const QString&b){return a+":"+b;}
QString audioChannelSettingValue(int c){return QString::number(qBound(0,c,3));}
void persistSingleAudioIdentity(bool input,const QString&name,const QString&id){events<<QString("persist:%1:%2:%3").arg(input).arg(name,id);}
void bridgeLog(const QString&s){events<<"log:"+s;}
struct Legacy{void setAudioInputDeviceName(const QString&s){events<<"legacy-in:"+s;}void setAudioOutputDeviceName(const QString&s){events<<"legacy-out:"+s;}};
class DecodiumBridge{public:QString m_audioInputDevice,m_audioOutputDevice,m_audioInputDeviceId,m_audioOutputDeviceId;int m_audioInputChannel=1,m_audioOutputChannel=2;bool m_cachedTxOutputDeviceValid=true,legacy=true;Legacy impl,*m_legacyBackend=&impl;
 bool legacyBackendAvailable(){return legacy;}void invalidateTxAudioCache(){events<<"invalidate";}void audioInputDeviceChanged(){events<<"notify-in";}void sstvRxSourceDeviceChanged(){events<<"notify-sstv";}void audioOutputDeviceChanged(){events<<"notify-out";}
 void syncSettingToLegacyIni(const QString&k,const QVariant&v){events<<"ini:"+k+"="+v.toString();}
 void rememberAudioInputDeviceIdentity(const QAudioDevice&,const QString&,bool);void rememberAudioOutputDeviceIdentity(const QAudioDevice&,const QString&,bool);void syncAudioDeviceSettingsToLegacyIni();};
#include "methods.inc"
void show(DecodiumBridge&b){std::cout<<b.m_audioInputDevice.toStdString()<<':'<<b.m_audioInputDeviceId.toStdString()<<':'<<b.m_audioOutputDevice.toStdString()<<':'<<b.m_audioOutputDeviceId.toStdString()<<':'<<b.m_cachedTxOutputDeviceValid<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge b;QAudioDevice d{"  Device  ","id1"};b.m_audioInputDeviceId="stale";b.m_audioOutputDeviceId="stale";b.rememberAudioInputDeviceIdentity(d,"",false);b.rememberAudioOutputDeviceIdentity(d,"",false);show(b);b.rememberAudioInputDeviceIdentity(d,"reason",true);b.rememberAudioOutputDeviceIdentity(d,"reason",true);show(b);b.m_cachedTxOutputDeviceValid=true;b.rememberAudioInputDeviceIdentity(d,"reason",true);b.rememberAudioOutputDeviceIdentity(d,"reason",true);show(b);d.name=" DEVICE ";d.id="id2";b.rememberAudioInputDeviceIdentity(d,"",true);b.rememberAudioOutputDeviceIdentity(d,"",true);show(b);b.legacy=false;d.name="Second";b.rememberAudioInputDeviceIdentity(d,"",true);b.rememberAudioOutputDeviceIdentity(d,"",true);show(b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-audioidentity-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==5
assert 'persist:1::' in lines[0] and 'persist:0::' in lines[0]
assert 'legacy-in:Device' in lines[1] and 'legacy-out:Device' in lines[1] and 'notify-sstv' in lines[1] and 'invalidate' in lines[1]
assert lines[2]=='Device:id1:Device:id1:1|' and 'invalidate' in lines[3] and 'legacy-' not in lines[3] and 'notify-' not in lines[3]
assert 'legacy-' not in lines[4] and 'notify-in' in lines[4] and 'notify-out' in lines[4]
print('Audio identity A/B: clearing, unchanged IDs, rename, legacy sync and TX cache invalidation passed')

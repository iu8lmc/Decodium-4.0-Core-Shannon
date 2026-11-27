#!/usr/bin/env python3
"""Compare audio device recovery behaviour before and after extraction."""
import os, shlex, subprocess, tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[2]
old = (root/'tests/bridge_refactor_fixtures/audiodevicerecovery_before.inc').read_text()
s = (root/'src/bridge/DecodiumBridgeAudioRecovery.cpp').read_text()
new = s.split('#define bridgeLog decodiumBridgeAudioLevelLog\n',1)[1].split('#undef bridgeLog',1)[0]
assert old == new
source = r'''
#include <QString>
#include <QStringList>
#include <iostream>
#define emit
QStringList events;
void bridgeLog(const QString& s){ events << "log:"+s; }
struct Device { QString value; QString description() const { return value; } };
class DecodiumBridge { public:
 bool m_transmitting=false,m_tuning=false; Device m_cachedDefaultAudioInput,m_cachedDefaultAudioOutput;
 void statusMessage(const QString&s){events<<"status:"+s;} void errorMessage(const QString&s){events<<"error:"+s;}
 void refreshAudioDeviceCache(const QString&s,bool a,bool b){events<<QString("cache:%1:%2:%3").arg(s).arg(a).arg(b);}
 void setAudioInputDevice(const QString&s){events<<"input:"+s;} void setAudioOutputDevice(const QString&s){events<<"output:"+s;}
 void setAudioInputChannel(int n){events<<QString("in-channel:%1").arg(n);} void setAudioOutputChannel(int n){events<<QString("out-channel:%1").arg(n);}
 void refreshAudioDevicesInternal(const QString&s,bool a,bool b){events<<QString("refresh:%1:%2:%3").arg(s).arg(a).arg(b);}
 void refreshAudioDevices(); bool restoreAudioDefaults();
};
#include "methods.inc"
int main(){DecodiumBridge b; b.refreshAudioDevices(); std::cout<<events.join("|").toStdString()<<'\n';events.clear();
 for(int scenario=0;scenario<6;scenario++){b.m_transmitting=scenario==0;b.m_tuning=scenario==1;b.m_cachedDefaultAudioInput.value=(scenario==3||scenario==5)?"  Input  ":"";b.m_cachedDefaultAudioOutput.value=(scenario==4||scenario==5)?" Output ":"";bool ok=b.restoreAudioDefaults();std::cout<<scenario<<':'<<ok<<':'<<events.join("|").toStdString()<<'\n';events.clear();}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-audio-recovery-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1]
lines=out[0].splitlines();assert len(lines)==7 and lines[0]=='refresh:manual refreshAudioDevices:0:1'
assert all(':0:status:Audio recovery postponed' in lines[i] for i in (1,2))
assert '2:0:cache:' in lines[3] and '|error:' in lines[3]
assert '3:1:' in lines[4] and '|input:Input|' in lines[4] and '|output:' not in lines[4]
assert '4:1:' in lines[5] and '|output:Output|' in lines[5] and '|input:' not in lines[5]
assert '5:1:' in lines[6] and '|input:Input|' in lines[6] and '|output:Output|' in lines[6]
assert all('in-channel:0|out-channel:0|refresh:safe audio recovery rebind:0:0' in line for line in lines[4:])
print('Audio recovery A/B: TX/tune guards, missing and default devices, channels and notifications passed')

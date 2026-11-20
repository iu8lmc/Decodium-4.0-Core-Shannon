#!/usr/bin/env python3
"""Compare audio channel serialization and macOS TX channel selection."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/audio_channel_helpers_before.inc').read_text();new=(root/'src/bridge/DecodiumBridgeAudioChannelHelpers.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Multimedia'],text=True))
harness=r'''
#include "Audio/AudioDevice.hpp"
#include <QAudioFormat>
#include <QTextStream>
#include "helpers.inc"
int main(){bool names=audioChannelSettingValue(-1)=="Mono"&&audioChannelSettingValue(1)=="Left"&&audioChannelSettingValue(2)=="Right"&&audioChannelSettingValue(99)=="Both";QAudioFormat mono;mono.setChannelCount(1);QAudioFormat stereo;stereo.setChannelCount(2);bool channels=txOutputChannelForFormat(mono,2)==AudioDevice::Mono&&txOutputChannelForFormat(stereo,0)==AudioDevice::Both&&txOutputChannelForFormat(stereo,1)==AudioDevice::Left&&txOutputChannelForFormat(stereo,2)==AudioDevice::Right&&txOutputChannelForFormat(stereo,99)==AudioDevice::Both;QTextStream(stdout)<<"names="<<names<<" channels="<<channels<<Qt::endl;return names&&channels?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-audio-channel-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);out=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label;subprocess.run(['c++','-std=c++17','-I'+str(d),'-I'+str(root),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True);v=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,v.stdout.strip(),flush=True);out.append(v.stdout)
 assert out[0]==out[1]

#!/usr/bin/env python3
"""Compare RTL-SDR status and RF view guards."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/rtlsdrstatus_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeRtlSdrStatus.cpp').read_text();mark='bool DecodiumBridge::rtlSdrSupported() const';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QStringList>
#include <iostream>
struct RtlSdrInput{bool running=false;static bool compiledIn(){return true;}bool isRunning(){return running;}};
class DecodiumBridge{public:QStringList m_rtlSdrDevices{"device"};QString m_rtlSdrStatus="ready";RtlSdrInput*m_rtlSdrInput=nullptr;int m_rtlSdrRfSampleRate=0;long long m_rtlSdrRfCenterFrequencyHz=0,m_rtlSdrRfSelectedFrequencyHz=0;bool enabled=false;bool rtlSdrEnabled()const{return enabled;}bool rtlSdrSupported()const;QStringList rtlSdrDevices()const;bool rtlSdrRunning()const;QString rtlSdrStatus()const;bool rtlSdrRfView()const;int rtlSdrRfSampleRate()const;double rtlSdrRfCenterFrequency()const;double rtlSdrRfSelectedFrequency()const;};
#include "methods.inc"
int main(){DecodiumBridge b;std::cout<<b.rtlSdrSupported()<<':'<<b.rtlSdrDevices().size()<<':'<<b.rtlSdrStatus().toStdString()<<':'<<b.rtlSdrRunning()<<':'<<b.rtlSdrRfView()<<'\n';RtlSdrInput i;b.m_rtlSdrInput=&i;b.enabled=true;b.m_rtlSdrRfSampleRate=2048000;b.m_rtlSdrRfCenterFrequencyHz=14000000;b.m_rtlSdrRfSelectedFrequencyHz=14074000;std::cout<<b.rtlSdrRfView()<<':';i.running=true;std::cout<<b.rtlSdrRunning()<<':'<<b.rtlSdrRfView()<<':'<<b.rtlSdrRfSampleRate()<<':'<<b.rtlSdrRfCenterFrequency()<<':'<<b.rtlSdrRfSelectedFrequency()<<'\n';b.m_rtlSdrRfCenterFrequencyHz=0;std::cout<<b.rtlSdrRfView()<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-rtlsdrstatus-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='1:1:ready:0:0\n0:1:1:2048000:1.4e+07:1.4074e+07\n0\n',outputs
print('RTL-SDR status A/B: device, running, RF view gates and frequencies passed')

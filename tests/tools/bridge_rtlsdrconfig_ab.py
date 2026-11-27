#!/usr/bin/env python3
"""Compare RTL-SDR direct sampling and receiver configuration gates."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/rtlsdrconfig_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeRtlSdrConfig.cpp').read_text();mark='bool DecodiumBridge::rtlSdrDirectSamplingAvailable(';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QStringList>
#include <QVariant>
#include "src/rtl/RtlSdrCapabilities.h"
#include <iostream>
namespace decodium{QString activeSettingsProfileName(){return "test";}}namespace RtlSdrDsp{enum class Demodulator{WeakSignal,Other};}struct Config{RtlSdrDsp::Demodulator demodulator=RtlSdrDsp::Demodulator::WeakSignal;};struct Input{bool run=false;Config cfg;bool isRunning(){return run;}Config activeConfig(){return cfg;}};struct Cache{template<class F>bool value(const QString&,F f)const{return f();}};
class DecodiumBridge{public:QStringList m_rtlSdrDevices{"RTL-SDR Blog V4","RTL-SDR Blog V3"};Cache m_rtlSdrEnabledCache;bool enabled=false;QString demod="weak";Input*m_rtlSdrInput=nullptr;QVariant getSetting(const QString&k,const QVariant&)const{return k=="RtlSdrEnabled"?QVariant(enabled):QVariant(demod);}bool rtlSdrDirectSamplingAvailable(int)const;bool rtlSdrEnabled()const;bool rtlSdrGeneralReceiverConfigured()const;bool rtlSdrGeneralReceiverActive()const;};
#include "methods.inc"
int main(){DecodiumBridge b;std::cout<<b.rtlSdrDirectSamplingAvailable(-1)<<b.rtlSdrDirectSamplingAvailable(0)<<b.rtlSdrDirectSamplingAvailable(1)<<b.rtlSdrDirectSamplingAvailable(2)<<'\n';std::cout<<b.rtlSdrEnabled()<<b.rtlSdrGeneralReceiverConfigured()<<b.rtlSdrGeneralReceiverActive()<<'\n';b.enabled=true;b.demod="  WEAK  ";std::cout<<b.rtlSdrEnabled()<<b.rtlSdrGeneralReceiverConfigured()<<'\n';b.demod="AM";Input i;b.m_rtlSdrInput=&i;std::cout<<b.rtlSdrGeneralReceiverConfigured()<<b.rtlSdrGeneralReceiverActive();i.run=true;std::cout<<b.rtlSdrGeneralReceiverActive();i.cfg.demodulator=RtlSdrDsp::Demodulator::Other;std::cout<<b.rtlSdrGeneralReceiverActive()<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-rtlsdrconfig-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I',str(r),str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='0010\n000\n10\n1001\n',outputs
print('RTL-SDR configuration A/B: direct sampling, enabled and demodulator guards passed')

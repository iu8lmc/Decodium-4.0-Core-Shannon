#!/usr/bin/env python3
"""Compare remote dial and band changes using a controlled band manager."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/remotefrequency_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeRemoteFrequency.cpp').read_text();mark='void DecodiumBridge::applyRemoteDialFrequency(';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeRemoteFrequencyLog(', 'bridgeLog(');assert old==new
source=r'''
#include <QString>
#include <iostream>
void bridgeLog(const QString&){}
struct Band{QString band="20M",mode;double*frequency;QString currentBandLambda(){return band;}void setCurrentMode(const QString&m){mode=m;}void changeBandByLambda(const QString&b){if(b=="40M"){band=b;*frequency=7074000;}}};
class DecodiumBridge{public:bool m_monitoring=false;QString m_mode="FT8";double m_frequency=14074000,last=0;Band*m_bandManager=nullptr;int requests=0,resets=0;void requestRigFrequencyFromBridge(double hz,const QString&){last=hz;requests++;}void resetRxPeriodAccumulation(bool v){if(v)resets++;}void applyRemoteDialFrequency(double,const QString&);void applyRemoteBandChange(const QString&);};
#include "methods.inc"
int main(){DecodiumBridge b;b.applyRemoteDialFrequency(0,"test");b.applyRemoteBandChange("40m");std::cout<<b.requests<<':'<<b.resets<<'\n';b.applyRemoteDialFrequency(7074000,"test");b.m_monitoring=true;b.applyRemoteDialFrequency(14074000,"test");std::cout<<b.requests<<':'<<b.resets<<':'<<b.last<<'\n';Band band;band.frequency=&b.m_frequency;b.m_bandManager=&band;b.applyRemoteBandChange("  ");b.applyRemoteBandChange("unknown");std::cout<<band.band.toStdString()<<':'<<band.mode.toStdString()<<'\n';b.applyRemoteBandChange(" 40m ");std::cout<<band.band.toStdString()<<':'<<band.mode.toStdString()<<':'<<b.m_frequency<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-remotefrequency-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='0:0\n2:1:1.4074e+07\n20M:FT8\n40M:FT8:7.074e+06\n',outputs
print('Remote frequency A/B: invalid input, monitoring reset, band normalization and mode passed')

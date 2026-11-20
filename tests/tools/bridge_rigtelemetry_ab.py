#!/usr/bin/env python3
"""Compare rig telemetry updates and transmission-limit invocations."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/rigtelemetry_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeRigTelemetry.cpp').read_text();mark='bool DecodiumBridge::catConnected() const';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QString>
#include <iostream>
class DecodiumBridge{public:bool m_catConnected=true,m_rigAlcValid=false;QString m_catRigName="rig",m_catMode="USB";double m_rigPowerWatts=0,m_rigSwr=0,m_rigAlc=0;int notify=0,swrCalls=0,powerCalls=0;void rigTelemetryChanged(){notify++;}void enforceSwrTransmissionLimit(const QString&){swrCalls++;}void enforceWsprPowerTransmissionLimit(const QString&){powerCalls++;}bool catConnected()const;QString catRigName()const;QString catMode()const;void updateRigTelemetry(double,double,double,bool);};
#include "methods.inc"
void show(DecodiumBridge&b){std::cout<<b.m_rigPowerWatts<<':'<<b.m_rigSwr<<':'<<b.m_rigAlc<<':'<<b.m_rigAlcValid<<':'<<b.notify<<':'<<b.swrCalls<<':'<<b.powerCalls<<'\n';}
int main(){DecodiumBridge b;std::cout<<b.catConnected()<<':'<<b.catRigName().toStdString()<<':'<<b.catMode().toStdString()<<'\n';b.updateRigTelemetry(0,0,0,false);show(b);b.updateRigTelemetry(25,2.5,.8,true);show(b);b.updateRigTelemetry(25,2.5,.8,true);show(b);b.updateRigTelemetry(25,2.5,.8,false);show(b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-rigtelemetry-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='1:rig:USB\n0:0:0:0:0:0:0\n25:2.5:0.8:1:1:1:1\n25:2.5:0.8:1:1:1:1\n25:2.5:0.8:0:2:2:2\n',outputs
print('Rig telemetry A/B: state, deduplication and SWR/power safety hooks passed')

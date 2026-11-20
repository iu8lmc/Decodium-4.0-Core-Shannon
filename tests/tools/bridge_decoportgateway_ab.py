#!/usr/bin/env python3
"""Compare DecoPort gateway controls and rig driver state."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/decoportgateway_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeDecoPortGateway.cpp').read_text();mark='QVariantMap DecodiumBridge::decoPortRigDriverState() const';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeDecoPortGatewayLog(', 'bridgeLog(');assert old==new
source=r'''
#include <QObject>
#include <QVariantMap>
#include <iostream>
void bridgeLog(const QString&){}
struct Rig{bool open=false;bool isOpen(){return open;}QString rigName(){return "test rig";}double frequencyHz(){return 14074000;}QString modeName(){return "USB";}QString lastError(){return "error";}};
class DecodiumDecoPortGateway:public QObject{public:bool accept=false;int starts=0,stops=0,port=0;bool start(int p){starts++;port=p;return accept;}QString status(){return "denied";}void stop(){stops++;}};
class DecodiumBridge{public:Rig*m_decoPortRig=nullptr;DecodiumDecoPortGateway*m_decoPortGateway=nullptr;int ensures=0;QObject*decoPortGatewayObject(){return m_decoPortGateway;}void decoPortEnsureRigDriver(){ensures++;}QVariantMap decoPortRigDriverState()const;bool startDecoPortGateway(int);void stopDecoPortGateway();};
#define qobject_cast static_cast
#include "methods.inc"
int main(){DecodiumBridge b;auto a=b.decoPortRigDriverState();std::cout<<a["open"].toBool()<<':'<<a["rigName"].toString().isEmpty()<<':'<<a["error"].toString().isEmpty()<<':'<<b.startDecoPortGateway(1234)<<'\n';Rig rig;DecodiumDecoPortGateway g;b.m_decoPortRig=&rig;b.m_decoPortGateway=&g;a=b.decoPortRigDriverState();std::cout<<a["open"].toBool()<<':'<<a["error"].toString().toStdString()<<'\n';rig.open=true;a=b.decoPortRigDriverState();std::cout<<a["open"].toBool()<<':'<<a["rigName"].toString().toStdString()<<':'<<a["modeName"].toString().toStdString()<<':'<<a["frequencyHz"].toDouble()<<'\n';std::cout<<b.startDecoPortGateway(1234)<<':';g.accept=true;std::cout<<b.startDecoPortGateway(5678)<<':'<<b.ensures<<':'<<g.starts<<':'<<g.port<<'\n';b.stopDecoPortGateway();std::cout<<g.stops<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-gateway-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='0:1:1:0\n0:error\n1:test rig:USB:1.4074e+07\n0:1:2:2:5678\n1\n',outputs
print('DecoPort gateway A/B: missing/closed/open rig, start failure/success and stop passed')

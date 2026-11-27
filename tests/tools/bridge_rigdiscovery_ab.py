#!/usr/bin/env python3
"""Compare rig discovery result and diagnostic reporting."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/rigdiscovery_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeRigDiscovery.cpp').read_text();mark='QVariantList DecodiumBridge::detectConnectedRigs() const';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeRigDiscoveryLog(', 'bridgeLog(');assert old==new
source=r'''
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include <iostream>
QStringList logs;void bridgeLog(const QString&s){logs<<s;}namespace DecodiumRigDetector{QVariantList candidates;QVariantList detect(){return candidates;}}
class DecodiumBridge{public:QVariantList detectConnectedRigs()const;};
#include "methods.inc"
int main(){DecodiumBridge b;auto v=b.detectConnectedRigs();std::cout<<v.size()<<':'<<logs.size()<<'\n';logs.clear();DecodiumRigDetector::candidates={QVariantMap{{"rigLabel","radio"},{"catPort","port"},{"confidence",99},{"evidence","usb"}}};v=b.detectConnectedRigs();std::cout<<v.size()<<':'<<v[0].toMap()["rigLabel"].toString().toStdString()<<':'<<logs.size()<<':'<<logs[1].contains("port")<<':'<<logs[1].contains("99")<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-rigdiscovery-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='0:1\n1:radio:2:1:1\n',outputs
print('Rig discovery A/B: empty and candidate data plus diagnostic reporting passed')

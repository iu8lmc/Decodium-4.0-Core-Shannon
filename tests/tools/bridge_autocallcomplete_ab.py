#!/usr/bin/env python3
"""Compare Auto Call completion, counter and limit notifications."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/autocallcomplete_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeAutoCall.cpp').read_text();mark='void DecodiumBridge::noteAutoCallQsoCompleted()';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeLogbookLog(', 'bridgeLog(');assert old==new
source=r'''
#include <QString>
#include <iostream>
void bridgeLog(const QString&){}
class DecodiumBridge{public:bool m_autoCallEnabled=false;QString m_autoCallCurrentCandidate;int m_autoCallQsoCount=0,m_autoCallMaxQsos=0,notify=0,status=0;void autoCallQsoCountChanged(){notify++;}void statusMessage(const QString&){status++;}void noteAutoCallQsoCompleted();};
#include "methods.inc"
void show(DecodiumBridge&b){std::cout<<b.m_autoCallQsoCount<<':'<<b.notify<<':'<<b.status<<':'<<b.m_autoCallCurrentCandidate.toStdString()<<'\n';}
int main(){DecodiumBridge b;b.m_autoCallCurrentCandidate="K1ABC";b.noteAutoCallQsoCompleted();show(b);b.m_autoCallEnabled=true;b.m_autoCallCurrentCandidate="";b.noteAutoCallQsoCompleted();show(b);b.m_autoCallCurrentCandidate="K1ABC";b.noteAutoCallQsoCompleted();show(b);b.m_autoCallMaxQsos=2;b.m_autoCallCurrentCandidate="K2XYZ";b.noteAutoCallQsoCompleted();show(b);b.noteAutoCallQsoCompleted();show(b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-autocallcomplete-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='0:0:0:K1ABC\n0:0:0:\n1:1:0:\n2:2:1:\n2:2:1:\n',outputs
print('Auto Call completion A/B: guards, counter, candidate reset and limit notification passed')

#!/usr/bin/env python3
"""Compare QSY frequency/mode ordering and preservation guard."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/qsycommand_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeQsyCommand.cpp').read_text();mark='void DecodiumBridge::qsyTo(';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QStringList>
#include <iostream>
class DecodiumBridge{public:QString m_mode="FT8";bool m_preserveFrequencyOnModeChange=false;QStringList calls;void setFrequency(double v){calls<<QString("frequency:%1").arg(v);}void setMode(const QString&v){calls<<QString("mode:%1:%2").arg(v).arg(m_preserveFrequencyOnModeChange);m_mode=v;}void requestRigFrequencyFromBridge(double,const QString&reason){calls<<"request:"+reason;}void statusMessage(const QString&){calls<<"status";}void qsyTo(double,const QString&);};
#include "methods.inc"
int main(){for(int k=0;k<4;k++){DecodiumBridge b;b.m_preserveFrequencyOnModeChange=k==3;b.qsyTo(7074000,k==0?"":(k==1?"FT8":"FT4"));std::cout<<b.calls.join(',').toStdString()<<':'<<b.m_preserveFrequencyOnModeChange<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-qsy-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='frequency:7.074e+06,request:qsy,status:0\nfrequency:7.074e+06,request:qsy,status:0\nfrequency:7.074e+06,mode:FT4:1,request:qsy,status:0\nfrequency:7.074e+06,mode:FT4:1,request:qsy,status:1\n',outputs
print('QSY command A/B: operation order, mode guards and frequency-preservation state passed')

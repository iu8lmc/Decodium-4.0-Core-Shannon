#!/usr/bin/env python3
"""Compare Hound TX selection and payload readiness guards."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/houndtxprepare_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeHoundTxPrepare.cpp').read_text();mark='bool DecodiumBridge::prepareHoundTxSelectionForStart(';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeHoundTxPrepareLog(', 'bridgeLog(');assert old==new
source=r'''
#include <QString>
#include <iostream>
constexpr int kSpecialOpHound=7;void bridgeLog(const QString&){}
class DecodiumBridge{public:int m_specialOperationActivity=7,m_currentTx=1,errors=0,status=0,regen=0,selected=0;QString m_tx1,m_tx3,m_dxCall;bool allow=true,regenGood=true;bool selectCurrentTxIfAllowed(int n,const QString&){selected++;if(allow)m_currentTx=n;return allow;}void regenerateTxMessages(){regen++;if(regenGood){m_tx1="FOX CALL";m_tx3="FOX RRR";}}void errorMessage(const QString&){errors++;}void statusMessage(const QString&){status++;}bool prepareHoundTxSelectionForStart(const QString&);};
#include "methods.inc"
int main(){for(int k=0;k<7;k++){DecodiumBridge b;if(k==0)b.m_specialOperationActivity=0;if(k==1){b.m_currentTx=4;b.allow=false;}if(k==2)b.m_tx1="ready";if(k==3){b.m_tx1="bad message";b.m_dxCall="FOX";}if(k==4)b.m_dxCall="";if(k==5){b.m_dxCall="FOX";b.regenGood=false;}if(k==6){b.m_currentTx=3;b.m_tx3="ready3";}bool ok=b.prepareHoundTxSelectionForStart("test");std::cout<<ok<<':'<<b.m_currentTx<<':'<<b.selected<<':'<<b.regen<<':'<<b.errors<<':'<<b.status<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-houndprepare-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='1:1:0:0:0:0\n0:4:1:0:0:0\n1:1:0:0:0:0\n1:1:0:1:0:0\n0:1:0:0:1:1\n0:1:0:1:1:1\n1:3:0:0:0:0\n',outputs
print('Hound preparation A/B: selection, payload validation, regeneration and rejection passed')

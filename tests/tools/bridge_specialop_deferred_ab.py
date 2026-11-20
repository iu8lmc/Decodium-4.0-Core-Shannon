#!/usr/bin/env python3
"""Compare deferred activity transitions under controlled QSO and TX states."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/specialop_before.inc').read_text();new=(r/'src/bridge/DecodiumBridgeSpecialOperation.cpp').read_text().replace('decodiumBridgeSpecialOpLog(', 'bridgeLog(')
def part(s):return s[s.index('bool DecodiumBridge::specialOperationTransitionBlockedByQsoFinalisation() const'):s.index('void DecodiumBridge::setSpecialOperationActivity(')]
src=r'''
#include <QObject>
#include <QString>
#include <iostream>
constexpr int kSpecialOpNone=0;void bridgeLog(const QString&){} namespace decodium::seq {bool specialOperationTransitionRequiresDeferral(bool a,bool b){return a||b;}}
struct DecodiumBridge{int m_deferredSpecialOperationActivity=-1,applied=-1,signalCount=0;bool m_transmitting=false,m_tuning=false,m_logAfterOwn73=false,m_logPromptOpen=false;void specialOperationChangePendingChanged(){signalCount++;}void setSpecialOperationActivity(int a){applied=a;}bool specialOperationTransitionBlockedByQsoFinalisation()const;bool applyDeferredSpecialOperationActivityIfReady();};
#include "methods.inc"
int main(){for(int state=0;state<6;state++){DecodiumBridge b;b.m_deferredSpecialOperationActivity=state?6:-1;b.m_transmitting=state==1;b.m_tuning=state==2;b.m_logAfterOwn73=state==3;b.m_logPromptOpen=state==4;bool ok=b.applyDeferredSpecialOperationActivityIfReady();std::cout<<ok<<':'<<b.m_deferredSpecialOperationActivity<<':'<<b.applied<<':'<<b.signalCount<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-specialop-deferred-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for s in (old,new):
  (p/'methods.inc').write_text(part(s));subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1] and out[0].splitlines()==['0:-1:-1:0']+['1:6:-1:0']*4+['1:-1:6:1'],out
print('special operation deferral A/B: TX, tune, final 73, log prompt and apply passed')

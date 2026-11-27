#!/usr/bin/env python3
"""Check special-operation source and compare state transitions with substitutes."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/specialop_before.inc').read_text();new='int DecodiumBridge::ncontest() const'+(r/'src/bridge/DecodiumBridgeSpecialOperation.cpp').read_text().split('int DecodiumBridge::ncontest() const',1)[1];new=new.replace('decodiumBridgeSpecialOpLog(','bridgeLog(')
if old!=new:raise SystemExit('special operation source differs')
method=old[old.index('void DecodiumBridge::updateSpecialOperationFromLegacy('):old.index('void DecodiumBridge::syncSpecialOperationToLegacyBackend(')]
src=r'''
#include <QtGlobal>
#include <QObject>
#include <iostream>
constexpr int kSpecialOpNone=0,kSpecialOpMax=9,kSpecialOpFox=6,kSpecialOpHound=7;
struct DecodiumBridge {int m_specialOperationActivity=0,m_ncontest=0;bool m_useLegacyTxBackend=false,m_forceLegacyTxForSpecialOp=false,m_foxMode=false,m_houndMode=false;int change=0,contest=0,fox=0,hound=0,udp=0;void specialOperationActivityChanged(){++change;}void ncontestChanged(){++contest;}void foxModeChanged(){++fox;}void houndModeChanged(){++hound;}void udpSendStatus(){++udp;}void updateSpecialOperationFromLegacy(int);};
#include "method.inc"
int main(){for(bool legacy:{false,true}){DecodiumBridge b;b.m_useLegacyTxBackend=legacy;for(int a:{-1,0,6,6,7,1,99,0}){b.updateSpecialOperationFromLegacy(a);std::cout<<b.m_specialOperationActivity<<':'<<b.m_ncontest<<':'<<b.m_forceLegacyTxForSpecialOp<<':'<<b.m_foxMode<<':'<<b.m_houndMode<<':'<<b.change<<':'<<b.contest<<':'<<b.fox<<':'<<b.hound<<':'<<b.udp<<'\n';}}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-specialop-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for code in (method,new[new.index('void DecodiumBridge::updateSpecialOperationFromLegacy('):new.index('void DecodiumBridge::syncSpecialOperationToLegacyBackend(')]):
  (p/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1] and len(outputs[0].splitlines())==16
print('special operation A/B: identical source and 16 legacy/transition cases')

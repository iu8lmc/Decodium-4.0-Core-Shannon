#!/usr/bin/env python3
"""Compare timing controls including the legacy Hound restrictions."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]; before=(r/'tests/bridge_refactor_fixtures/txtimingcontrols_before.inc').read_text()
after='void DecodiumBridge::setTxPeriod(int v)'+(r/'src/bridge/DecodiumBridgeTxSlot.cpp').read_text().split('void DecodiumBridge::setTxPeriod(int v)',1)[1]
if before!=after:raise SystemExit('timing controls differ')
source=r'''
#include <iostream>
#define emit
constexpr int kSpecialOpHound=7;
struct Legacy {int first=-1,alt=-1,nfirst=0,nalt=0;void setTxFirst(bool b){first=b;++nfirst;}void setAlt12Enabled(bool b){alt=b;++nalt;}};
struct DecodiumBridge {int m_txPeriod=0,m_specialOperationActivity=0,periodChanges=0,altChanges=0;bool m_alt12Enabled=false,enabled=true;Legacy leg,*m_legacyBackend=&leg;bool usingLegacyBackendForTx()const{return enabled;}void txPeriodChanged(){++periodChanges;}void alt12EnabledChanged(){++altChanges;}void setTxPeriod(int);void setAlt12Enabled(bool);};
#include "method.inc"
int main(){DecodiumBridge b;auto show=[&](){std::cout<<b.m_txPeriod<<','<<b.m_alt12Enabled<<','<<b.leg.first<<','<<b.leg.alt<<','<<b.leg.nfirst<<','<<b.leg.nalt<<','<<b.periodChanges<<','<<b.altChanges<<'\n';};b.setTxPeriod(4);b.setAlt12Enabled(true);show();b.setTxPeriod(1);b.setAlt12Enabled(true);show();b.m_specialOperationActivity=7;b.setTxPeriod(0);b.setAlt12Enabled(false);b.setTxPeriod(1);b.setAlt12Enabled(true);show();b.enabled=false;b.setTxPeriod(0);b.setAlt12Enabled(false);show();}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-txcontrols-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(source)
 for code in (before,after):
  (p/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
expected='1,1,1,1,1,1,1,1\n1,1,1,1,1,1,1,1\n1,1,0,0,3,3,3,3\n0,0,0,0,3,3,4,4\n'
if outputs!=[expected,expected]:raise SystemExit('TX timing controls A/B mismatch: '+repr(outputs))
print('TX timing controls A/B OK\n'+expected,end='')

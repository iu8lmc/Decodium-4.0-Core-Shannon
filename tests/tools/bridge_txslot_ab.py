#!/usr/bin/env python3
"""Compare TX slot selection, guards and legacy updates against the snapshot."""
import os, shlex, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
before=(r/'tests/bridge_refactor_fixtures/txslot_before.inc').read_text()
after=(r/'src/bridge/DecodiumBridgeTxSlot.cpp').read_text().split('int DecodiumBridge::currentTx() const',1)[1]
after=('int DecodiumBridge::currentTx() const'+after).split('\nvoid DecodiumBridge::setTxPeriod(int v)',1)[0]
after=after.replace('decodiumBridgeTxSlotLog(', 'bridgeLog(')
if before!=after: raise SystemExit('TX slot implementations differ')
harness=r'''
#include <QString>
#include <iostream>
#define emit
void bridgeLog(QString const&) {}
struct Legacy { int chosen=0; void selectTxMessage(int n) {chosen=n;} };
class DecodiumBridge {
public:
 int m_currentTx=1,m_explicitManualTxRequest=1,m_ft2AsyncSmartTxSerial=1,m_ft2AsyncSmartTxDeadlineMs=5,m_ft2AsyncSmartTxRetries=3;
 bool m_ft2AsyncSmartTxPending=true,m_periodicTxCheckScheduled=true,m_ft2ManualClickTx1BypassPeriodOnce=true,m_asyncTxEnabled=true, legacyEnabled=true;
 int mask=0,reset=0,changed=0,messageChanged=0,cache=0,precompute=0,status=0;
 QString m_mode="FT2"; Legacy legacy; Legacy *m_legacyBackend=&legacy;
 bool isTxDisabled(int n)const{return mask & (1<<(n-1));}
 bool usingLegacyBackendForTx()const{return legacyEnabled;}
 void resetCompletedQsoLatchesForCq(QString const&){++reset;}
 void currentTxChanged(){++changed;} void currentTxMessageChanged(){++messageChanged;}
 void invalidateTxAudioCache(){++cache;} void scheduleTxAudioPrecompute(){++precompute;}
 void statusMessage(QString const&){++status;}
 int currentTx() const; bool selectCurrentTxIfAllowed(int,QString const&); bool ensureCurrentTxCanTransmit(QString const&); void setCurrentTx(int);
};
#include "method.inc"
int main(){
 DecodiumBridge b;
 auto state=[&](char const *label,bool result){std::cout<<label<<':'<<result<<','<<b.currentTx()<<','<<b.m_explicitManualTxRequest<<','<<b.m_ft2AsyncSmartTxPending<<','<<b.m_ft2AsyncSmartTxSerial<<','<<b.m_ft2AsyncSmartTxDeadlineMs<<','<<b.m_ft2AsyncSmartTxRetries<<','<<b.m_periodicTxCheckScheduled<<','<<b.m_ft2ManualClickTx1BypassPeriodOnce<<','<<b.legacy.chosen<<','<<b.reset<<','<<b.changed<<','<<b.messageChanged<<','<<b.cache<<','<<b.precompute<<','<<b.status<<'\n';};
 state("invalid",b.selectCurrentTxIfAllowed(0,"")); b.mask=2;state("disabled",b.selectCurrentTxIfAllowed(2,""));b.mask=0;
 state("changed",b.selectCurrentTxIfAllowed(3,""));state("same",b.selectCurrentTxIfAllowed(3,""));
 state("cq",b.selectCurrentTxIfAllowed(6,""));b.mask=32;state("guard-disabled",b.ensureCurrentTxCanTransmit(""));b.mask=0;state("guard-ok",b.ensureCurrentTxCanTransmit(""));
 b.legacyEnabled=false;b.m_mode="FT8";b.setCurrentTx(2);state("modern",true);b.m_currentTx=0;state("guard-invalid",b.ensureCurrentTxCanTransmit(""));
}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outs=[]
with tempfile.TemporaryDirectory(prefix='decodium-txslot-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(harness)
 for code in (before,after):
  (p/'method.inc').write_text(code)
  subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True)
  outs.append(subprocess.check_output([str(p/'test')],text=True))
if outs[0]!=outs[1]:raise SystemExit('TX slot A/B mismatch')
lines=outs[0].splitlines()
assert len(lines)==9 and lines[0].startswith('invalid:0,1,') and lines[1].startswith('disabled:0,1,') and lines[2].startswith('changed:1,3,0,0,2,0,0,0,0,3,') and lines[4].startswith('cq:1,6,') and lines[5].startswith('guard-disabled:0,6,') and lines[-1].startswith('guard-invalid:0,0,')
print('TX slot A/B OK\n'+outs[0],end='')

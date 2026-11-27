#!/usr/bin/env python3
"""Compare QSO transitions using the actual shared sequencer rules."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/qsotransition_before.inc').read_text();new='bool DecodiumBridge::canAdvance('+(r/'src/bridge/DecodiumBridgeQsoTransition.cpp').read_text().split('bool DecodiumBridge::canAdvance(',1)[1];new=new.replace('decodiumBridgeQsoTransitionLog(', 'bridgeLog(');assert old==new
src=r'''
#include <QObject>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <iostream>
#include "Sequencer/QsoSequencerRules.hpp"
int logs=0;void bridgeLog(const QString&){logs++;}
struct DecodiumBridge{bool m_ft2TransitionCensus=false,m_asyncTxEnabled=false,m_quickQsoEnabled=false,allow=true;QString m_mode="FT8",m_callsign="9H1SR";int m_currentTx=6,m_qsoProgress=0,m_lastNtx=0,m_txRetryCount=0,m_txDisabledMask=0,m_txWatchdogTicks=7,signalsCount=0,remembered=0,pruned=0;bool selectCurrentTxIfAllowed(int n,const QString&){if(!allow)return false;m_currentTx=n;return true;}void qsoProgressChanged(){signalsCount++;}void rememberPartnerStateV2(const QString&){remembered++;}void prunePartnerMemoryIfDueV2(){pruned++;}bool canAdvance(int,int)const;bool advanceQsoState(int);};
#include "methods.inc"
int main(){DecodiumBridge b;for(int from=0;from<=6;from++)for(int to=0;to<=6;to++)std::cout<<b.canAdvance(from,to);std::cout<<'\n';for(int k=0;k<9;k++){DecodiumBridge x;int tx=1;if(k==1)x.m_quickQsoEnabled=true;if(k==2)x.m_txDisabledMask=1;if(k==3){x.m_quickQsoEnabled=true;tx=3;}if(k==4)x.allow=false;if(k==5)tx=0;if(k==6)tx=7;if(k==7){x.m_ft2TransitionCensus=true;x.m_mode="FT2";x.m_asyncTxEnabled=true;tx=4;}if(k==8)tx=6;logs=0;bool ok=x.advanceQsoState(tx);std::cout<<ok<<':'<<x.m_currentTx<<':'<<x.m_qsoProgress<<':'<<x.m_txWatchdogTicks<<':'<<x.signalsCount<<':'<<x.remembered<<':'<<x.pruned<<':'<<logs<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-qsotransition-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'methods.inc').write_text(code);files=[p/'main.cpp',r/'Sequencer/QsoSequencerRules.cpp',r/'Sequencer/MessageTokenRules.cpp',r/'src/radio/Radio.cpp'];subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(r),'-I'+str(r/'src/radio'),'-I'+str(r/'src/core'),*[str(x) for x in files],*flags,'-o',str(p/'test')],check=True);env=os.environ.copy();env.pop('LOCALAPPDATA',None);out.append(subprocess.check_output([str(p/'test')],text=True,env=env))
assert out[0]==out[1],out
lines=out[0].splitlines();assert len(lines)==10 and len(lines[0])==49 and lines[1].startswith('1:1:2:0:') and lines[2].startswith('1:2:3:0:') and lines[3].startswith('1:2:3:0:') and lines[4].startswith('1:3:5:0:') and lines[5].startswith('0:'),lines
print('QSO transition A/B: 49 state pairs, actual remap rules, Quick QSO and census passed')

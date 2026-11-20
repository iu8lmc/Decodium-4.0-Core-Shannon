#!/usr/bin/env python3
"""Compare bridge FT8 gate wiring using the real decode gate implementation."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/ft8bridgegate_before.inc').read_text();new='bool DecodiumBridge::usesFt8AutoTxDecodeGate() const'+(r/'src/bridge/DecodiumBridgeFt8AutoGate.cpp').read_text().split('bool DecodiumBridge::usesFt8AutoTxDecodeGate() const',1)[1]
for a,b in [('decodiumBridgeFt8GateLog(', 'bridgeLog('),('decodiumBridgeTruthyEnvironmentFlag(', 'truthyEnvironmentFlag('),('decodiumBridgeLatestD3SyncStartMs(', 'latestD3CompatibleSyncTxStartMs('),('decodiumBridgeTxPeriodIsEven(', 'bridgeTxPeriodIsEven(')]:new=new.replace(a,b)
assert old==new
src=r'''
#include <QCoreApplication>
#include <QTimer>
#include <QString>
#include <iostream>
#include "src/bridge/Ft8AutoTxDecodeGate.h"
void bridgeLog(const QString&){}bool truthyEnvironmentFlag(const char*){return false;}bool bridgeTxPeriodIsEven(int p){return p==0;}int latestD3CompatibleSyncTxStartMs(const QString&,int,bool){return 1234;}
class DecodiumBridge:public QObject{public:QString m_mode="FT8",m_callsign="9H1SR";bool legacy=false,tci=false,m_decoPortUseRemote=false,multi=false,m_cwTxActive=false,m_telemetryTxActive=false,m_autoSeq=true,m_txEnabled=true,m_autoCqRepeat=false,m_activeFt8AutoTxFullFrame=false,m_manualTxHold=false,m_monitoring=true,m_transmitting=false,m_tuning=false;int m_explicitManualTxRequest=0,m_txPeriod=0,m_currentTx=1,cancel=0,checks=0;qint64 now=0;quint64 m_periodTimerSessionId=1,m_txRequestGeneration=1;decodium::Ft8AutoTxDecodeGate m_ft8AutoTxDecodeGate;decodium::Ft8AutoTxDecodeGate::Context m_activeFt8AutoTxContext;bool usingLegacyBackendForTx()const{return legacy;}bool usingTciAudioInput()const{return tci;}bool mamMultiStreamSequencerActive()const{return multi;}int effectivePeriodMsForMode(const QString&)const{return 15000;}qint64 correctedUtcEpochMs()const{return now;}void completeTxPlayback(const QString&,bool){cancel++;}void checkAndStartPeriodicTx(){checks++;}bool usesFt8AutoTxDecodeGate()const;decodium::Ft8AutoTxDecodeGate::Context ft8AutoTxDecodeContext()const;int syncTxCommitDeadlineMs(bool)const;bool validateFt8AutoTxAudioStart(qint64);bool deferFt8AutoTxForDecode();};
#include "methods.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(int k=0;k<9;k++){DecodiumBridge b;if(k==1)b.legacy=true;if(k==2)b.tci=true;if(k==3)b.multi=true;if(k==4)b.m_explicitManualTxRequest=1;if(k==5)b.m_callsign="";if(k==6)b.m_mode="FT2";if(k==7)b.m_decoPortUseRemote=true;if(k==8)b.m_telemetryTxActive=true;std::cout<<b.usesFt8AutoTxDecodeGate()<<':'<<b.syncTxCommitDeadlineMs(false)<<'\n';}DecodiumBridge b;b.now=100;bool waiting=b.deferFt8AutoTxForDecode();auto c=b.ft8AutoTxDecodeContext();b.m_ft8AutoTxDecodeGate.decoded(c,100);bool ready=b.deferFt8AutoTxForDecode();b.m_activeFt8AutoTxFullFrame=true;b.m_activeFt8AutoTxContext=c;bool fits=b.validateFt8AutoTxAudioStart(12640);b.now=14999;bool late=b.validateFt8AutoTxAudioStart(12640);std::cout<<"gate:"<<waiting<<':'<<ready<<':'<<fits<<':'<<late<<':'<<b.cancel<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-ft8bridgegate-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'methods.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(r),str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1],out
lines=out[0].splitlines();assert len(lines)==10 and lines[0].startswith('1:') and all(x=='0:1234' for x in lines[1:9]) and lines[-1]=='gate:1:0:1:0:1',lines
print('FT8 bridge gate A/B: enable guards, decoded readiness and audio deadline passed')

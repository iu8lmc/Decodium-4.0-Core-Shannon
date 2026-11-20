#!/usr/bin/env python3
"""Compare decode/TX rendezvous guards, pending filters and Qt timers."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/decodetxwait_before.inc').read_text();new='bool DecodiumBridge::shouldDeferAutoTxUntilTimeSyncDecode('+(r/'src/bridge/DecodiumBridgeDecodeTxRendezvous.cpp').read_text().split('bool DecodiumBridge::shouldDeferAutoTxUntilTimeSyncDecode(',1)[1];new=new.replace('decodiumBridgeDecodeTxWaitLog(', 'bridgeLog(').replace('decodiumBridgeLatestD3SyncStartMs(', 'latestD3CompatibleSyncTxStartMs(');assert old==new
src=r'''
#include <QObject>
#include <QCoreApplication>
#include <QTimer>
#include <QEventLoop>
#include <QHash>
#include <QSet>
#include <iostream>
void bridgeLog(const QString&){}int autoTxDecodeGraceMs(const QString& m,bool){return m=="FT2"?100:60;}int latestD3CompatibleSyncTxStartMs(const QString&,int,bool){return 500;}
class DecodiumBridge:public QObject{public:QString m_mode="FT4",m_callsign="9H1SR",m_dxCall="K1ABC";bool legacy=false,m_manualTxHold=false,m_monitoring=true,m_transmitting=false,m_tuning=false,m_asyncTxEnabled=false,m_autoSeq=true,m_txEnabled=true,m_autoCqRepeat=false,m_ft8FastSequence=false,pressure=false,m_lowCpuModeEnabled=false,ft8gate=false,defer=false;QHash<quint64,QString>m_decodeModeBySerial;QHash<quint64,qint64>m_decodeStartMsBySerial;QSet<quint64>m_ft8EarlyDecodeSerials,m_ft8DeepInTxSerials;quint64 m_periodTimerSessionId=1,m_txRequestGeneration=1;int checks=0,reschedules=0; qint64 now=0;bool isTimeSyncDecodeMode(const QString& m)const{return m=="FT2"||m=="FT4"||m=="FT8";}bool usingLegacyBackendForTx()const{return legacy;}bool cpuPressureActive()const{return pressure;}bool usesFt8AutoTxDecodeGate()const{return ft8gate;}bool deferFt8AutoTxForDecode(){return defer;}void checkAndStartPeriodicTx(){checks++;}int periodMsForMode(const QString&)const{return 1000;}bool effectiveRelaxLatestCap()const{return false;}qint64 correctedUtcEpochMs()const{return now;}void scheduleSyncTxAtNextValidSlot(const QString&,int,int){reschedules++;}bool shouldDeferAutoTxUntilTimeSyncDecode(const QString&)const;bool hasPendingTimeSyncDecodeForMode(const QString&)const;int effectiveAutoTxDecodeGraceMs(const QString&)const;void scheduleDeferredAutoTxAfterTimeSyncDecode(const QString&,quint64);};
#include "methods.inc"
void wait(){QEventLoop loop;QTimer::singleShot(110,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(int k=0;k<7;k++){DecodiumBridge b;if(k==1)b.legacy=true;if(k==2)b.m_manualTxHold=true;if(k==3)b.m_monitoring=false;if(k==4)b.m_transmitting=true;if(k==5){b.m_mode="FT2";b.m_asyncTxEnabled=true;}if(k==6)b.m_callsign="";std::cout<<"guard:"<<b.shouldDeferAutoTxUntilTimeSyncDecode(b.m_mode)<<'\n';}DecodiumBridge p;p.m_decodeModeBySerial[1]="FT4";p.m_decodeStartMsBySerial[1]=1;std::cout<<"pending:"<<p.hasPendingTimeSyncDecodeForMode("ft4")<<':';p.m_ft8EarlyDecodeSerials.insert(1);std::cout<<p.hasPendingTimeSyncDecodeForMode("FT4")<<'\n';p.pressure=true;std::cout<<"grace:"<<p.effectiveAutoTxDecodeGraceMs("FT2")<<':'<<p.effectiveAutoTxDecodeGraceMs("FT4")<<'\n';for(int k=0;k<7;k++){DecodiumBridge b;if(k==1)b.m_txRequestGeneration++;if(k==2)b.now=300;if(k==3)b.now=600;if(k==4)b.ft8gate=true;if(k==5){b.ft8gate=true;b.defer=true;}b.scheduleDeferredAutoTxAfterTimeSyncDecode("FT4",1);if(k==1)b.m_txRequestGeneration++;if(k==6)b.m_manualTxHold=true;wait();std::cout<<"timer:"<<b.checks<<':'<<b.reschedules<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-decodetxwait-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1],out
lines=out[0].splitlines();assert lines[:7]==['guard:1']+['guard:0']*6 and lines[7:9]==['pending:1:0','grace:80:900'] and lines[9:]==['timer:1:0','timer:0:0','timer:1:0','timer:0:1','timer:1:0','timer:0:0','timer:0:0'],lines
print('decode/TX wait A/B: guards, pending serials, CPU grace, deadlines and cancellation passed')

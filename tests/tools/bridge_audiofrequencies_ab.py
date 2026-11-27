#!/usr/bin/env python3
"""Compare RX/TX audio frequency paths and persistence before/after."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/audiofrequencies_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeAudioFrequencies.cpp').read_text();mark='void DecodiumBridge::setRxFrequency(int f)';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QString>
#include <QDateTime>
#include <QMap>
#include <iostream>
struct QSettings {enum Format {IniFormat};enum Scope {UserScope};static QMap<QString,int> values;QSettings(Format,Scope,const QString&,const QString&){} void setValue(const QString&k,int v){values[k]=v;}};QMap<QString,int>QSettings::values;namespace decodium {void beginActiveSettingsProfile(QSettings&) {}}
struct Legacy{int rx=0,tx=0,nrx=0,ntx=0;void setRxFrequency(int n){rx=n;nrx++;}void setTxFrequency(int n){tx=n;ntx++;}};
class DecodiumBridge {public:int m_rxFrequency=1500,m_txFrequency=1500,m_localRxFrequencyTargetHz=0,m_localTxFrequencyTargetHz=0; qint64 m_localRxFrequencyGuardUntilMs=0,m_localTxFrequencyGuardUntilMs=0;bool available=true,legacyOwner=false;Legacy*m_legacyBackend;int rxSignal=0,txSignal=0,rebuild=0,invalid=0,split=0,precompute=0;bool legacyBackendAvailable(){return available;}bool usingLegacyBackendForTx(){return legacyOwner;}void rxFrequencyChanged(){rxSignal++;}void txFrequencyChanged(){txSignal++;}void rebuildRxDecodeList(){rebuild++;}void invalidateTxAudioCache(){invalid++;}void syncActiveCatTxSplitFrequency(const QString&){split++;}void scheduleTxAudioPrecompute(){precompute++;}void setRxFrequency(int);void setTxFrequency(int);};
#include "methods.inc"
void show(DecodiumBridge&b,Legacy&l){std::cout<<b.m_rxFrequency<<':'<<b.m_txFrequency<<':'<<b.m_localRxFrequencyTargetHz<<':'<<b.m_localTxFrequencyTargetHz<<':'<<b.rxSignal<<':'<<b.txSignal<<':'<<b.rebuild<<':'<<b.invalid<<':'<<b.split<<':'<<b.precompute<<':'<<l.nrx<<':'<<l.ntx<<':'<<QSettings::values.value("rxFrequency",-1)<<':'<<QSettings::values.value("txFrequency",-1)<<':'<<QSettings::values.value("TXFrequency",-1)<<':'<<(b.m_localRxFrequencyGuardUntilMs>QDateTime::currentMSecsSinceEpoch())<<':'<<(b.m_localTxFrequencyGuardUntilMs>QDateTime::currentMSecsSinceEpoch())<<'\n';}
int main(){Legacy l;DecodiumBridge b;b.m_legacyBackend=&l;b.setRxFrequency(-1);b.setTxFrequency(9000);show(b,l);b.setRxFrequency(0);b.setTxFrequency(5000);show(b,l);b.legacyOwner=true;b.available=false;b.setRxFrequency(1200);b.setTxFrequency(1300);show(b,l);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-audiofreq-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1],outputs
assert outputs[0].splitlines()==['0:5000:0:5000:1:1:1:1:1:1:1:1:0:5000:5000:1:1','0:5000:0:5000:1:1:1:1:1:1:1:1:0:5000:5000:1:1','1200:1300:1200:1300:2:2:1:2:2:2:1:1:1200:1300:1300:1:1'],outputs
print('Audio frequency A/B: bounds, unchanged values, guard timers, persistence, legacy and cache hooks passed')

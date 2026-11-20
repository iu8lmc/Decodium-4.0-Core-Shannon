#!/usr/bin/env python3
"""Compare split CAT sync with controlled rig, legacy and satellite states."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/catsplitsync_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeCatSplitSync.cpp').read_text();mark='void DecodiumBridge::syncActiveCatTxSplitFrequency(';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <iostream>
QStringList events;void bridgeLog(const QString&s){events<<"log:"+s;}
struct Doppler{bool enabledFlag=false,appliedFlag=false;bool enabled(){return enabledFlag;}bool applied(){return appliedFlag;}};
bool owner=false,fallback=false,network=false;bool legacyOwnsRigControl(void*){return owner;}bool useLegacyRigControlFallback(void*,const QString&){return fallback;}bool isHamlibFamilyBackend(const QString&s){return s=="hamlib"||s=="tci";}bool isNetworkRigctlCat(const QString&,QObject*){return network;}QString normalizedCatSplitMode(const QString&s){return s.toLower();}
void activeCatSetTxFreq(QObject*,void*,const QString&,double f,void*){events<<QString("set:%1").arg(f,0,'f',0);}
class DecodiumBridge:public QObject{public:Doppler doppler,*m_q65Doppler=&doppler;QObject cat;void*m_legacyBackend=nullptr,*m_hamlibCat=nullptr,*m_omniRigCat=nullptr;QString m_catBackend="hamlib";bool satellite=false,m_networkRigctlStartupReadOnly=false,settle=false;double dial=0,m_frequency=14074000;int m_txFrequency=1500;DecodiumBridge(){cat.setProperty("connected",true);cat.setProperty("splitMode","rig");cat.setProperty("split",false);cat.setProperty("txFrequency",0);}
 void refreshQ65Doppler(){events<<"doppler";}bool ft2LinkSatelliteHalfDuplexOperationActive(){return satellite;}QObject*catManagerObj(){return &cat;}bool hamlibCatFrequencySettleActive(const QString&){return settle;}double catSplitTxDialFrequencyHz(){return dial;}double applyFrequencyCalibration(double f){return f+100;}int catSplitXitHzForTxFrequency(int){return 500;}int effectiveTxAudioFrequencyHz(){return 1000;}void syncActiveCatTxSplitFrequency(const QString&);};
#include "methods.inc"
void show(const char*n){std::cout<<n<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge b;b.doppler.enabledFlag=b.doppler.appliedFlag=true;b.syncActiveCatTxSplitFrequency("test");show("doppler");b.doppler.enabledFlag=b.doppler.appliedFlag=false;b.satellite=true;b.syncActiveCatTxSplitFrequency("test");show("satellite");b.satellite=false;owner=true;b.syncActiveCatTxSplitFrequency("test");show("legacy");owner=false;b.m_networkRigctlStartupReadOnly=network=true;b.syncActiveCatTxSplitFrequency("test");show("network");b.m_networkRigctlStartupReadOnly=network=false;b.settle=true;b.syncActiveCatTxSplitFrequency("test");show("settle");b.settle=false;b.cat.setProperty("splitMode","none");b.syncActiveCatTxSplitFrequency("test");show("none");b.cat.setProperty("splitMode","rig");b.syncActiveCatTxSplitFrequency("test");show("no-split");b.cat.setProperty("split",true);b.syncActiveCatTxSplitFrequency("test");show("disable");b.dial=14075000;b.syncActiveCatTxSplitFrequency("test");show("enable");}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-catsplitsync-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==9
assert lines[0]=='doppler|doppler' and lines[1]=='satellite|' and lines[2]=='legacy|'
assert 'skipped idle' in lines[3] and 'delayed during QSY' in lines[4] and lines[5]=='none|' and lines[6]=='no-split|'
assert 'set:0' in lines[7] and 'set:14075100' in lines[8]
print('CAT split A/B: Doppler, satellite, legacy, network/QSY guards and zero sentinel passed')

#!/usr/bin/env python3
"""Compare configured CAT mode selection with simulated backends."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/catrigmode_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeCatRigMode.cpp').read_text();mark='void DecodiumBridge::applyConfiguredCatRigMode(';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QString>
#include <QStringList>
#include <iostream>
struct FakeDateTime{static qint64 currentMSecsSinceEpoch(){return 1000;}};
#define QDateTime FakeDateTime
struct FakeTimer{template<class T,class F>static void singleShot(int,T*,F f){f();}};
#define QTimer FakeTimer
QStringList events;void bridgeLog(const QString&s){events<<"log:"+s;}
struct Cat{bool live=true;bool connected(){return live;}void setRigMode(const QString&s){events<<"rig:"+s;}};
struct Remote{bool live=true;bool isLinked(){return live;}void setModeName(const QString&s){events<<"remote:"+s;}} remote,*g_decoPortCatLink=&remote;
bool network=false;bool isNetworkRigctlCat(const QString&,Cat*){return network;}bool decoPortIsCat(const QString&s){return s=="decoport";}bool isHamlibFamilyBackend(const QString&s){return s=="hamlib"||s=="tci";}
struct Rtty{QString restore;QString restoreTarget(int,const QString&,qint64){return restore;}};
class DecodiumBridge{public:QString m_mode="FT8",m_ssbRadioMode="USB",m_catMode,m_catBackend="hamlib",configured="DATA-U";bool m_transmitting=false,m_tuning=false,m_decoPortRemoteKeyed=false,m_networkRigctlStartupReadOnly=false,settle=false,packet=false;Rtty m_rttyRigModeState;Cat hamlib,cat4,omni,*m_hamlibCat=&hamlib,*m_cat4OmCat=&cat4,*m_omniRigCat=&omni;
 QString configuredCatRigMode(){return configured;}int rttyCatContext(){return 0;}bool hamlibCatFrequencySettleActive(const QString&){return settle;}bool configuredCatRigModeRequestsDataPacket(){return packet;}void applyConfiguredCatRigMode(const QString&,const QString&);};
#include "methods.inc"
void show(const char*n){std::cout<<n<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge b;b.applyConfiguredCatRigMode("test","DIGU");show("normalise");b.applyConfiguredCatRigMode("test","DIGL");show("lower");b.m_catMode="data-u";b.applyConfiguredCatRigMode("test","DATA-U");show("same");b.m_catMode="";b.m_networkRigctlStartupReadOnly=network=true;b.applyConfiguredCatRigMode("test","USB");show("network");b.m_networkRigctlStartupReadOnly=network=false;b.settle=true;b.applyConfiguredCatRigMode("test","USB");show("settle");b.settle=false;b.m_catBackend="decoport";b.applyConfiguredCatRigMode("test","LSB");show("remote");b.m_catBackend="cat4om";b.packet=true;b.m_transmitting=true;b.applyConfiguredCatRigMode("startTx","USB");show("cat4");b.m_catBackend="omnirig";b.applyConfiguredCatRigMode("startTx","USB");show("omni");b.m_catBackend="hamlib";b.configured="";b.m_rttyRigModeState.restore="USB";b.applyConfiguredCatRigMode("test","");show("restore-deferred");b.m_transmitting=false;b.applyConfiguredCatRigMode("test","");show("restore");}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-catrigmode-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==10
assert 'rig:DATA-U' in lines[0] and 'rig:DATA-L' in lines[1] and lines[2]=='same|'
assert 'deferred mode write' in lines[3] and 'delayed during QSY' in lines[4] and 'remote:DIGL' in lines[5]
assert 'rig:USB' in lines[6] and 'rig:DATA-U' in lines[6] and 'rig:USB' in lines[7] and 'rig:DATA-U' in lines[7]
assert 'restore deferred' in lines[8] and 'rig:USB' in lines[9]
print('CAT mode A/B: aliases, network/QSY guards, remote, post-PTT verification and RTTY restore passed')

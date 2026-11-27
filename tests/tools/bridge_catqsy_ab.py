#!/usr/bin/env python3
"""Compare CAT QSY and delayed settle using a simulated radio and timer."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/catqsy_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeCatQsy.cpp').read_text();mark='void DecodiumBridge::requestRigFrequencyFromBridge(';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QString>
#include <QStringList>
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>
#include <iostream>
#define emit
struct FakeDateTime{static qint64 currentMSecsSinceEpoch(){return 1000;}};
#define QDateTime FakeDateTime
std::vector<std::function<void()>> callbacks;struct FakeTimer{template<class T,class F>static void singleShot(int,T*,F f){callbacks.push_back(f);}};
#define QTimer FakeTimer
QStringList events;void bridgeLog(const QString&s){events<<"log:"+s;}bool isHamlibFamilyBackend(const QString&s){return s=="hamlib"||s=="tci";}
struct Doppler{void setEnabled(bool){events<<"doppler-off";}};struct Cat{bool live=true;double f=100;bool connected(){return live;}double frequency(){return f;}};
class DecodiumBridge;void activeCatSetFreq(DecodiumBridge*,Cat*,const QString&,double f,void*,void*){events<<QString("cat:%1").arg(f,0,'f',0);}
class DecodiumBridge{public:Doppler doppler,*m_q65Doppler=&doppler;Cat cat,*m_hamlibCat=&cat;void*m_omniRigCat=nullptr,*m_legacyBackend=nullptr;bool m_catConnected=false;QString m_catBackend="hamlib",m_mode="FT8";double m_frequency=100,m_localCatFrequencyPreviousHz=0,m_localCatFrequencyTargetHz=0,offset=0;long long m_localCatFrequencyGuardUntilMs=0,m_localCatFrequencyGuardMaxMs=0,m_catFrequencySettleUntilMs=0;quint64 m_catQsySettleSerial=0;
 double applyFrequencyCalibration(double f){return f+offset;}double logicalFrequencyFromCatDial(double f){return f-offset;}void setFrequency(double f){m_frequency=f;events<<QString("frequency:%1").arg(f,0,'f',0);}void errorMessage(const QString&s){events<<"error:"+s;}void applyRttyRigMode(const QString&){events<<"rtty";}void applyConfiguredCatRigMode(const QString&){events<<"mode";}void syncActiveCatTxSplitFrequency(const QString&){events<<"split";}
 void requestRigFrequencyFromBridge(double,const QString&);void schedulePostQsyCatSettledSync(double,const QString&,int=500,int=0);bool hamlibCatFrequencySettleActive(const QString&)const;};
#include "methods.inc"
void show(const char*n,DecodiumBridge&b){std::cout<<n<<':'<<b.m_frequency<<':'<<b.m_catQsySettleSerial<<':'<<b.m_catFrequencySettleUntilMs<<':'<<callbacks.size()<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
void fire(){auto f=callbacks.front();callbacks.erase(callbacks.begin());f();}
int main(){DecodiumBridge b;b.requestRigFrequencyFromBridge(0,"test");show("invalid",b);b.requestRigFrequencyFromBridge(200,"test");show("local",b);b.m_catConnected=true;b.offset=-300;b.requestRigFrequencyFromBridge(250,"test");show("bad-offset",b);b.offset=0;b.requestRigFrequencyFromBridge(300,"test");show("hamlib",b);fire();show("retry",b);b.cat.f=300;fire();show("settled",b);b.schedulePostQsyCatSettledSync(300,"old");b.schedulePostQsyCatSettledSync(300,"new");fire();show("stale",b);fire();show("latest",b);b.m_catBackend="other";b.m_mode="RTTY";b.requestRigFrequencyFromBridge(400,"test");show("rtty",b);fire();show("rtty-later",b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-catqsy-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==10
assert lines[0].endswith('|') and 'frequency:200' in lines[1] and 'error:Cannot tune' in lines[2] and 'frequency:250' not in lines[2]
assert 'cat:300' in lines[3] and 'retry 1/2' in lines[4] and 'cat:300' in lines[4]
assert 'mode;split' in lines[5] and lines[6].endswith('|') and 'mode;split' in lines[7]
assert 'rtty;cat:400;split' in lines[8] and lines[9].endswith('|rtty')
print('CAT QSY A/B: invalid offsets, local path, retries, stale callbacks, settled sync and RTTY passed')

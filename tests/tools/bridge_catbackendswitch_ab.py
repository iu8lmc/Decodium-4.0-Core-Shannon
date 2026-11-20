#!/usr/bin/env python3
"""Compare CAT backend switching and SSTV safety guards."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/catbackendswitch_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeCatBackendSwitch.cpp').read_text();mark='void DecodiumBridge::setCatBackend(';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeEmbeddedLegacyRigEnabled(', 'embeddedLegacyRigControlEnabledForBackend(');assert old==new
source=r'''
#include <QCoreApplication>
#include <QEventLoop>
#include <QVariant>
#include <iostream>
struct FakeSettings{enum Format{IniFormat};enum Scope{UserScope};static QString saved;FakeSettings(Format,Scope,const char*,const char*){}void setValue(const char*,const QString&v){saved=v;}};QString FakeSettings::saved;namespace decodium{void beginActiveSettingsProfile(FakeSettings&){}}
bool embeddedLegacyRigControlEnabledForBackend(const QString&b,bool requested){return b=="hamlib"&&!requested;}
struct Cat{int disconnects=0;void disconnectRig(){disconnects++;}};struct Doppler{bool enabled=true;void setEnabled(bool v){enabled=v;}};struct Legacy{bool enabled=false;void setRigControlEnabled(bool v){enabled=v;}};struct Rtty{int clears=0;void clear(){clears++;}};
class DecodiumBridge:public QObject{public:QString m_catBackend="hamlib",m_lastCatError="error";bool m_catConnected=true,sstv=false,legacyAvailable=true,legacyTx=false;Cat*m_cat4OmCat=nullptr,*m_omniRigCat=nullptr,*m_hamlibCat=nullptr;Doppler*m_q65Doppler=nullptr;Legacy*m_legacyBackend=nullptr;Rtty m_rttyRigModeState;int cancels=0,errors=0,halts=0,signalsCount=0,refresh=0,telemetry=0;bool sstvTxActive(){return sstv;}void cancelSstvTx(){cancels++;}void errorMessage(const QString&){errors++;}void halt(){halts++;}void catConnectedChanged(){signalsCount++;}void catBackendChanged(){signalsCount++;}void catManagerChanged(){signalsCount++;}void lastCatErrorChanged(){signalsCount++;}void refreshPskReporterLocalStation(){refresh++;}void updateRigTelemetry(double,double){telemetry++;}bool legacyBackendAvailable(){return legacyAvailable;}bool legacyTxBackendRequested(){return legacyTx;}void setCatBackend(const QString&);};
#define QSettings FakeSettings
#include "methods.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(int k=0;k<4;k++){DecodiumBridge b;Cat a,c,d;Doppler q;Legacy l;b.m_cat4OmCat=&a;b.m_omniRigCat=&c;b.m_hamlibCat=&d;b.m_q65Doppler=&q;b.m_legacyBackend=&l;if(k==1)b.sstv=true;QString input=k==0?" HAMLIB ":(k==3?"bad":" TCI ");if(k==3)b.m_catBackend="omnirig";b.setCatBackend(input);std::cout<<b.m_catBackend.toStdString()<<':'<<b.cancels<<':'<<b.errors<<':'<<b.halts<<':'<<a.disconnects<<':'<<c.disconnects<<':'<<d.disconnects<<':'<<b.m_catConnected<<':'<<b.signalsCount<<':'<<b.refresh<<':'<<b.telemetry<<':'<<b.m_rttyRigModeState.clears<<':'<<l.enabled<<':'<<q.enabled<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-catswitch-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
expected='hamlib:0:0:0:0:0:0:1:0:0:0:0:0:1\nhamlib:1:1:0:0:0:0:1:0:0:0:0:0:0\ntci:0:0:1:1:1:1:0:4:2:1:1:0:0\nhamlib:0:0:1:1:1:1:0:4:2:1:1:1:0\n';assert outputs[0]==outputs[1]==expected,outputs
print('CAT backend switch A/B: normalization, SSTV guard, disconnect and legacy routing passed')

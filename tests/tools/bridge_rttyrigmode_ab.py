#!/usr/bin/env python3
"""Compare RTTY CAT mode routing, context and deferral."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/rttyrigmode_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeRttyRigMode.cpp').read_text();mark='QString DecodiumBridge::rttyCatContext() const';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeRttyRigModeLog(', 'bridgeLog(').replace('decodiumBridgeIsHamlibFamilyBackend(', 'isHamlibFamilyBackend(').replace('decodiumBridgeDecoPortIsCat(', 'decoPortIsCat(');assert old==new
source=r'''
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <iostream>
void bridgeLog(const QString&){}bool decoPortIsCat(const QString&s){return s=="decoport";}bool isHamlibFamilyBackend(const QString&s){return s=="hamlib"||s=="tci";}
struct Cat{bool con=true;int calls=0;QString value;bool connected(){return con;}void setRigMode(const QString&v){value=v;calls++;}};struct ModeState{int calls=0;QString ctx;void overrideRequested(const QString&c,const QString&){ctx=c;calls++;}};
class DecodiumBridge:public QObject{public:bool m_catConnected=true,m_decoPortUseRemote=false,settle=false;QString m_catBackend="hamlib",m_catRigName="rig",m_mode="RTTY",m_catMode,desired="DATA-U";Cat*m_cat4OmCat=nullptr,*m_hamlibCat=nullptr,*m_omniRigCat=nullptr;ModeState m_rttyRigModeState;QString rttyRigMode(){return desired;}bool hamlibCatFrequencySettleActive(const QString&){return settle;}QString rttyCatContext()const;void applyRttyRigMode(const QString&);};
#include "methods.inc"
void wait(){QEventLoop l;QTimer::singleShot(700,&l,&QEventLoop::quit);l.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;Cat h,c,o;b.m_hamlibCat=&h;b.m_cat4OmCat=&c;b.m_omniRigCat=&o;std::cout<<b.rttyCatContext().toStdString()<<':';b.m_decoPortUseRemote=true;std::cout<<b.rttyCatContext().isEmpty()<<'\n';b.m_decoPortUseRemote=false;b.applyRttyRigMode("test");std::cout<<h.calls<<':'<<h.value.toStdString()<<':'<<b.m_rttyRigModeState.ctx.toStdString()<<'\n';b.m_catMode="data-u";b.applyRttyRigMode("test");std::cout<<h.calls<<'\n';b.m_catMode.clear();b.m_catBackend="cat4om";b.applyRttyRigMode("test");b.m_catBackend="omnirig";b.applyRttyRigMode("test");std::cout<<c.calls<<':'<<o.calls<<'\n';b.m_mode="FT8";b.applyRttyRigMode("test");std::cout<<o.calls<<'\n';b.m_mode="RTTY";b.m_catBackend="hamlib";b.settle=true;b.applyRttyRigMode("test");b.settle=false;wait();std::cout<<h.calls<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-rttymode-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='hamlib|rig:1\n1:DATA-U:hamlib|rig\n1\n1:1\n1\n2\n',outputs
print('RTTY rig mode A/B: context, CAT routing, no-op and QSY deferral passed')

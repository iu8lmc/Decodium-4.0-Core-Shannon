#!/usr/bin/env python3
"""Compare safe CAT recovery and backend selection."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/catrecovery_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeCatRecovery.cpp').read_text();mark='bool DecodiumBridge::resetCatConnection()';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QString>
#include <QStringList>
#include <iostream>
#define emit
QStringList events;void bridgeLog(const QString&s){events<<"log:"+s;}
struct FakeEvents{enum{AllEvents=0};};struct FakeApp{static void processEvents(int,int){events<<"events";}};
#define QEventLoop FakeEvents
#define QCoreApplication FakeApp
struct Cat{QString name;void disconnectRig(){events<<"disconnect:"+name;}void connectRig(){events<<"connect:"+name;}};
struct Legacy{void retryRigConnection(){events<<"legacy-retry";}};bool owner=false,fallback=false;bool legacyOwnsRigControl(Legacy*){return owner;}bool useLegacyRigControlFallback(Legacy*,const QString&){return fallback;}bool isHamlibFamilyBackend(const QString&s){return s=="hamlib"||s=="tci";}
class DecodiumBridge{public:bool m_transmitting=false,m_tuning=false;QString m_catBackend="hamlib";Cat c{"cat4"},o{"omni"},h{"hamlib"},*m_cat4OmCat=&c,*m_omniRigCat=&o,*m_hamlibCat=&h;Legacy l,*m_legacyBackend=&l;void statusMessage(const QString&s){events<<"status:"+s;}bool resetCatConnection();void retryRigConnection();};
#include "methods.inc"
void show(const char*n){std::cout<<n<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge b;b.m_transmitting=true;std::cout<<"tx-result:"<<b.resetCatConnection()<<'\n';show("tx");b.m_transmitting=false;b.m_tuning=true;std::cout<<"tune-result:"<<b.resetCatConnection()<<'\n';show("tune");b.m_tuning=false;owner=true;b.retryRigConnection();show("owner");owner=false;fallback=true;b.retryRigConnection();show("fallback");fallback=false;for(auto name:{"hamlib","tci","cat4om","omnirig"}){b.m_catBackend=name;b.retryRigConnection();show(name);}b.m_catBackend="hamlib";std::cout<<"reset-result:"<<b.resetCatConnection()<<'\n';show("reset");}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-catrecovery-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==12
assert lines[0]=='tx-result:0' and 'postponed' in lines[1] and lines[2]=='tune-result:0' and 'postponed' in lines[3]
assert lines[4]=='owner|legacy-retry' and lines[5]=='fallback|legacy-retry'
assert lines[6]=='hamlib|disconnect:cat4;disconnect:omni;events;connect:hamlib' and lines[7]=='tci|disconnect:cat4;disconnect:omni;events;connect:hamlib'
assert lines[8]=='cat4om|disconnect:omni;disconnect:hamlib;events;connect:cat4' and lines[9]=='omnirig|disconnect:cat4;disconnect:hamlib;events;connect:omni'
assert lines[10]=='reset-result:1' and 'disconnect:cat4;disconnect:omni;disconnect:hamlib;events;' in lines[11] and 'connect:hamlib' in lines[11]
print('CAT recovery A/B: TX/tune guards, legacy ownership and backend release/reconnect passed')

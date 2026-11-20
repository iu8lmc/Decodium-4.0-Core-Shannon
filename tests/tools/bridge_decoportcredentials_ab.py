#!/usr/bin/env python3
"""Compare DecoPort key migration, storage, propagation and clearing."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/decoportcredentials_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeDecoPortCredentials.cpp').read_text();mark='QByteArray DecodiumBridge::decoPortAuthKey() const';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeDecoPortCredentialsLog(', 'bridgeLog(');assert old==new
source=r'''
#include <QString>
#include <QByteArray>
#include <QVariant>
#include <QMap>
#include <iostream>
void bridgeLog(const QString&){}
struct FakeSettings{enum Format{IniFormat};enum Scope{UserScope};static QMap<QString,QVariant>values;int depth=0;FakeSettings(Format,Scope,const QString&,const QString&){}void setValue(const QString&k,const QVariant&v){values[(depth?"profile/":"")+k]=v;}void remove(const QString&k){values.remove((depth?"profile/":"")+k);}void endGroup(){if(depth)depth--;}void sync(){}};QMap<QString,QVariant>FakeSettings::values;namespace decodium{bool profile=true;bool beginActiveSettingsProfile(FakeSettings&s){if(profile)s.depth=2;return profile;}QVariant profiledSettingsValue(const QString&,const QString&k,const QVariant&d){return FakeSettings::values.value((profile?"profile/":"")+k,d);}}
namespace decoport{QByteArray deriveKeyFromPassword(const QString&p){return p.isEmpty()?QByteArray():QByteArray("key:")+p.toUtf8();}}
struct Endpoint{QByteArray key;int stops=0;void setAuthKey(const QByteArray&v){key=v;}void stop(){stops++;}};
class DecodiumBridge{public:Endpoint*m_decoPortGateway=nullptr,*m_decoPortLink=nullptr,*m_decoPortDiscovery=nullptr;QByteArray decoPortAuthKey()const;bool hasDecoPortPassword()const;bool setDecoPortPassword(const QString&);void clearDecoPortPassword();};
#define QSettings FakeSettings
#include "methods.inc"
int main(){DecodiumBridge b;Endpoint g,l,d;b.m_decoPortGateway=&g;b.m_decoPortLink=&l;b.m_decoPortDiscovery=&d;FakeSettings::values["DecoPortPasswordPending"]="root";FakeSettings::values["profile/DecoPortPasswordPending"]="synthetic";std::cout<<b.decoPortAuthKey().toStdString()<<':'<<b.hasDecoPortPassword()<<':'<<FakeSettings::values.contains("DecoPortPasswordPending")<<':'<<FakeSettings::values.contains("profile/DecoPortPasswordPending")<<':'<<(g.key==l.key&&l.key==d.key)<<'\n';std::cout<<b.setDecoPortPassword("")<<':'<<b.hasDecoPortPassword()<<'\n';b.clearDecoPortPassword();std::cout<<b.hasDecoPortPassword()<<':'<<g.stops<<':'<<g.key.isEmpty()<<':'<<l.key.isEmpty()<<':'<<d.key.isEmpty()<<'\n';decodium::profile=false;b.setDecoPortPassword("other");std::cout<<b.decoPortAuthKey().toStdString()<<'\n';b.clearDecoPortPassword();std::cout<<b.hasDecoPortPassword()<<':'<<g.stops<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-decoportcredentials-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='key:synthetic:1:0:0:1\n0:1\n0:1:1:1:1\nkey:other\n0:2\n',outputs
print('DecoPort credentials A/B: pending migration, empty rejection, profile, propagation and clearing passed')

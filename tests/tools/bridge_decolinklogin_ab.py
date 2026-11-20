#!/usr/bin/env python3
"""Compare DecoLink credential/profile handling with synthetic stores and link."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/decolinklogin_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeDecolinkLogin.cpp').read_text();mark='static QString decolinkSecretService()';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QMap>
#include <QJsonDocument>
#include <iostream>
#define emit
QStringList events;void bridgeLog(const QString&s){events<<"log:"+s;}
struct FakeSettings{enum Format{IniFormat};enum Scope{UserScope};static QMap<QString,QVariant> values;FakeSettings(Format,Scope,const QString&,const QString&){}void setValue(const QString&k,const QVariant&v){values[k]=v;}void endGroup(){}void sync(){}};QMap<QString,QVariant> FakeSettings::values;
#define QSettings FakeSettings
namespace decodium{bool beginActiveSettingsProfile(FakeSettings&){return true;}QVariant profiledSettingsValue(const QString&,const QString&k,const QVariant&d){return FakeSettings::values.value(k,d);}}
namespace secure_settings{struct Lookup{bool found=false;QString value;};struct Backend{QString stored;bool present=false;Lookup lookup(const QString&,const QString&){return {present,stored};}bool store(const QString&,const QString&,const QString&p,QString*){stored=p;present=true;events<<"store";return true;}void remove(const QString&,const QString&){present=false;stored.clear();events<<"remove";}};Backend backend;Backend& default_backend(){return backend;}}
struct Link{int profile=99;void setAudioProfile(int p){profile=p;events<<QString("profile:%1").arg(p);}void connectTo(const QString&a,const QString&r,int p,const QString&e,const QString&pw,const QString&station){events<<QString("connect:%1:%2:%3:%4:%5:%6").arg(a,r).arg(p).arg(e,pw,station);}};
class DecodiumBridge{public:Link link,*m_decolinkLink=&link;static QString tr(const char*s){return QString::fromUtf8(s);}void errorMessage(const QString&s){events<<"error:"+s;}void decolinkLinkObject(){events<<"link";}QVariantMap decolinkSavedLogin()const;void decolinkConnect(const QString&,const QString&,int,const QString&,const QString&,const QString&,bool);void setDecolinkAudioProfile(int);void decolinkForgetPassword();};
#include "methods.inc"
void show(const char*n,DecodiumBridge&b){std::cout<<n<<'|'<<QJsonDocument::fromVariant(b.decolinkSavedLogin()).toJson(QJsonDocument::Compact).toStdString()<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge b;show("default",b);b.decolinkConnect(" host "," relay ",5555," ","", " station ",true);show("invalid",b);b.setDecolinkAudioProfile(99);show("invalid-profile",b);b.setDecolinkAudioProfile(3);show("profile",b);b.decolinkConnect(" host "," relay ",5555," synthetic@test ","pw", " station ",true);show("remember",b);b.decolinkConnect(" host "," relay ",5555,"synthetic@test","", " station ",true);show("reuse",b);b.decolinkConnect(" host "," relay ",5555,"synthetic@test","pw", " station ",false);show("forget",b);b.decolinkForgetPassword();show("remove",b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-decolinklogin-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==8
assert '"authHost":"decolink.ft2.it"' in lines[0] and 'enter e-mail and password' in lines[1]
assert 'profile:-1' in lines[2] and 'profile:3' in lines[3]
assert '"hasPassword":true' in lines[4] and '|store;link;profile:3;connect:host:relay:5555:synthetic@test:pw:station' in lines[4]
assert 'connect:host:relay:5555:synthetic@test:pw:station' in lines[5] and '"hasPassword":false' in lines[6] and 'remove' in lines[7]
print('DecoLink A/B: defaults, missing credentials, profile bounds, password reuse and removal passed')

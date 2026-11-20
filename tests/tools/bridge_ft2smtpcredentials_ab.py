#!/usr/bin/env python3
"""A/B FT2 SMTP credential handling with a synthetic secure backend."""
import os, shlex, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/ft2smtpcredentials_before.inc').read_text()
s=(root/'src/bridge/DecodiumBridgeFt2SmtpCredentials.cpp').read_text();start='QVariantMap DecodiumBridge::ft2LinkEmailGatewayPasswordStatus(';new=start+s.split(start,1)[1].split('#undef ft2LinkSmtpSecretKey',1)[0];assert old==new
main=(root/'src/bridge/DecodiumBridge.cpp').read_text();helpers=main[main.index('static QString ft2LinkSmtpConfigString('):main.index('QString decodiumBridgeFt2SmtpSecretKey(')]
source=r'''
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QRegularExpression>
#include <QJsonDocument>
#include <iostream>
#define emit
QStringList events;
namespace secure_settings {
struct Lookup{bool found;QString error,value;};
struct Backend {bool isAvailable=true, fail=false, present=false;QString value;
 bool available()const{return isAvailable;} Lookup lookup(const QString&,const QString&)const{return {present,fail?QStringLiteral("lookup-error"):QString(),value};}
 bool store(const QString&,const QString&,const QString&v,QString*e)const{events<<"store:"+v;if(fail)*e="store-error";return !fail;}
 bool remove(const QString&,const QString&,QString*e)const{events<<"remove";if(fail)*e="remove-error";return !fail;}};
Backend backend;Backend& default_backend(){return backend;} QString service(const QString&s){return "service:"+s;}}
class DecodiumBridge {public:QString m_callsign=" iu8lmc ";void statusMessage(const QString&s){events<<"status:"+s;}void errorMessage(const QString&s){events<<"error:"+s;}
 QVariantMap sendFt2LinkGatewayEmail(quint32 id,const QVariantMap&c,const QString&e){QVariantMap x=c;x["id"]=id;x["eml"]=e;return x;}
 QVariantMap ft2LinkEmailGatewayPasswordStatus(const QVariantMap&)const; QVariantMap setFt2LinkEmailGatewayPassword(const QVariantMap&,const QString&);QVariantMap clearFt2LinkEmailGatewayPassword(const QVariantMap&);QVariantMap testFt2LinkEmailGateway(const QVariantMap&);};
#include "helpers.inc"
#include "methods.inc"
void show(const QVariantMap&v){std::cout<<QJsonDocument::fromVariant(v).toJson(QJsonDocument::Compact).toStdString()<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge b;QVariantMap c{{"host"," SMTP/EXAMPLE "},{"username"," USER@EXAMPLE "}};show(b.ft2LinkEmailGatewayPasswordStatus(c));show(b.setFt2LinkEmailGatewayPassword(c,""));secure_settings::backend.isAvailable=false;show(b.setFt2LinkEmailGatewayPassword(c,"synthetic"));show(b.clearFt2LinkEmailGatewayPassword(c));secure_settings::backend.isAvailable=true;secure_settings::backend.fail=true;show(b.setFt2LinkEmailGatewayPassword(c,"synthetic"));show(b.clearFt2LinkEmailGatewayPassword(c));secure_settings::backend.fail=false;show(b.setFt2LinkEmailGatewayPassword(c,"synthetic"));show(b.clearFt2LinkEmailGatewayPassword(c));show(b.testFt2LinkEmailGateway(c));c["toEmail"]="explicit@example";show(b.testFt2LinkEmailGateway(c));}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-smtpcred-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);(p/'helpers.inc').write_text(helpers);results=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);results.append(subprocess.check_output([str(p/'test')],text=True))
assert results[0]==results[1];lines=results[0].splitlines();assert len(lines)==10
assert 'ft2LinkSmtpPassword/smtp_example/user@example' in lines[0]
assert 'Missing SMTP password' in lines[1] and 'unavailable' in lines[2] and 'unavailable' in lines[3]
assert 'store-error' in lines[4] and 'remove-error' in lines[5] and 'saved securely' in lines[6] and 'password cleared' in lines[7]
assert '"testOnly":true' in lines[8] and '"toEmail":"USER@EXAMPLE"' in lines[8] and 'explicit@example' in lines[9]
print('FT2 SMTP credentials A/B: lookup, empty password, unavailable/failing/success backend and test configuration passed')

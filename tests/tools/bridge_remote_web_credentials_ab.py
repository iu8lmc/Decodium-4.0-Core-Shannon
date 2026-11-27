#!/usr/bin/env python3
"""Compare the moved web token resolution against a synthetic secret backend."""
import os, shlex, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=(r/'src/bridge/DecodiumBridge.cpp').read_text()
module=(r/'src/bridge/DecodiumBridgeRemoteWebInit.cpp').read_text()
fixture=(r/'tests/bridge_refactor_fixtures/remote_web_init_before.inc').read_text()
start='                {\n                    QStringList secureProfiles;'
end='                bool const remoteExposed'
old=fixture[fixture.index(start):fixture.index(end)]
new=module[module.index(start):module.index(end)]
assert old==new
helper=main[main.index('static QString resolveSecureSettingFromProfiles('):main.index('\nQString decodiumBridgeResolveSecureSetting',main.index('static QString resolveSecureSettingFromProfiles('))]
harness=r'''
#include <QCoreApplication>
#include <QMap>
#include <QSettings>
#include <QTemporaryDir>
#include <QStringList>
#include <iostream>
namespace secure_settings {
struct Lookup { bool found; QString value; };
QMap<QString,QString> secrets;
struct Backend { Lookup lookup(const QString& service,const QString&) const { return {secrets.contains(service),secrets.value(service)}; } };
Backend backend;
const Backend& default_backend() { return backend; }
QString service(QString s) { s=s.trimmed().toUpper(); return s.isEmpty()?QStringLiteral("DEFAULT"):s; }
bool is_placeholder(const QString& s) { return s==QStringLiteral("__secure__"); }
QString load_or_import(QSettings*, const QString& service, const QString&, const QString& plain) {
 if(secrets.contains(service)) return secrets.value(service);
 return is_placeholder(plain)?QString():plain;
}
}
void bridgeLog(const QString&) {}
#include "helper.inc"
QString decodiumBridgeResolveSecureSetting(QString const& key, QString const& raw, QStringList profiles) { return resolveSecureSettingFromProfiles(key,raw,profiles); }
struct Harness {
 QString m_callsign,legacy,plain,path;
 QVariant readSettingFromLegacyIni(const QString&) const { return legacy; }
 QString legacyIniPath() const { return path+QStringLiteral("/legacy.ini"); }
 QVariant getSetting(const QString&,const QVariant& fallback) const { return plain.isNull()?fallback:QVariant(plain); }
 QString run(QString authToken,QString root) {
  QSettings webSettings(path+QStringLiteral("/web.ini"),QSettings::IniFormat);
  webSettings.clear();webSettings.setValue(QStringLiteral("MyCall"),root);
#include "block.inc"
  return authToken;
 }
};
int main(int argc,char** argv){QCoreApplication app(argc,argv);QTemporaryDir dir;Harness h;h.path=dir.path();h.m_callsign="IU8LMC";h.legacy="9H1SR";
 auto show=[&](const char* name,QString token){std::cout<<name<<'|'<<token.toStdString()<<'\n';};
 secure_settings::secrets["IU8LMC"]=" primary ";show("primary",h.run("raw","ROOT1"));
 secure_settings::secrets.clear();secure_settings::secrets["9H1SR"]=" legacy ";show("legacy",h.run("","ROOT1"));
 secure_settings::secrets.clear();secure_settings::secrets["DEFAULT"]=" default ";show("default",h.run("","ROOT1"));
 secure_settings::secrets.clear();show("plain",h.run(" plain ","ROOT1"));show("placeholder",h.run("__secure__","ROOT1"));
}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-remote-credential-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(harness);(p/'helper.inc').write_text(helper);outputs=[]
 for label,block in [('before',old),('after',new.replace('resolveSecureSettingFromProfiles(', 'decodiumBridgeResolveSecureSetting('))]:
  (p/'block.inc').write_text(block);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
 assert outputs[0]==outputs[1]
 assert outputs[0].splitlines()==['primary|primary','legacy|legacy','default|default','plain|plain','placeholder|'],outputs[0]
 print('Remote token A/B: primary, legacy, default, plaintext and placeholder passed with synthetic backend')

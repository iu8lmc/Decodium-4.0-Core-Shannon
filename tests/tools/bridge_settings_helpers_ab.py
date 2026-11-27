#!/usr/bin/env python3
"""Compare aliases, SMTP keys, and secure profile lookup using a fake backend."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
fixture=(root/'tests/bridge_refactor_fixtures/settings_helpers_before.inc').read_text()
module=(root/'src/bridge/DecodiumBridgeSettingsHelpers.cpp').read_text()
assert module.endswith(fixture)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include "SecureSettings.hpp"
#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>
#include <QVariantMap>
namespace secure_settings {
QStringList lookups;
class Fake final:public Backend {public:bool available()const override{return true;}LookupResult lookup(const QString& svc,const QString& key)const override{lookups<<svc+":"+key;if(svc=="service:SECOND")return {true,true," secret-value ",{}};return {true,false,{},{}};}bool store(const QString&,const QString&,const QString&,QString*)const override{return false;}bool remove(const QString&,const QString&,QString*)const override{return false;}};
Backend const& default_backend(){static Fake b;return b;}
QString service(const QString& p){return "service:"+p;}
bool is_placeholder(const QString& s){return s=="<stored>";}
}
#include "helpers.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);bool alias=aliasedBridgeSettingKey("ShowGreyline")=="MapShowGreyline"&&aliasedBridgeSettingKey("MapShowGreyline")=="ShowGreyline"&&aliasedBridgeSettingKey("AlwaysPass12")=="Pass12"&&aliasedBridgeSettingKey("AlwaysPass13")=="AlwaysPass13"&&aliasedBridgeSettingKey("HideB4")=="FiltersHideWorkedBand";QVariantMap smtp{{"host"," MAIL.Example:25 "},{"username"," User+Tag@Example "}};bool key=ft2LinkSmtpSecretKey(smtp)=="ft2LinkSmtpPassword/mail.example_25/user+tag@example"&&ft2LinkSmtpSecretKey({})=="ft2LinkSmtpPassword/default-host/default-user";auto value=resolveSecureSettingFromProfiles("password"," old ",{"first","second","first"});bool precedence=value=="secret-value"&&secure_settings::lookups==QStringList({"service:FIRST:password","service:SECOND:password"});secure_settings::lookups.clear();auto fallback=resolveSecureSettingFromProfiles("password","<stored>",{});bool empty=fallback.isEmpty()&&secure_settings::lookups==QStringList({"service:DEFAULT:password"});bool ok=alias&&key&&precedence&&empty;QTextStream(stdout)<<"alias="<<alias<<" smtp="<<key<<" priority="<<precedence<<" placeholder="<<empty<<Qt::endl;return ok?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-settings-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outs=[]
 for label,body in [('before',fixture),('after',module[-len(fixture):])]:
  (d/'helpers.inc').write_text(body);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(d),'-I'+str(root/'src/security'),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outs.append(result.stdout)
 assert outs[0]==outs[1]

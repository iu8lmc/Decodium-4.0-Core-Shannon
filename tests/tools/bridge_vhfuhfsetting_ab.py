#!/usr/bin/env python3
"""Compare VHF/UHF settings, legacy sync and notifications."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/vhfuhfsetting_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeVhfUhfSetting.cpp').read_text();mark='void DecodiumBridge::setVhfUhfFeatures(';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QString>
#include <QVariant>
#include <QMap>
#include <iostream>
struct FakeSettings{enum Format{IniFormat};enum Scope{UserScope};static QMap<QString,QVariant>values;FakeSettings(Format,Scope,const QString&,const QString&){}void setValue(const QString&k,const QVariant&v){values[k]=v;}void sync(){}};QMap<QString,QVariant>FakeSettings::values;namespace decodium{void beginActiveSettingsProfile(FakeSettings&){}}
class DecodiumBridge{public:bool m_vhfUhfFeatures=false;int legacy=0,notify=0,settingsNotify=0;void syncSettingToLegacyIni(const QString&,const QVariant&){legacy++;}void vhfUhfFeaturesChanged(){notify++;}void settingValueChanged(const QString&,const QVariant&){settingsNotify++;}void setVhfUhfFeatures(bool);};
#define QSettings FakeSettings
#include "methods.inc"
int main(){DecodiumBridge b;b.setVhfUhfFeatures(false);std::cout<<b.legacy<<':'<<b.notify<<':'<<b.settingsNotify<<'\n';b.setVhfUhfFeatures(true);std::cout<<b.m_vhfUhfFeatures<<':'<<b.legacy<<':'<<b.notify<<':'<<b.settingsNotify<<':'<<FakeSettings::values["vhfUhfFeatures"].toBool()<<':'<<FakeSettings::values["VHFUHF"].toBool()<<'\n';b.setVhfUhfFeatures(true);b.setVhfUhfFeatures(false);std::cout<<b.m_vhfUhfFeatures<<':'<<b.legacy<<':'<<b.notify<<':'<<b.settingsNotify<<':'<<FakeSettings::values["VHFUHF"].toBool()<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-vhfuhf-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='0:0:0\n1:2:1:2:1:1\n0:4:2:4:0\n',outputs
print('VHF/UHF setting A/B: persistence, legacy sync, deduplication and notifications passed')

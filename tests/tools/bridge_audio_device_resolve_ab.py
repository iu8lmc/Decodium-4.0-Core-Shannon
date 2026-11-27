#!/usr/bin/env python3
"""Compare audio device matching and settings writes with synthetic endpoints."""
from pathlib import Path
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
fixture = (root/'tests/bridge_refactor_fixtures/audio_device_resolve_before.inc').read_text()
module = (root/'src/bridge/DecodiumBridgeAudioDeviceResolve.cpp').read_text()
assert module.endswith(fixture)
flags = shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'], text=True))
harness = r'''
#include <QCoreApplication>
#include <QList>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#include "DecodiumProfileSettings.h"
class FakeAudioDevice {public: QString name,id; QString description()const{return name;}};
QString decodiumBridgeAudioDeviceIdForSettings(const FakeAudioDevice& d){return d.id;}
bool decodiumBridgeAudioDeviceIdMatches(const FakeAudioDevice& d,const QString& id){return !d.id.isEmpty()&&!id.trimmed().isEmpty()&&d.id.compare(id.trimmed(),Qt::CaseInsensitive)==0;}
QString decodiumBridgeAudioDeviceIdSettingForLog(QString s){s=s.trimmed();return s.isEmpty()?QStringLiteral("<unset>"):s;}
#include "resolve.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir temp;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());QList<FakeAudioDevice> devices{{"Speakers (3- USB Audio CODEC )","aa"},{"Another","bb"}};FakeAudioDevice result;QString reason;bool byId=tryResolveAudioDeviceByIdentity(devices,"bad"," AA ",&result,&reason)&&result.id=="aa"&&reason=="id";bool exact=tryResolveAudioDeviceByIdentity(devices,"another","",&result,&reason)&&result.id=="bb"&&reason=="exact-name";bool norm=tryResolveAudioDeviceByIdentity(devices,"Speakers (USB Audio CODEC)","",&result,&reason)&&result.id=="aa"&&reason=="normalized-name";devices.append(FakeAudioDevice{"Speakers (4- USB Audio CODEC)","cc"});bool ambiguous=!tryResolveAudioDeviceByIdentity(devices,"Speakers (USB Audio CODEC)","",&result,&reason)&&reason.startsWith("ambiguous-normalized-name");bool fallback=!tryResolveAudioDeviceByIdentity(devices,"","",&result,&reason)&&reason=="default";bool missing=!tryResolveAudioDeviceByIdentity(devices,"missing","dd",&result,&reason)&&reason=="id-and-name-not-found";persistSingleAudioIdentity(true,"input"," ab ");QSettings settings(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(settings);bool stored=settings.value("audioInputDevice")=="input"&&settings.value("SoundInName")=="input"&&settings.value("audioInputDeviceId")=="ab";persistSingleAudioIdentity(false,"output","");settings.sync();bool output=settings.value("audioOutputDevice")=="output"&&!settings.contains("audioOutputDeviceId");bool saved=audioDeviceSavedIdentityMatches("wrong","aa",devices[0])&&!audioDeviceSavedIdentityMatches("Speakers (3- USB Audio CODEC )","bb",devices[0]);bool named=hasAudioDeviceNamed(devices,"Another")&&!hasAudioDeviceNamed(devices,"");bool log=audioDeviceSelectionForLog("","")=="[<default>] id=<unset>";bool ok=byId&&exact&&norm&&ambiguous&&fallback&&missing&&stored&&output&&saved&&named&&log;QTextStream(stdout)<<"id="<<byId<<" exact="<<exact<<" normalized="<<norm<<" ambiguous="<<ambiguous<<" default="<<fallback<<" missing="<<missing<<" persisted="<<stored<<" output="<<output<<" identity="<<saved<<" named="<<named<<" log="<<log<<Qt::endl;return ok?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-audio-resolve-ab-') as tmp:
    d=Path(tmp); (d/'main.cpp').write_text(harness); outputs=[]
    for label,body in [('before',fixture),('after',module[-len(fixture):])]:
        (d/'resolve.inc').write_text(body.replace('QAudioDevice','FakeAudioDevice'))
        exe=d/label
        subprocess.run(['c++','-std=c++17','-I'+str(d),'-I'+str(root/'src/radio'),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True)
        result=subprocess.run([str(exe)],capture_output=True,text=True,check=True)
        print(label,result.stdout.strip(),flush=True);outputs.append(result.stdout)
    assert outputs[0]==outputs[1]

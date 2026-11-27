#!/usr/bin/env python3
"""Compare second-instance validation, profile setup and launch decisions."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/secondinstance_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeSecondInstance.cpp').read_text();mark='QString DecodiumBridge::launchSecondInstance(';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QObject>
#include <QCoreApplication>
#include <QDebug>
#include <QRegularExpression>
#include <QVariant>
#include <QMap>
#include <iostream>
struct FakeSettings{enum Format{IniFormat};enum Scope{UserScope};static QMap<QString,QVariant>values;QStringList groups;FakeSettings(Format,Scope,const QString&,const QString&){}void beginGroup(const QString&v){groups<<v;}void endGroup(){groups.removeLast();}void setValue(const QString&k,const QVariant&v){values[groups.join('/')+'/'+k]=v;}void sync(){}};QMap<QString,QVariant>FakeSettings::values;
struct FakeProcess{static bool success;static int launches;static bool startDetached(const QString&,const QStringList&){launches++;return success;}};bool FakeProcess::success=true;int FakeProcess::launches=0;
namespace decodium{QString activeSettingsProfileName(){return "current";}int initialized=0;void ensureSettingsProfileInitialized(const QString&){initialized++;}QVariant profiledSettingsValue(const QString&,const QString&key,const QVariant&def){return key=="CatSharePort"?QVariant(99999):def;}}
struct Share{bool live=false;bool listening(){return live;}QString status(){return "not ready";}};
class DecodiumBridge:public QObject{public:Share*m_catShare=nullptr;bool makeLive=true;int configure=0;void configureCatShare(bool,int,bool,bool){configure++;if(m_catShare)m_catShare->live=makeLive;}QString launchSecondInstance(const QString&,bool);};
#define QSettings FakeSettings
#define QProcess FakeProcess
#include "methods.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;std::cout<<!b.launchSecondInstance(" ",false).isEmpty()<<':'<<!b.launchSecondInstance("a/b",false).isEmpty()<<':'<<!b.launchSecondInstance("CURRENT",false).isEmpty()<<':'<<decodium::initialized<<'\n';auto ok=b.launchSecondInstance(" alpha ",false);std::cout<<ok.isEmpty()<<':'<<decodium::initialized<<':'<<FakeProcess::launches<<':'<<FakeSettings::values["MultiSettings/alpha/CatShareEnabled"].toBool()<<':'<<FakeSettings::values["MultiSettings/alpha/Transceiver/serialPort"].toString().isEmpty()<<'\n';Share share;b.m_catShare=&share;b.makeLive=false;std::cout<<!b.launchSecondInstance("beta",true).isEmpty()<<':'<<b.configure<<':'<<FakeProcess::launches<<'\n';b.makeLive=true;auto shared=b.launchSecondInstance("gamma",true);std::cout<<shared.isEmpty()<<':'<<b.configure<<':'<<FakeProcess::launches<<':'<<FakeSettings::values["MultiSettings/gamma/Transceiver/networkPort"].toString().toStdString()<<'\n';FakeProcess::success=false;std::cout<<!b.launchSecondInstance("delta",false).isEmpty()<<':'<<FakeProcess::launches<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-secondinstance-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True,stderr=subprocess.DEVNULL))
assert outputs[0]==outputs[1]=='1:1:1:0\n1:1:1:0:1\n1:1:1\n1:2:2:127.0.0.1:4533\n1:3\n',outputs
print('Second instance A/B: invalid names, profile setup, shared CAT failure/success and launch failure passed')

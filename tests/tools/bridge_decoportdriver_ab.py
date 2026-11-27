#!/usr/bin/env python3
"""Compare DecoPort automatic driver safety checks using simulated devices."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/decoportdriver_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeDecoPortDriver.cpp').read_text();mark='void DecodiumBridge::decoPortEnsureRigDriver()';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <iostream>
#define connect fakeConnect
template<class...A>void fakeConnect(A...){ }
QStringList events;void bridgeLog(const QString&s){events<<s;}
struct DecodiumRigDetector{static QVariantList candidates;static QVariantList detect(){return candidates;}};QVariantList DecodiumRigDetector::candidates;
struct DecoPortRigDriver{static int model;bool openedFlag=false;bool isOpen(){return openedFlag;}static int resolveModel(const QString&,const QString&,QString*m){*m="match";return model;}void opened(const QString&){}void failed(const QString&){}void open(const QString&p,int baud,int civ,int model){events<<QString("open:%1:%2:%3:%4").arg(p).arg(baud).arg(civ).arg(model);openedFlag=true;}};int DecoPortRigDriver::model=1;
struct Cat{QString serial="CAT1",ptt="PTT1";QString serialPort(){return serial;}QString pttPort(){return ptt;}};
class DecodiumBridge:public QObject{public:DecoPortRigDriver*m_decoPortRig=nullptr;Cat cat,*m_hamlibCat=&cat;void decoPortEnsureRigDriver();};
#include "methods.inc"
void show(const char*n,DecodiumBridge&b){std::cout<<n<<':'<<(b.m_decoPortRig&&b.m_decoPortRig->isOpen())<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge b;b.decoPortEnsureRigDriver();show("none",b);QVariantMap m{{"rigLabel","Synthetic"},{"rigToken","test"},{"catPort"," "},{"baudRate",9600},{"civAddress",7},{"isRadio",true}};DecodiumRigDetector::candidates={m};b.decoPortEnsureRigDriver();show("noport",b);m["catPort"]=" cat1 ";DecodiumRigDetector::candidates={m};b.decoPortEnsureRigDriver();show("clash",b);m["catPort"]=" testport ";DecodiumRigDetector::candidates={m};DecoPortRigDriver::model=0;b.decoPortEnsureRigDriver();show("nomodel",b);DecoPortRigDriver::model=1;m["isRadio"]=false;DecodiumRigDetector::candidates={m};b.decoPortEnsureRigDriver();show("notradio",b);m["isRadio"]=true;DecodiumRigDetector::candidates={m};b.decoPortEnsureRigDriver();show("open",b);b.decoPortEnsureRigDriver();show("already",b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-decoportdriver-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==7
assert 'nothing detected' in lines[0] and 'no CAT port' in lines[1] and 'already the application' in lines[2]
assert 'no Hamlib model' in lines[3] and 'not recognised as a radio' in lines[4]
assert lines[5].startswith('open:1|') and 'open:testport:9600:7:1' in lines[5] and lines[6]=='already:1|'
print('DecoPort driver A/B: detection, CAT conflict, model/radio guards and simulated open passed')

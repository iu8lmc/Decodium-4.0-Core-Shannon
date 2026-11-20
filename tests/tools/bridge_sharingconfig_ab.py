#!/usr/bin/env python3
"""Compare CAT and spot sharing configuration and routing decisions."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/sharingconfig_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeSharingConfig.cpp').read_text();mark='void DecodiumBridge::configureCatShare(';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeSharingConfigLog(', 'bridgeLog(');assert old==new
source=r'''
#include <QObject>
#include <QString>
#include <QVariant>
#include <QMap>
#include <iostream>
void bridgeLog(const QString&){}
struct CatShare{bool en=false,ctrl=false,ptt=false;int port=0;void configure(bool e,int p,bool c,bool t){en=e;port=p;ctrl=c;ptt=t;}};
struct Native:QObject{bool en=false,ptt=false;int port=0,virtuals=0;bool enabled(){return en;}int rawPort(){return port;}void configure(bool e,bool t,int p,const QString&){en=e;ptt=t;port=p;}void createVirtualPort(){virtuals++;}};
struct Spot{bool en=false;int port=0;void configure(bool e,int p){en=e;port=p;}};
struct Hamlib{bool conn=false,ing=false;int reopen=0;bool connected(){return conn;}bool connecting(){return ing;}void connectRig(){reopen++;}};
class DecodiumBridge{public:CatShare*m_catShare=nullptr;Native*m_nativeCatShare=nullptr;Spot*m_spotShare=nullptr;Hamlib*m_hamlibCat=nullptr;QString m_catBackend="hamlib";QMap<QString,QVariant> values;void setSetting(const QString&k,const QVariant&v){values[k]=v;}void configureCatShare(bool,int,bool,bool);QObject*nativeCatShareObject()const;void configureNativeCatShare(bool,bool,int);void createNativeCatShareVirtualPort();void configureSpotShare(bool,int);};
#include "methods.inc"
int main(){DecodiumBridge b;b.configureCatShare(true,1,true,false);b.configureNativeCatShare(true,true,10);b.configureSpotShare(true,3);std::cout<<b.values["CatSharePort"].toInt()<<':'<<b.values["NativeCatShareRawPort"].toInt()<<':'<<b.values["SpotSharePort"].toInt()<<':'<<(b.nativeCatShareObject()==nullptr)<<'\n';CatShare c;Native n;Spot s;Hamlib h;b.m_catShare=&c;b.m_nativeCatShare=&n;b.m_spotShare=&s;b.m_hamlibCat=&h;h.conn=true;b.configureCatShare(true,5555,true,true);b.configureNativeCatShare(true,false,4444);b.configureNativeCatShare(true,true,4444);b.createNativeCatShareVirtualPort();b.configureSpotShare(true,9999);std::cout<<c.en<<':'<<c.port<<':'<<c.ctrl<<':'<<c.ptt<<':'<<n.en<<':'<<n.ptt<<':'<<n.port<<':'<<n.virtuals<<':'<<h.reopen<<':'<<s.en<<':'<<s.port<<':'<<(b.nativeCatShareObject()==&n)<<'\n';b.m_catBackend="other";b.configureNativeCatShare(false,false,70000);std::cout<<n.en<<':'<<n.port<<':'<<h.reopen<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-sharingconfig-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='4533:0:4534:1\n1:5555:1:1:1:1:4444:1:1:1:9999:1\n0:0:1\n',outputs
print('Sharing configuration A/B: ports, settings, permissions, routing, reconnect and virtual-port forwarding passed')

#!/usr/bin/env python3
"""Compare DecoPort hub rows with simulated device and port owners."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/decoporthubrows_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeDecoPortHubRows.cpp').read_text();mark='namespace {\n#define HUB_TR';new=mark+s.split(mark,1)[1].split('#undef HUB_TR',1)[0];assert old==new
source=r'''
#include <QCoreApplication>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QHash>
#include <QList>
#include <QJsonDocument>
#include <iostream>
struct FakeSerialInfo{QString n,d;QString portName()const{return n;}QString description()const{return d;}QString systemLocation()const{return n;}static QList<FakeSerialInfo> availablePorts(){return {{"one","USB"},{"two","com0com"},{"three","USB"},{"four","USB"}};}};
#define QSerialPortInfo FakeSerialInfo
namespace decoport_hub{struct PortUse{bool used=false,known=true;quint32 pid=0;QString program,state;};QHash<QString,PortUse>serialOwners(){return {{"THREE",{true,true,55,"Other","open"}}};}PortUse tcpOwner(quint16 p){if(p==4532)return {false,false,0,"",""};if(p==4534)return {true,true,55,"Other","LISTEN"};return {};}PortUse udpOwner(quint16 p){if(p==2237)return {true,true,quint32(QCoreApplication::applicationPid()),"","bound"};return {};}}
namespace decodium::ports{struct SerialPortRegistry{static QString ownerForPort(const QString&s){return s=="one"?"native":QString();}};}
#include "methods.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto rows=buildDecoPortHubRows("FOUR",4555);for(auto const&r:rows)std::cout<<QJsonDocument::fromVariant(r).toJson(QJsonDocument::Compact).toStdString()<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-hubrows-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);raw=subprocess.check_output([str(p/'test')],text=True);import json,re;out.append([re.sub(r'PID \d+','PID <pid>',x) for x in raw.splitlines()])
assert out[0]==out[1];rows=[json.loads(x) for x in out[0]];assert len(rows)==12
assert [x['state'] for x in rows[:4]]==['decodium','free','busy','decodium'] and rows[1]['kind']=='virtual'
assert rows[4]['name']=='TCP 4555' and rows[5]['state']=='unknown' and rows[6]['state']=='busy' and rows[-1]['state']=='decodium'
print('DecoPort hub rows A/B: serial ownership, virtual ports and TCP/UDP states passed')

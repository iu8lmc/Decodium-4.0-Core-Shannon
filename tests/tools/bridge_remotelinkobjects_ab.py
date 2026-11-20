#!/usr/bin/env python3
"""Compare lazy remote objects and their guarded disconnection callbacks."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/remotelinkobjects_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeRemoteLinkObjects.cpp').read_text();mark='QObject* DecodiumBridge::decoPortDiscoveryObject()';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QObject>
#include <QString>
#include <QByteArray>
#include <functional>
#include <vector>
#include <type_traits>
#include <iostream>
#define DECODIUM_HAS_SSTV 1
#define connect fakeConnect
std::vector<std::function<void()>> closed;std::vector<std::function<void(quint32)>> streams;
template<class A,class B,class C,class F,class...E>void fakeConnect(A*,B,C*,F f,E...){if constexpr(std::is_invocable_v<F>)closed.push_back(f);else streams.push_back(f);}
namespace decodium::sstv{enum class SstvAudioSourceKind{DecoPort};}
class RemoteRadioLink:public QObject{public:explicit RemoteRadioLink(QObject*p):QObject(p){}bool linked=false;bool isLinked(){return linked;}void linkedChanged(){}void remoteStreamChanged(quint32){}};
class DecoPortLink:public RemoteRadioLink{public:explicit DecoPortLink(QObject*p):RemoteRadioLink(p){}void setAuthKey(const QByteArray&){}};
class DecolinkLink:public RemoteRadioLink{public:explicit DecolinkLink(QObject*p):RemoteRadioLink(p){}};
class DecoPortDiscovery:public QObject{public:explicit DecoPortDiscovery(QObject*p):QObject(p){}void setAuthKey(const QByteArray&){}};
class DecodiumBridge:public QObject{public:mutable DecoPortDiscovery*m_decoPortDiscovery=nullptr;mutable DecoPortLink*m_decoPortLink=nullptr;mutable DecolinkLink*m_decolinkLink=nullptr;bool m_decoPortUseRemote=false,m_remoteIsDecolink=false,m_sstvRxRequested=false;int localOff=0,relayOff=0,selected=0;quint32 last=0;QByteArray decoPortAuthKey(){return "synthetic";}QByteArray decoPortAuthKey()const{return "synthetic";}void setDecoPortUseRemote(bool){localOff++;}void setDecolinkUseRemote(bool){relayOff++;}void selectSstvRxSource(decodium::sstv::SstvAudioSourceKind,quint32 stream){selected++;last=stream;}QObject*decoPortDiscoveryObject()const;QObject*decoPortLinkObject()const;QObject*decolinkLinkObject()const;};
#include "methods.inc"
int main(){DecodiumBridge b;auto*d=b.decoPortDiscoveryObject();auto*l=b.decoPortLinkObject();auto*r=b.decolinkLinkObject();std::cout<<(d==b.decoPortDiscoveryObject())<<':'<<(l==b.decoPortLinkObject())<<':'<<(r==b.decolinkLinkObject())<<':'<<closed.size()<<':'<<streams.size()<<'\n';closed[0]();closed[1]();streams[0](10);b.m_decoPortUseRemote=true;b.m_sstvRxRequested=true;streams[0](0);streams[0](10);closed[0]();closed[1]();std::cout<<b.localOff<<':'<<b.relayOff<<':'<<b.selected<<':'<<b.last<<'\n';b.m_remoteIsDecolink=true;streams[1](20);closed[0]();closed[1]();std::cout<<b.localOff<<':'<<b.relayOff<<':'<<b.selected<<':'<<b.last<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-remotelinkobjects-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1]=='1:1:1:2:2\n1:0:1:10\n1:1:2:20\n',out
print('Remote objects A/B: lazy reuse, correct disconnect ownership and SSTV stream guards passed')

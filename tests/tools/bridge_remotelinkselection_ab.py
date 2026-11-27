#!/usr/bin/env python3
"""Compare remote link choice and switching behaviour."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/remotelinkselection_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeRemoteLinkSelection.cpp').read_text();mark='RemoteRadioLink* DecodiumBridge::remoteLink() const';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QObject>
#include <iostream>
struct RemoteRadioLink{virtual ~RemoteRadioLink()=default;};struct DecoPort:RemoteRadioLink{};struct Decolink:RemoteRadioLink{};
class DecodiumBridge{public:mutable DecoPort*m_decoPortLink=nullptr;mutable Decolink*m_decolinkLink=nullptr;bool m_remoteIsDecolink=false,m_decoPortUseRemote=false,allow=true;int ssbstop=0,changes=0,onCalls=0,offCalls=0;void decoPortLinkObject()const{if(!m_decoPortLink)m_decoPortLink=new DecoPort;}void decolinkLinkObject()const{if(!m_decolinkLink)m_decolinkLink=new Decolink;}void ssbStop(){ssbstop++;}void setDecoPortUseRemote(bool v){if(v)onCalls++;else offCalls++;m_decoPortUseRemote=v&&allow;}void decoPortUseRemoteChanged(){changes++;}RemoteRadioLink*remoteLink()const;RemoteRadioLink*remoteLinkIfAny()const;void setDecolinkUseRemote(bool);};
#include "methods.inc"
int main(){DecodiumBridge b;std::cout<<(b.remoteLinkIfAny()==nullptr)<<':'<<(b.remoteLink()!=nullptr)<<':'<<(b.remoteLinkIfAny()==b.m_decoPortLink)<<'\n';b.m_decoPortUseRemote=true;b.setDecolinkUseRemote(true);std::cout<<b.m_remoteIsDecolink<<':'<<b.m_decoPortUseRemote<<':'<<b.offCalls<<':'<<b.onCalls<<':'<<b.ssbstop<<':'<<b.changes<<':'<<(b.remoteLinkIfAny()==nullptr)<<':'<<(b.remoteLink()!=nullptr)<<'\n';b.setDecolinkUseRemote(false);std::cout<<b.m_remoteIsDecolink<<':'<<b.m_decoPortUseRemote<<':'<<b.offCalls<<':'<<b.changes<<'\n';b.allow=false;b.setDecolinkUseRemote(true);std::cout<<b.m_remoteIsDecolink<<':'<<b.m_decoPortUseRemote<<':'<<b.onCalls<<':'<<b.changes<<'\n';b.setDecolinkUseRemote(false);std::cout<<b.offCalls<<':'<<b.changes<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-remotelink-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='1:1:1\n1:1:1:1:1:1:1:1\n0:0:2:2\n0:0:2:3\n2:4\n',outputs
print('Remote link selection A/B: lazy links, switch, disable and failed activation passed')

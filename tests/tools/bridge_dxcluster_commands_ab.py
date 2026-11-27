#!/usr/bin/env python3
"""Compare DX cluster command behaviour before and after extraction."""
import os, shlex, subprocess, tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[2]
old = (root / 'tests/bridge_refactor_fixtures/dxcluster_commands_before.inc').read_text()
module = (root / 'src/bridge/DecodiumBridgeDxClusterCommands.cpp').read_text()
new = 'void DecodiumBridge::connectDxCluster()' + module.split('void DecodiumBridge::connectDxCluster()', 1)[1]
assert old == new.replace('decodiumBridgeDxClusterLog(', 'bridgeLog(')
source = r'''
#include <QString>
#include <iostream>
void bridgeLog(const QString&) {}
class Cluster { public: QString h="old", call="oldcall"; int p=8000, connects=0, disconnects=0; QString host()const{return h;} int port()const{return p;} void setHost(const QString&v){h=v;} void setPort(int v){p=v;} void setCallsign(const QString&v){call=v;} void connectCluster(){++connects;} void disconnectCluster(){++disconnects;} };
class DecodiumBridge {public: Cluster* m_dxCluster=nullptr; QString m_callsign; bool setting=false; void setSetting(const QString&key,bool value){if(key=="AutoSpotEnabled")setting=value;} void connectDxCluster();void connectDxCluster(const QString&,int);void disconnectDxCluster();void setAutoSpotEnabled(bool);};
#include "methods.inc"
int main(){Cluster c; DecodiumBridge b;b.connectDxCluster();b.disconnectDxCluster(); b.m_dxCluster=&c; b.connectDxCluster("  host.test  ",1234);std::cout<<c.h.toStdString()<<":"<<c.p<<":"<<c.call.toStdString()<<":"<<c.connects<<"\n";b.m_callsign="9H1SR";b.connectDxCluster("  ",-1);b.connectDxCluster();b.disconnectDxCluster();b.setAutoSpotEnabled(true);std::cout<<c.h.toStdString()<<":"<<c.p<<":"<<c.call.toStdString()<<":"<<c.connects<<":"<<c.disconnects<<":"<<b.setting<<"\n";b.setAutoSpotEnabled(false);std::cout<<b.setting<<"\n";}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-dxcluster-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new.replace('decodiumBridgeDxClusterLog(', 'bridgeLog(')):
  (p/'methods.inc').write_text(body)
  subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True)
  outputs.append(subprocess.check_output([str(p/'test')],text=True))
 assert outputs[0]==outputs[1]=='host.test:1234:oldcall:1\nhost.test:1234:9H1SR:3:1:1\n0\n',outputs
print('DX Cluster commands A/B: null, defaults, host, port, callsign, connect, disconnect and autospot passed')

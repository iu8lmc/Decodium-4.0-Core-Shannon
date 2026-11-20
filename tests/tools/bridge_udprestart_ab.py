#!/usr/bin/env python3
"""Compare UDP client shutdown and deferred restart single-flight."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/udprestart_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeUdpRestart.cpp').read_text();mark='void DecodiumBridge::shutdownUdpMessageClient()';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeUdpRestartLog(', 'bridgeLog(');assert old==new
source=r'''
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <iostream>
void bridgeLog(const QString&){}int disconnected=0,deleted=0;
class Client:public QObject{public:void disconnect(QObject*){disconnected++;}void deleteLater(){deleted++;QObject::deleteLater();}};
class DecodiumBridge:public QObject{public:Client*m_udpMessageClient=nullptr,*m_udpSecondaryMessageClient=nullptr,*m_udpTertiaryMessageClient=nullptr;bool m_udpMessageClientRestartPending=false;int initialized=0;void initUdpMessageClient(){initialized++;}void shutdownUdpMessageClient();void scheduleUdpMessageClientRestart();};
#include "methods.inc"
void wait(){QEventLoop l;QTimer::singleShot(250,&l,&QEventLoop::quit);l.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;b.shutdownUdpMessageClient();std::cout<<disconnected<<':'<<deleted<<'\n';b.m_udpMessageClient=new Client;b.m_udpSecondaryMessageClient=new Client;b.m_udpTertiaryMessageClient=new Client;b.scheduleUdpMessageClientRestart();b.scheduleUdpMessageClientRestart();std::cout<<b.m_udpMessageClientRestartPending<<':'<<b.initialized<<'\n';wait();std::cout<<b.m_udpMessageClientRestartPending<<':'<<b.initialized<<':'<<disconnected<<':'<<deleted<<':'<<(b.m_udpMessageClient==nullptr)<<':'<<(b.m_udpSecondaryMessageClient==nullptr)<<':'<<(b.m_udpTertiaryMessageClient==nullptr)<<'\n';b.scheduleUdpMessageClientRestart();wait();std::cout<<b.initialized<<':'<<disconnected<<':'<<deleted<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-udprestart-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='0:0\n1:0\n0:1:3:3:1:1:1\n2:3:3\n',outputs
print('UDP restart A/B: three-client shutdown, deferred coalescing and repeat passed')

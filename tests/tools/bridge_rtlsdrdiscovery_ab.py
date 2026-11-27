#!/usr/bin/env python3
"""Compare RTL-SDR asynchronous enumeration and coalescing."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/rtlsdrdiscovery_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeRtlSdrDiscovery.cpp').read_text();mark='void DecodiumBridge::refreshRtlSdrDevices()';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QCoreApplication>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QEventLoop>
#include <QThread>
#include <QTimer>
#include <QStringList>
#include <atomic>
#include <iostream>
class RtlSdrInput{public:static std::atomic<int> calls;static int mode;static QStringList enumerateDevices(QString*error){++calls;QThread::msleep(100);if(mode==2){*error="synthetic error";return {};}return mode==0?QStringList{}:QStringList{"synthetic"};}};std::atomic<int>RtlSdrInput::calls{0};int RtlSdrInput::mode=0;
class DecodiumBridge:public QObject{public:bool m_rtlSdrDiscoveryPending=false;QString m_rtlSdrStatus;QStringList m_rtlSdrDevices;int statusCount=0,deviceCount=0;void rtlSdrStatusChanged(){statusCount++;}void rtlSdrDevicesChanged(){deviceCount++;}void refreshRtlSdrDevices();};
#include "methods.inc"
void wait(){QEventLoop l;QTimer::singleShot(350,&l,&QEventLoop::quit);l.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;b.refreshRtlSdrDevices();b.refreshRtlSdrDevices();std::cout<<b.m_rtlSdrDiscoveryPending<<':'<<b.statusCount<<'\n';wait();std::cout<<RtlSdrInput::calls<<':'<<b.m_rtlSdrDiscoveryPending<<':'<<b.m_rtlSdrStatus.toStdString()<<':'<<b.deviceCount<<'\n';RtlSdrInput::mode=1;b.refreshRtlSdrDevices();wait();std::cout<<RtlSdrInput::calls<<':'<<b.m_rtlSdrDevices.join(',').toStdString()<<':'<<b.deviceCount<<':'<<b.m_rtlSdrStatus.toStdString()<<'\n';RtlSdrInput::mode=2;b.refreshRtlSdrDevices();wait();std::cout<<RtlSdrInput::calls<<':'<<b.m_rtlSdrDevices.size()<<':'<<b.deviceCount<<':'<<b.m_rtlSdrStatus.toStdString()<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Concurrent'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-rtlsdrdiscovery-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='1:1\n1:0:RTL-SDR: no receiver detected:0\n2:synthetic:1:RTL-SDR: 1 receiver(s) detected\n3:0:2:synthetic error\n',outputs
print('RTL-SDR discovery A/B: coalescing, empty, device and error results passed')

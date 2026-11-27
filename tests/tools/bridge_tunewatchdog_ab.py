#!/usr/bin/env python3
"""Compare Tune watchdog timer, safety limits and timeout paths."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/tunewatchdog_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeTuneWatchdog.cpp').read_text();mark='void DecodiumBridge::armTuneWatchdog()';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeTuneWatchdogLog(', 'bridgeLog(');assert old==new
source=r'''
#include <QCoreApplication>
#include <QTimer>
#include <QVariant>
#include <iostream>
void bridgeLog(const QString&){}
class DecodiumBridge:public QObject{public:bool enabled=true,m_tuning=false,m_bridgeAudioTuneActive=false;int seconds=90,stops=0,status=0;QTimer*m_tuneWatchdogTimer=nullptr;QVariant getSetting(const QString&k,const QVariant&)const{return k=="TuneWatchdog"?QVariant(enabled):QVariant(seconds);}void statusMessage(const QString&){status++;}void stopTune(){stops++;}void armTuneWatchdog();void disarmTuneWatchdog(const QString&);};
#include "methods.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;b.enabled=false;b.armTuneWatchdog();std::cout<<(b.m_tuneWatchdogTimer==nullptr)<<'\n';b.enabled=true;b.seconds=0;b.armTuneWatchdog();std::cout<<(b.m_tuneWatchdogTimer==nullptr)<<'\n';b.seconds=1;b.armTuneWatchdog();std::cout<<b.m_tuneWatchdogTimer->interval()<<':'<<b.m_tuneWatchdogTimer->isSingleShot()<<'\n';QMetaObject::invokeMethod(b.m_tuneWatchdogTimer,"timeout",Qt::DirectConnection);std::cout<<b.stops<<':'<<b.status<<'\n';b.m_tuning=true;QMetaObject::invokeMethod(b.m_tuneWatchdogTimer,"timeout",Qt::DirectConnection);std::cout<<b.stops<<':'<<b.status<<'\n';b.m_tuning=false;b.m_bridgeAudioTuneActive=true;QMetaObject::invokeMethod(b.m_tuneWatchdogTimer,"timeout",Qt::DirectConnection);std::cout<<b.stops<<':'<<b.status<<'\n';b.seconds=9000;b.armTuneWatchdog();std::cout<<b.m_tuneWatchdogTimer->interval()<<':'<<b.m_tuneWatchdogTimer->isActive()<<'\n';b.disarmTuneWatchdog("test");std::cout<<b.m_tuneWatchdogTimer->isActive()<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-tunewatchdog-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='1\n1\n5000:1\n0:0\n1:1\n2:2\n3600000:1\n0\n',outputs
print('Tune watchdog A/B: disable, bounds, idle/active timeout, single-shot and disarm passed')

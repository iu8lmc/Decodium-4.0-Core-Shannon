#!/usr/bin/env python3
"""Compare UTC period timer arming, immediate modes and stale callbacks."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/periodtimerarm_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgePeriodTimerArm.cpp').read_text();mark='void DecodiumBridge::armPeriodTimerForCurrentMode(';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgePeriodTimerLog(', 'bridgeLog(').replace('decodiumBridgeOwnsLegacyJtRxWhenCatSuppressed(', 'bridgeOwnsLegacyJtRxWhenCatSuppressed(').replace('decodiumBridgeOwnsFst4Audio(', 'bridgeOwnsFst4Audio(');assert old==new
source=r'''
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <iostream>
void bridgeLog(const QString&){}bool bridgeOwnsLegacyJtRxWhenCatSuppressed(const QString&m){return m=="JT65";}bool bridgeOwnsFst4Audio(const QString&m){return m=="FST4";}
class DecodiumBridge:public QObject{public:static constexpr int TIMER_MS=250;QTimer*m_periodTimer=nullptr;QString m_mode="FT8",m_bridgeOwnedJtFallbackLastMode="old";bool m_monitoring=true,m_ntpEnabled=false,m_ntpSynced=false;double m_ntpOffsetMs=0;int m_periodTicks=9,resets=0;long long m_lastPeriodSlot=4,m_bridgeOwnedJtFallbackLastSlot=5,m_bridgeOwnedJtFallbackLastDispatchMs=6;quint64 m_periodTimerSessionId=1;int effectivePeriodMsForMode(const QString&)const{return 300;}qint64 correctedUtcMsecsSinceStartOfDay()const{return 0;}void resetRxPeriodAccumulation(bool v){if(v)resets++;}void armPeriodTimerForCurrentMode(quint64,const QString&);};
#include "methods.inc"
void wait(){QEventLoop l;QTimer::singleShot(380,&l,&QEventLoop::quit);l.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge empty;empty.armPeriodTimerForCurrentMode(1,"test");std::cout<<empty.resets<<'\n';for(auto mode:{"WSPR","JT65","FST4"}){DecodiumBridge b;QTimer t;b.m_periodTimer=&t;b.m_mode=mode;b.armPeriodTimerForCurrentMode(1,"test");std::cout<<t.isActive()<<':'<<b.resets<<':'<<b.m_periodTicks<<':'<<b.m_lastPeriodSlot<<':'<<b.m_bridgeOwnedJtFallbackLastSlot<<':'<<b.m_bridgeOwnedJtFallbackLastDispatchMs<<':'<<b.m_bridgeOwnedJtFallbackLastMode.isEmpty()<<'\n';}for(int k=0;k<4;k++){DecodiumBridge b;QTimer t;b.m_periodTimer=&t;b.armPeriodTimerForCurrentMode(1,"test");if(k==1)b.m_monitoring=false;if(k==2)b.m_periodTimerSessionId++;if(k==3)b.m_mode="FT4";wait();std::cout<<t.isActive()<<':'<<b.resets<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-periodtimer-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='0\n'+'1:1:0:-1:-1:0:1\n'*3+'1:1\n0:0\n0:0\n0:0\n',outputs
print('UTC period timer A/B: immediate modes, aligned start and stale session guards passed')

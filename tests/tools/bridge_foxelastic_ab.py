#!/usr/bin/env python3
"""Compare the elastic timer gates and dispatch against the original."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/mamdispatch_before.inc').read_text().split('// Dispatch di un periodo TX MAM.',1)[0];new=('bool DecodiumBridge::foxElasticActive() const'+(r/'src/bridge/DecodiumBridgeMamDispatch.cpp').read_text().split('bool DecodiumBridge::foxElasticActive() const',1)[1]).split('// Dispatch di un periodo TX MAM.',1)[0];new=new.replace('decodiumBridgeMamQueueLog(', 'bridgeLog(')
if old!=new:raise SystemExit('elastic methods differ')
src=r'''
#include <QString>
#include <QDateTime>
#include <QTimer>
#include <QCoreApplication>
#include <iostream>
void bridgeLog(QString const&){}
struct Cycle{bool open=false,go=false;int ended=0,dispatch=0;void txEnded(qint64){open=true;++ended;}bool windowOpen()const{return open;}bool shouldGo(qint64)const{return go;}void dispatched(){open=false;++dispatch;}int pendingCount()const{return 2;}int newcomerDelayMs()const{return 50;}qint64 windowStartMs()const{return QDateTime::currentMSecsSinceEpoch();}};
class DecodiumBridge:public QObject{public:
 bool m_ft2ElasticFox=true,m_asyncTxEnabled=true,m_transmitting=false,m_tuning=false,m_manualTxHold=false,m_periodicTxInFlight=false,m_foxElasticGo=false;QString m_mode="FT2";QTimer*m_foxElasticTimer=nullptr;Cycle m_foxElastic;int checks=0;bool active=true,sawGo=false;
 bool mamMultiStreamSequencerActive()const{return active;}void checkAndStartPeriodicTx(){++checks;sawGo=m_foxElasticGo;}
 bool foxElasticActive()const;void foxElasticOnTxFinished();void foxElasticPoll();
};
#include "method.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;b.foxElasticOnTxFinished();std::cout<<b.m_foxElastic.ended<<','<<b.m_foxElasticTimer->isActive()<<'\n';b.foxElasticPoll();std::cout<<b.m_foxElastic.windowOpen()<<','<<b.checks<<'\n';b.m_foxElastic.go=true;b.m_periodicTxInFlight=true;b.foxElasticPoll();std::cout<<b.m_foxElastic.windowOpen()<<','<<b.checks<<'\n';b.m_periodicTxInFlight=false;b.foxElasticPoll();std::cout<<b.m_foxElastic.windowOpen()<<','<<b.checks<<','<<b.sawGo<<','<<b.m_foxElasticGo<<','<<b.m_foxElasticTimer->isActive()<<'\n';b.foxElasticOnTxFinished();b.m_manualTxHold=true;b.foxElasticPoll();std::cout<<b.m_foxElastic.windowOpen()<<','<<b.checks<<','<<b.m_foxElastic.dispatch<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-foxelastic-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
expected='1,1\n1,0\n1,0\n0,1,1,0,0\n0,1,2\n'
if outputs!=[expected,expected]:raise SystemExit('elastic A/B mismatch '+repr(outputs))
print('Fox elastic A/B OK\n'+expected,end='')

#!/usr/bin/env python3
"""Compare completed QSO routing, logging and queued TX checks."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/qsofinish_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeQsoFinish.cpp').read_text();mark='bool DecodiumBridge::finishAutoSequenceQsoFromCompletedSignoff(';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeQsoFinishLog(', 'bridgeLog(');assert old==new
source=r'''
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QStringList>
#include <iostream>
void bridgeLog(const QString&){}
class DecodiumBridge:public QObject{public:bool courtesy=false,m_pendingAutoLogValid=false,m_qsoLogged=false,m_multiAnswerMode=false,m_autoCqRepeat=false,autoCq=false,deferred=false,advance=true,m_txEnabled=false,m_periodicTxCheckScheduled=false,m_transmitting=false,m_tuning=false,preserved=false;QString partner="K1ABC",m_pendingAutoLogCall="fallback",marked;QStringList m_callerQueue;quint64 m_txRequestGeneration=1;int snapshot=0,remember=0,logs=0,resets=0,next=0,checks=0;bool rememberedLog=false;bool releaseCourtesySignoffIfActive(const QString&){return courtesy;}QString inferredPartnerForAutolog(){return partner;}void capturePendingAutoLogSnapshot(){snapshot++;}void markWorldMapQsoClosed(const QString&p,const QString&){marked=p;}void rememberCompletedAutoCqPartner(const QString&,bool v,const QString&){remember++;rememberedLog=v;}void logQso(){logs++;}bool autoCqCanRestartTx6(){return autoCq;}void resetQsoStateForNextSequence(bool v){resets++;preserved=v;}bool applyDeferredSpecialOperationActivityIfReady(){return deferred;}void processNextInQueue(){next++;}bool advanceQsoState(int){return advance;}void setTxEnabled(bool v){m_txEnabled=v;}void checkAndStartPeriodicTx(){checks++;}bool finishAutoSequenceQsoFromCompletedSignoff(const QString&,bool);};
#include "methods.inc"
void wait(){QEventLoop l;QTimer::singleShot(20,&l,&QEventLoop::quit);l.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(int k=0;k<7;k++){DecodiumBridge b;if(k==0)b.courtesy=true;if(k==2){b.partner=" ";b.m_pendingAutoLogValid=true;}if(k==3){b.m_multiAnswerMode=true;b.m_callerQueue<<"x";}if(k>=4)b.autoCq=true;if(k==5)b.m_txRequestGeneration=1;if(k==6)b.deferred=true;bool result=b.finishAutoSequenceQsoFromCompletedSignoff("test",k==2);if(k==5)b.m_txRequestGeneration++;wait();std::cout<<result<<':'<<b.snapshot<<':'<<b.remember<<':'<<b.rememberedLog<<':'<<b.logs<<':'<<b.resets<<':'<<b.preserved<<':'<<b.next<<':'<<b.checks<<':'<<b.m_txEnabled<<':'<<b.marked.trimmed().toStdString()<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-qsofinish-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
expected='1:0:0:0:0:0:0:0:0:0:\n1:0:1:0:0:1:0:0:0:0:K1ABC\n1:1:1:1:1:1:0:0:0:0:fallback\n1:0:1:0:0:1:1:1:0:0:K1ABC\n1:0:1:0:0:1:1:0:1:1:K1ABC\n1:0:1:0:0:1:1:0:0:1:K1ABC\n1:0:1:0:0:1:1:0:0:0:K1ABC\n';assert outputs[0]==outputs[1]==expected,outputs
print('QSO finish A/B: courtesy, log, queue, automatic CQ and stale timer passed')

#!/usr/bin/env python3
"""A/B courtesy 73 guards, repeated replies, release and scheduled work."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/courtesy_before.inc').read_text();new='bool DecodiumBridge::maybeStartCourtesySignoff('+(r/'src/bridge/DecodiumBridgeCourtesySignoff.cpp').read_text().split('bool DecodiumBridge::maybeStartCourtesySignoff(',1)[1];new=new.replace('decodiumBridgeCourtesyLog(', 'bridgeLog(').replace('decodiumBridgeRecentAutoCqPartnerWindowSeconds()', 'kRecentAutoCqPartnerWindowSeconds');assert old==new
src=r'''
#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QHash>
#include <QTimer>
#include <QStringList>
#include <iostream>
constexpr int kRecentAutoCqPartnerWindowSeconds=600;void bridgeLog(const QString&){}namespace decodium::seq {bool courtesySignoffDue(const QString& msg,int count,int maximum){return !msg.isEmpty()&&count<maximum;}}
class DecodiumBridge:public QObject {public:QString m_mode="FT8",m_courtesySignoffBase,m_dxCall,m_autoCqLockedCall;bool m_autoCqRepeat=true,m_transmitting=false,m_tuning=false,m_manualTxHold=false,m_logAfterOwn73=false,m_ft2DeferredLogPending=false,m_qsoLogged=false,m_txEnabled=true,m_periodicTxCheckScheduled=false,duplicate=true,disabled=false,advance=true;int m_currentTx=6,m_txRetryCount=1,m_nTx73=1,periodic=0,finished=0,cleared=0,remembered=0;double m_frequency=14074000;quint64 m_txRequestGeneration=1;QStringList m_callerQueue;QHash<QString,QList<qint64>> m_courtesySignoffSentMs;bool isTxDisabled(int){return disabled;}bool isRecentAutoCqWorkedOrLoggedDuplicate(const QString&,double,const QString&){return duplicate;}void setDxCall(const QString&s){m_dxCall=s;}void setDxGrid(const QString&){}void genStdMsgs(const QString&,const QString&){}bool advanceQsoState(int){return advance;}void setTxEnabled(bool v){m_txEnabled=v;}void checkAndStartPeriodicTx(){periodic++;}bool finishAutoSequenceQsoFromCompletedSignoff(const QString&,bool){finished++;return true;}void clearRecentAutoCqAbandoned(const QString&,double,const QString&,const QString&){cleared++;}void rememberRecentAutoCqWorked(const QString&,double,const QString&){remembered++;}bool maybeStartCourtesySignoff(const QString&,const QString&,const QString&);bool releaseCourtesySignoffIfActive(const QString&);};
#include "methods.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(int k=0;k<12;k++){DecodiumBridge b;if(k==1)b.m_autoCqRepeat=false;if(k==2)b.m_mode="JT65";if(k==3)b.m_transmitting=true;if(k==4)b.m_tuning=true;if(k==5)b.m_manualTxHold=true;if(k==6)b.m_dxCall="BUSY";if(k==7)b.m_callerQueue<<"K1ABC";if(k==8)b.disabled=true;if(k==9)b.duplicate=false;if(k==10)b.advance=false;if(k==11)b.m_courtesySignoffSentMs["K1ABC"]={QDateTime::currentMSecsSinceEpoch(),QDateTime::currentMSecsSinceEpoch()};bool started=b.maybeStartCourtesySignoff("K1ABC","K1ABC","K1ABC 73");QEventLoop loop;QTimer::singleShot(5,&loop,&QEventLoop::quit);loop.exec();bool released=b.releaseCourtesySignoffIfActive("test");std::cout<<started<<':'<<released<<':'<<b.periodic<<':'<<b.finished<<':'<<b.cleared<<':'<<b.remembered<<':'<<b.m_qsoLogged<<'\n';}}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-courtesy-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'methods.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1],out
lines=out[0].splitlines();assert len(lines)==12 and lines[0]=='1:1:1:1:1:1:1' and all(x.startswith('0:0:0:0:0:0:') for x in lines[1:]),lines
print('courtesy A/B: QSO/queue/PTT guards, repeat limit, release and timer passed')

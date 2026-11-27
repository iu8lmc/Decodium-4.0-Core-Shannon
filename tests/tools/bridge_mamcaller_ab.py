#!/usr/bin/env python3
"""Compare MAM caller ingestion and queue operations with controlled dependencies."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/mamcaller_before.inc').read_text();new='void DecodiumBridge::removeCallerFromQueue('+(r/'src/bridge/DecodiumBridgeMamCaller.cpp').read_text().split('void DecodiumBridge::removeCallerFromQueue(',1)[1];new=new.replace('decodiumBridgeMamQueueLog(', 'bridgeLog(').replace('decodiumBridgeInferPartnerForAutolog(', 'inferPartnerFromDirectedMessage(');assert old==new
src=r'''
#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QStringList>
#include <iostream>
namespace Radio{QString base_callsign(const QString&s){return s.section('/',0,0);}}
QString normalizedBaseCall(const QString&s){return Radio::base_callsign(s.trimmed()).toUpper();}QString normalizeCallToken(QString s){return s.trimmed();}bool tokenMatchesCall(const QString&s,const QString&c,const QString&){return s==c;}QString inferPartnerFromDirectedMessage(const QString&s,const QString&,const QString&){return s.simplified().split(' ').value(1);}int decodeSnrOrDefault(const QString&s,int d){bool ok=false;int x=s.toInt(&ok);return ok?x:d;}void bridgeLog(const QString&){}
struct DecodiumBridge{bool m_multiAnswerMode=true,m_autoCqRepeat=false,m_manualTxHold=false,m_txEnabled=false,duplicate=false,ghost=false,multistream=true;QString m_dxCall,m_autoCqLockedCall,m_callsign="9H1SR",m_mode="FT8";QStringList m_callerQueue;int m_currentTx=6,changes=0,worked=0,processed=0;double m_frequency=14074000;QHash<QString,qint64> m_qsoCooldown;QString inferredPartnerForAutolog()const{return {};}bool shouldSuppressDirectedGhostDecode(const QStringList&,const QString&){return ghost;}bool isRecentAutoCqDuplicate(const QString&,double,const QString&){return duplicate;}void rememberRecentAutoCqWorked(const QString&,double,const QString&){worked++;}bool enqueueCallerInternal(const QString& c,int f,int s,bool){m_callerQueue<<QString("%1 %2 %3").arg(c).arg(f).arg(s);return true;}bool mamMultiStreamSequencerActive(){return multistream;}void processNextInQueue(){processed++;}void callerQueueChanged(){changes++;}void removeCallerFromQueue(const QString&);QString activeMamCallerBase()const;void removeActiveMamCallerFromQueue(const QString&);void maybeEnqueueMamCallerFromDecode(const QStringList&,bool);void maybeEnqueueMamCallerFromMessage(const QString&,int,int,bool);};
#include "methods.inc"
int main(){for(int k=0;k<9;k++){DecodiumBridge b;if(k==1)b.m_manualTxHold=true;if(k==2)b.duplicate=true;if(k==3)b.m_dxCall="K1ABC";if(k==4)b.multistream=false;if(k==5)b.m_callerQueue<<"K1ABC 1200 -10";if(k==6)b.ghost=true;if(k==7)b.m_callsign="";QString msg=k==5?"9H1SR K1ABC 73":k==8?"OTHER K1ABC -10":"9H1SR K1ABC -10";QStringList fields={"120000","-10","0","0",msg,"","","1200"};b.maybeEnqueueMamCallerFromDecode(fields,true);std::cout<<b.m_callerQueue.size()<<':'<<b.worked<<':'<<b.m_qsoCooldown.size()<<':'<<b.processed<<':'<<b.changes<<'\n';}DecodiumBridge b;b.m_callerQueue<<"K1ABC/P 1000 -5"<<"W1AW 1100 -8";b.removeCallerFromQueue("K1ABC");std::cout<<b.m_callerQueue.join(',').toStdString()<<':'<<b.changes<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-mamcaller-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1],out
lines=out[0].splitlines();assert lines==['1:0:0:0:0','0:0:0:0:0','0:0:0:0:0','0:0:0:0:0','1:0:0:1:0','0:1:1:0:1','0:0:0:0:0','0:0:0:0:0','0:0:0:0:0','W1AW 1100 -8:1'],lines
print('MAM caller A/B: ingestion, duplicate/signoff guards and queue removal passed')

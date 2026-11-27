#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/caller_scoring_before.inc').read_text();sig='decodium::seq::CallerScoreInput DecodiumBridge::callerScoreInputFor('
s=(R/'src/bridge/DecodiumBridgeCallerScoring.cpp').read_text();b=sig+s.split(sig,1)[1];b=b.replace('decodiumBridgeInferPartnerForAutolog(', 'inferPartnerFromDirectedMessage(').replace('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')
if a!=b:raise SystemExit('Scoring methods differ from pre-extraction snapshot')
f=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
h=r'''
#include <QDateTime>
#include <QStringList>
#include <QSet>
#include <QHash>
#include <iostream>
#include "Sequencer/CallerScoring.hpp"
#define emit
using decodium::seq::normalizedBaseCall;
namespace decodium { namespace seq { QString normalizedBaseCall(QString const& s){return s.toUpper().split('/').first();} } }
QString freqHzToBandToken(quint64){return "20m";} QString workedCallBandKey(QString b,QString c){return b+"|"+c;} void bridgeLog(QString const&){}
QString normalizeCallToken(QString s){return s;} bool tokenMatchesCall(QString s,QString c,QString){return s==c;} QString inferPartnerFromDirectedMessage(QString s,QString,QString){return s.split(' ').value(1);} int decodeSnrOrDefault(QString s,int d){bool ok;int n=s.toInt(&ok);return ok?n:d;}
struct DxccEntity {QString name;bool isValid()const{return !name.isEmpty();}};struct DxccLookup {bool isLoaded(){return true;} DxccEntity lookup(QString s){return {s=="NEW"?"Newland":"Oldland"};}};
class DecodiumBridge { public:
 struct Worked {QSet<QString>callEver,callByBand,callTodayByBand,dxccEver,dxccByBand;} m_worked; quint64 m_frequency=14074000; DxccLookup *m_dxccLookup=nullptr; QStringList m_callerQueue;QSet<QString>m_callerQueuePinned;QHash<QString,qint64>m_callerQueueArrivalMs;bool m_callerScoring=true,m_callerQueueManualOrder=false,m_autoCqRepeat=true;QString m_dxCall,m_callsign="ME",m_mode="FT8";int notifications=0;
 void callerQueueChanged(){notifications++;} bool isRecentAutoCqDuplicate(QString const&,quint64,QString const&){return false;}
 QStringList parseFt8Row(QString const& s){return s.split('|');}
 decodium::seq::CallerScoreInput callerScoreInputFor(QString const&,int,bool,qint64)const;void rankCallerQueue(QString const&);QStringList scoredCallerRowOrder(QStringList const&);
};
#include "method.inc"
void check(bool v){if(!v)abort();}
int main(){ DecodiumBridge b; DxccLookup dx;b.m_dxccLookup=&dx;b.m_worked.callEver.insert("OLD");b.m_worked.callByBand.insert("20m|OLD");b.m_worked.dxccEver.insert("Oldland");
 auto x=b.callerScoreInputFor("OLD",-8,true,10);check(x.workedEver&&x.workedBand&&!x.newDxccEver);
 auto y=b.callerScoreInputFor("NEW",2,true,20);check(!y.workedEver&&y.newDxccEver&&y.newDxccBand);
 b.m_callerQueue={"OLD 1000 -10","NEW 1000 2"};b.rankCallerQueue("test");check(b.m_callerQueue.first().startsWith("NEW")&&b.notifications==1);
 b.m_callerQueue={"OLD 1000 -10","NEW 1000 2"};b.m_callerQueuePinned.insert("OLD");b.rankCallerQueue("test");check(b.m_callerQueue.first().startsWith("OLD"));
 b.m_callerQueuePinned.clear();b.m_callerQueueManualOrder=true;b.rankCallerQueue("test");check(b.m_callerQueue.first().startsWith("OLD"));
 QStringList rows={"0|-10|0|0|ME OLD FN42","0|2|0|0|ME NEW FN31"}; auto r=b.scoredCallerRowOrder(rows);check(r.first()==rows.last());b.m_callerScoring=false;check(b.scoredCallerRowOrder(rows)==rows);
 std::cout<<"caller scoring A/B: priority, DXCC, worked, pinned, manual and slot order passed\n"; }
'''
# A local declaration before the implementation keeps this harness independent of the bridge headers.
h=h.replace('using decodium::seq::normalizedBaseCall;', 'namespace decodium { namespace seq { QString normalizedBaseCall(QString const&); } }\nusing decodium::seq::normalizedBaseCall;')
with tempfile.TemporaryDirectory(prefix='decodium-callerscore-ab-') as t:
 d=Path(t);(d/'test.cpp').write_text(h);outs=[]
 for code in (a,b):
  (d/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),str(d/'test.cpp'),*f,'-o',str(d/'test')],check=True);outs.append(subprocess.check_output([str(d/'test')],text=True))
 if outs[0]!=outs[1]:raise SystemExit('Caller scoring A/B mismatch')
 print(outs[0],end='')

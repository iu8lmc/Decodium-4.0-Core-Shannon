#!/usr/bin/env python3
"""Compare Auto Call candidate selection and deferred retry guards."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/autocallselect_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeAutoCallSelect.cpp').read_text();mark='bool DecodiumBridge::tryAutoCallFromRows(';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QSet>
#include <QList>
#include <vector>
#include <functional>
#include <cmath>
#include <iostream>
struct FakeApp{static bool closingDown(){return false;}};
#define QCoreApplication FakeApp
std::vector<std::function<void()>> callbacks;struct FakeTimer{template<class T,class F>static void singleShot(int,T*,F f){callbacks.push_back(f);}};
#define QTimer FakeTimer
QStringList events;void bridgeLog(const QString&s){events<<s;}
struct DecodeUserFilterConfig{};QString normalizedBaseCall(QString s){return s.trimmed().toUpper();}QString normalizedUsableCallToken(QString s){return s.trimmed().toUpper();}QString freqHzToBandToken(double){return "20m";}QString workedCallBandKey(const QString&b,const QString&c){return b+":"+c;}bool shouldAcceptDecodeEntryByUserFilters(const QVariantMap&e,const DecodeUserFilterConfig&,bool,QString*){return e.value("fromCall").toString()!="FILTER";}int secondsFromUtcDisplayToken(QString s){bool ok=false;int v=s.toInt(&ok);return ok?v:-1;}int signedUtcSecondDelta(int a,int b){return b-a;}
class DecodiumBridge:public QObject{public:bool m_autoCallEnabled=true,m_targetCallActive=false,m_transmitting=false,m_tuning=false,m_txEnabled=false,m_autoCallStartingCandidate=false,m_shuttingDown=false,accept=true;int m_qsoProgress=0,m_autoCallMaxQsos=0,m_autoCallQsoCount=0,m_autoCallPriority=0,checks=0;double m_frequency=14074000;quint64 m_txRequestGeneration=1;QString m_dxCall,m_callsign="ME",m_grid="HOME",m_mode="FT8",m_autoCallCurrentCandidate;struct Worked{QSet<QString>callByBand;}m_worked;
 bool autoCallModeSupported(){return true;}DecodeUserFilterConfig readDecodeUserFilterConfig(){return {};}QStringList parseFt8Row(const QString&s){return s.split('|');}bool shouldAcceptDecodedMessage(const QString&s,QString*){return !s.contains("REJECT");}QString extractDecodedCallsign(const QString&s,bool){return s.split(' ').value(1);}bool isRecentAutoCqWorkedOrLoggedDuplicate(const QString&c,double,const QString&){return c=="RECENT";}void enrichDecodeEntry(QVariantMap&){}QString extractDecodedGrid(const QString&s){return s.split(' ').value(2);}double calcDistance(const QString&,const QString&g){return g=="FAR"?1000:(g=="NEAR"?10:-1);}void processDecodeDoubleClick(const QString&s,const QString&,const QString&,int){events<<"click:"+s;if(accept){m_dxCall=s.split(' ').value(1);m_txEnabled=true;}}void checkAndStartPeriodicTx(){checks++;}bool tryAutoCallFromRows(const QStringList&);};
#include "methods.inc"
void fire(){auto f=callbacks.front();callbacks.erase(callbacks.begin());f();}
void show(const char*n,DecodiumBridge&b,bool ok){std::cout<<n<<':'<<ok<<':'<<b.m_autoCallCurrentCandidate.toStdString()<<':'<<b.m_dxCall.toStdString()<<':'<<b.checks<<':'<<callbacks.size()<<'\n';events.clear();}
int main(){QStringList rows{"10|-10|0|1000|CQ A NEAR","12|-15|0|1100|CQ B FAR","11|-2|0|1200|CQ C NEAR","13|-1|0|1300|CQ FILTER NEAR","14|-1|0|1400|CQ RECENT NEAR"};DecodiumBridge a;bool ok=a.tryAutoCallFromRows(rows);show("latest",a,ok);fire();show("retry",a,ok);DecodiumBridge b;b.m_autoCallPriority=1;ok=b.tryAutoCallFromRows(rows);show("strong",b,ok);b.m_txRequestGeneration++;fire();show("stale",b,ok);DecodiumBridge c;c.m_autoCallPriority=2;ok=c.tryAutoCallFromRows(rows);show("far",c,ok);callbacks.clear();DecodiumBridge d;d.m_worked.callByBand.insert("20m:B");ok=d.tryAutoCallFromRows(rows);show("worked",d,ok);callbacks.clear();DecodiumBridge e;e.accept=false;ok=e.tryAutoCallFromRows(rows);show("not-armed",e,ok);DecodiumBridge f;f.m_transmitting=true;ok=f.tryAutoCallFromRows(rows);show("busy",f,ok);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-autocallselect-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1]=='latest:1:B:B:0:1\nretry:1:B:B:1:0\nstrong:1:C:C:0:1\nstale:1:C:C:0:0\nfar:1:B:B:0:1\nworked:1:C:C:0:1\nnot-armed:0:::0:0\nbusy:0:::0:0\n',out
print('Auto Call selection A/B: priorities, filters, worked station, arm and stale retry passed')

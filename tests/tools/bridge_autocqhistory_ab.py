#!/usr/bin/env python3
"""Compare recent AutoCQ cache mutation with controlled callsign/band helpers."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/autocqhistory_before.inc').read_text();new='void DecodiumBridge::clearRecentAutoCqAbandoned('+(r/'src/bridge/DecodiumBridgeAutoCqHistory.cpp').read_text().split('void DecodiumBridge::clearRecentAutoCqAbandoned(',1)[1];new=new.replace('decodiumBridgePruneDuplicateCache(', 'pruneRecentDuplicateCache(').replace('decodiumBridgeAutoCqHistoryLog(', 'bridgeLog(');assert old==new
src=r'''
#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <iostream>
namespace Radio {QString base_callsign(const QString&s){return s.section('/',0,0);}}
int prunes=0;void pruneRecentDuplicateCache(QHash<QString,QDateTime>&,const QDateTime&){prunes++;}void bridgeLog(const QString&){}
struct DecodiumBridge {QString m_mode="FT8";double m_frequency=14074000;bool m_autoCqRepeat=true,m_multiAnswerMode=false;QHash<QString,QDateTime> m_recentAutoCqAbandonedUtcByKey,m_recentAutoCqWorkedUtcByKey;QHash<QString,qint64> m_qsoCooldown;int removals=0;QString autoCqBandKeyForFrequency(double f){return f>0?"20m":"--";}void removeCallerFromQueue(const QString&){removals++;}void clearRecentAutoCqAbandoned(const QString&,double,const QString&,const QString&);void rememberRecentAutoCqAbandoned(const QString&,double,const QString&);void rememberRecentAutoCqWorked(const QString&,double,const QString&);void rememberCompletedAutoCqPartner(const QString&,bool,const QString&);};
#include "methods.inc"
int main(){DecodiumBridge b;b.rememberRecentAutoCqAbandoned(" k1abc/p ",14074000,"ft8");b.rememberRecentAutoCqWorked("W1AW",14074000,"FT4");std::cout<<b.m_recentAutoCqAbandonedUtcByKey.contains("K1ABC|20M|FT8")<<':'<<b.m_recentAutoCqWorkedUtcByKey.contains("W1AW|20M|FT4")<<':'<<prunes<<'\n';b.clearRecentAutoCqAbandoned("K1ABC/P",14074000,"","");b.rememberRecentAutoCqAbandoned("",0,"");std::cout<<b.m_recentAutoCqAbandonedUtcByKey.size()<<':'<<prunes<<'\n';b.rememberCompletedAutoCqPartner("K1ABC/P",true,"test");b.rememberCompletedAutoCqPartner("W1AW",false,"test");std::cout<<b.m_qsoCooldown.size()<<':'<<b.m_recentAutoCqWorkedUtcByKey.contains("K1ABC|20M|FT8")<<':'<<b.m_recentAutoCqAbandonedUtcByKey.contains("W1AW|20M|FT8")<<':'<<b.removals<<'\n';b.m_autoCqRepeat=false;b.m_multiAnswerMode=false;b.rememberCompletedAutoCqPartner("N0CALL",true,"test");std::cout<<b.m_qsoCooldown.size()<<':'<<b.removals<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-autocqhistory-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'methods.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1] and out[0].splitlines()==['1:1:2','0:2','2:1:1:2','3:2'],out
print('AutoCQ history A/B: worked, abandoned, clear, cooldown and queue handling passed')

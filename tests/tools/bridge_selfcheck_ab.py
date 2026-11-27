#!/usr/bin/env python3
"""Compare bridge self-check state snapshots against original."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/selfcheck_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeSelfCheck.cpp').read_text();mark='QVariantList DecodiumBridge::runSelfCheck()';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QCoreApplication>
#include <QDateTime>
#include <QThread>
#include <QJsonDocument>
#include <QJsonArray>
#include "DecodiumSelfCheck.hpp"
#include <iostream>
#define FORK_RELEASE_VERSION "1.0.737"
class DecodiumBridge{public:QString m_latestVersion,m_callsign="9H1SR",m_mode="FT8";bool m_updateAvailable=false,m_monitoring=false,enabled=true,synced=true,mam=false;double m_ntpOffsetMs=0;int m_ftThreads=1;QDateTime m_monitoringSince;QString mode(){return m_mode;}bool ntpEnabled(){return enabled;}bool ntpSynced(){return synced;}bool mamMultiStream(){return mam;}qint64 totalDecodesReceived(){return 20;}QVariantList runSelfCheck();};
#include "methods.inc"
void show(DecodiumBridge&b){auto rows=b.runSelfCheck();std::cout<<QJsonDocument(QJsonArray::fromVariantList(rows)).toJson(QJsonDocument::Compact).toStdString()<<'\n';}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;show(b);b.m_latestVersion="1.0.999";b.m_updateAvailable=true;b.enabled=false;b.m_mode="FT2";b.mam=true;show(b);b.m_updateAvailable=false;b.enabled=true;b.m_ntpOffsetMs=1500;show(b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-selfcheck-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I',str(r/'src/app'),str(p/'main.cpp'),str(r/'src/app/DecodiumSelfCheck.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1],outputs
assert 'outdated' in outputs[0] and 'synchronisation' in outputs[0] and len(outputs[0].splitlines())==3
print('Bridge self-check A/B: normal, outdated, unsynchronised and offset states passed')

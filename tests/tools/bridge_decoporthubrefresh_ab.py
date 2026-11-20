#!/usr/bin/env python3
"""Compare asynchronous hub refresh and single-flight behaviour."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/decoporthubrefresh_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeDecoPortHubRefresh.cpp').read_text();mark='void DecodiumBridge::refreshDecoPortHub()';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QCoreApplication>
#include <QEventLoop>
#include <QPointer>
#include <QTimer>
#include <QThread>
#include <QVariantList>
#include <QtConcurrent/QtConcurrentRun>
#include <atomic>
#include <iostream>
struct Cat{QString serialPort(){return "  test-port ";}};struct Share{int port(){return 4555;}};std::atomic<int> calls{0};QVariantList buildDecoPortHubRows(QString const&cat,int port){++calls;QThread::msleep(150);return {cat,port};}
class DecodiumBridge:public QObject{public:Cat*m_hamlibCat=nullptr;Share*m_catShare=nullptr;bool m_decoPortHubBusy=false;QVariantList m_decoPortHub;int notifications=0;void decoPortHubChanged(){notifications++;}void refreshDecoPortHub();};
#include "methods.inc"
void wait(){QEventLoop l;QTimer::singleShot(450,&l,&QEventLoop::quit);l.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);Cat cat;Share share;DecodiumBridge b;b.m_hamlibCat=&cat;b.m_catShare=&share;b.refreshDecoPortHub();b.refreshDecoPortHub();std::cout<<"busy:"<<b.m_decoPortHubBusy<<'\n';wait();std::cout<<"result:"<<calls<<':'<<b.m_decoPortHubBusy<<':'<<b.notifications<<':'<<b.m_decoPortHub[0].toString().toStdString()<<':'<<b.m_decoPortHub[1].toInt()<<'\n';b.m_hamlibCat=nullptr;b.m_catShare=nullptr;b.refreshDecoPortHub();wait();std::cout<<"defaults:"<<calls<<':'<<b.notifications<<':'<<b.m_decoPortHub[0].toString().isEmpty()<<':'<<b.m_decoPortHub[1].toInt()<<'\n';auto*dead=new DecodiumBridge;dead->refreshDecoPortHub();delete dead;wait();std::cout<<"destroyed:"<<calls<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Concurrent'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-hubrefresh-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='busy:1\nresult:1:0:1:TEST-PORT:4555\ndefaults:2:2:1:4533\ndestroyed:3\n',outputs
print('DecoPort hub A/B: asynchronous result, coalescing, defaults and destroyed receiver passed')

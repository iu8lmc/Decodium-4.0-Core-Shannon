#!/usr/bin/env python3
"""Exercise the Linux branch with mocked Pulse/PipeWire operations."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/tx_output_resolve_before.inc').read_text()
module=(root/'src/bridge/DecodiumBridgeTxOutputResolve.cpp').read_text();assert module.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Multimedia'],text=True))
harness=r'''
#include <QCoreApplication>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QDebug>
#include <QTextStream>
#include <QString>
#include <QList>
static bool sinkSuccess=true;static int calls=0;
static QString linuxPulseSinkNameFromDisplay(QString s){return s.startsWith("Pulse: ")?s.mid(7):QString();}
static bool linuxSetDefaultPulseSink(const QString&,QString* e){++calls;if(!sinkSuccess)*e="synthetic error";return sinkSuccess;}
static void bridgeLog(const QString&){}
static bool decodiumBridgeResolveAudioDeviceByIdentity(const QList<QAudioDevice>&,const QString&,const QString&,QAudioDevice*,QString* reason){*reason="not found";return false;}
static QString decodiumBridgeAudioDeviceIdSettingForLog(QString s){return s;}
static QString decodiumBridgeAudioDeviceIdForSettings(const QAudioDevice&){return {};}
#define Q_OS_LINUX 1
#include "helpers.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QList<QAudioDevice> devices;QAudioDevice fallback;bool found=false;QString reason;findOutputDevice("Pulse: dummy","",devices,fallback,&found,&reason);bool pulse=found&&reason=="linux-pulse-sink"&&calls==1;sinkSuccess=false;findOutputDevice("Pulse: fail","",devices,fallback,&found,&reason);bool failed=!found&&reason=="not found"&&calls==2;findOutputDevice("","",devices,fallback,&found,&reason);bool def=found&&reason=="default";QTextStream(stdout)<<"pulse="<<pulse<<" failed="<<failed<<" default="<<def<<Qt::endl;return pulse&&failed&&def?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-output-linux-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outputs=[]
 for label,body in [('before',old),('after',module[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outputs.append(result.stdout)
 assert outputs[0]==outputs[1]

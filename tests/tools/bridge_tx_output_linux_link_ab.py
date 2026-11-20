#!/usr/bin/env python3
"""Link and exercise the actual extracted Linux branch with stubbed Pulse operations."""
from pathlib import Path
import shlex, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/tx_output_resolve_before.inc').read_text()
module=(root/'src/bridge/DecodiumBridgeTxOutputResolve.cpp').read_text()
assert module.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Multimedia'],text=True))
prefix=r'''
#include <QCoreApplication>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QDebug>
#include <QTextStream>
#include <QString>
#include <QList>
static bool sinkSuccess=true;static int calls=0;
QString decodiumBridgeLinuxPulseSinkNameFromDisplay(QString s){return s.startsWith("Pulse: ")?s.mid(7):QString();}
bool decodiumBridgeLinuxSetDefaultPulseSink(const QString&,QString* e){++calls;if(!sinkSuccess)*e="synthetic error";return sinkSuccess;}
void decodiumBridgeTxOutputResolveLog(const QString&){}
bool decodiumBridgeResolveAudioDeviceByIdentity(const QList<QAudioDevice>&,const QString&,const QString&,QAudioDevice*,QString* reason){*reason="not found";return false;}
QString decodiumBridgeAudioDeviceIdSettingForLog(QString s){return s;}
QString decodiumBridgeAudioDeviceIdForSettings(const QAudioDevice&){return {};}
'''
old_alias='''#define bridgeLog decodiumBridgeTxOutputResolveLog\n#define linuxPulseSinkNameFromDisplay decodiumBridgeLinuxPulseSinkNameFromDisplay\n#define linuxSetDefaultPulseSink decodiumBridgeLinuxSetDefaultPulseSink\n'''
test=r'''
int main(int argc,char**argv){QCoreApplication app(argc,argv);QList<QAudioDevice> devices;QAudioDevice fallback;bool found=false;QString reason;decodiumBridgeFindTxOutputDevice("Pulse: dummy","",devices,fallback,&found,&reason);bool pulse=found&&reason=="linux-pulse-sink"&&calls==1;sinkSuccess=false;decodiumBridgeFindTxOutputDevice("Pulse: fail","",devices,fallback,&found,&reason);bool failed=!found&&reason=="not found"&&calls==2;decodiumBridgeFindTxOutputDevice("","",devices,fallback,&found,&reason);bool def=found&&reason=="default";QTextStream(stdout)<<"pulse="<<pulse<<" failed="<<failed<<" default="<<def<<Qt::endl;return pulse&&failed&&def?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-output-linux-link-') as tmp:
 d=Path(tmp);outs=[]
 for label,body in [('before',old_alias+old),('after',module)]:
  # Enable only the branch in the subject source, after Qt detects the host platform.
  body=body.replace('#ifdef Q_OS_LINUX','#if 1')
  source=d/(label+'.cpp');source.write_text(prefix+body+test);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(root/'src/bridge'),str(source),*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outs.append(result.stdout)
 assert outs[0]==outs[1]

#!/usr/bin/env python3
"""Compare asynchronous TX WAV output with the original implementation."""
import os,shlex,subprocess,tempfile,wave
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/txrecording_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeTxRecording.cpp').read_text();mark='void DecodiumBridge::saveTxRecordingAsync(';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeTxRecordingLog(', 'bridgeLog(');assert old==new
source=r'''
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QDir>
#include <QFileInfo>
bool decodiumBridgeEnsureParentDirectoryForFile(QString const& path, QString*){return QDir().mkpath(QFileInfo(path).absolutePath());}
#include "DecodiumBridgeWavIO.h"
void bridgeLog(const QString&){}
class DecodiumBridge:public QObject{public:void saveTxRecordingAsync(const QString&,QVector<float>,int,const QString&);};
#include "methods.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;b.saveTxRecordingAsync(QString::fromLocal8Bit(argv[1]),{0.0f,.5f,-.5f,1.0f},12000,"lab");b.saveTxRecordingAsync(QString::fromLocal8Bit(argv[2]),{},12000,"lab");b.saveTxRecordingAsync(QString::fromLocal8Bit(argv[3]),{.5f},0,"lab");QEventLoop l;QTimer::singleShot(500,&l,&QEventLoop::quit);l.exec();}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-txrecording-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for idx,body in enumerate((old,new)):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I',str(r/'src/bridge'),str(p/'main.cpp'),str(r/'src/bridge/DecodiumBridgeWavIO.cpp'),*flags,'-o',str(p/'test')],check=True);out=p/f'{idx}.wav';bad1=p/f'{idx}-empty.wav';bad2=p/f'{idx}-rate.wav';subprocess.run([str(p/'test'),str(out),str(bad1),str(bad2)],check=True);assert out.exists() and not bad1.exists() and not bad2.exists();outputs.append(out.read_bytes())
 assert outputs[0]==outputs[1]
 with wave.open(str(out),'rb') as w:assert (w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes())==(1,2,12000,4)
print('TX recording A/B: identical PCM WAV, invalid input rejection and asynchronous completion passed')

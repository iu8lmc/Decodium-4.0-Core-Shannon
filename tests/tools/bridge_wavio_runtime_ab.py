#!/usr/bin/env python3
"""Exercise both WAV writers on boundary samples and compare actual bytes."""
import os,shlex,struct,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2]
old=(r/'tests/bridge_refactor_fixtures/wavio_before.inc').read_text()
new='bool writeMono16WavFile('+(r/'src/bridge/DecodiumBridgeWavIO.cpp').read_text().split('bool writeMono16WavFile(',1)[1]
new=new.replace('decodiumBridgeEnsureParentDirectoryForFile(', 'ensureParentDirectoryForFile(')
src=r'''
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QVector>
#include <iostream>
bool ensureParentDirectoryForFile(QString const& p,QString*){return QDir().mkpath(QFileInfo(p).absolutePath());}
#include "method.inc"
int main(int argc,char**argv){if(argc!=2)return 2;QString base=QString::fromUtf8(argv[1]);QVector<float> f{-2.0f,-1.0f,-0.5f,0.0f,0.5f,1.0f,2.0f};QVector<short> s{-32768,-1,0,1,32767};std::cout<<writeMono16WavFile(base+"/nested/float.wav",f,48000)<<writeMono16WavFile(base+"/nested/short.wav",s,12000)<<writeMono16WavFile(base+"/empty.wav",QVector<float>{},48000)<<writeMono16WavFile(base+"/rate.wav",s,0)<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));data=[]
with tempfile.TemporaryDirectory(prefix='decodium-wavio-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(src)
 for i,code in enumerate((old,new)):
  (p/'method.inc').write_text(code);exe=p/'test';subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(exe)],check=True);d=p/str(i);out=subprocess.check_output([str(exe),str(d)],text=True);assert out=='1100\n';data.append(((d/'nested/float.wav').read_bytes(),(d/'nested/short.wav').read_bytes()));assert not (d/'empty.wav').exists() and not (d/'rate.wav').exists()
if data[0]!=data[1]:raise SystemExit('WAV bytes differ')
f,s=data[0];assert struct.unpack('<7h',f[44:])==(-32767,-32767,-16384,0,16384,32767,32767) and struct.unpack('<5h',s[44:])==(-32768,-1,0,1,32767)
print('WAV writers A/B OK: float clipping/rounding, int16 boundaries, invalid inputs and nested paths')
